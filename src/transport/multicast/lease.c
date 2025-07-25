//
// Copyright (c) 2022 ZettaScale Technology
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Apache License, Version 2.0
// which is available at https://www.apache.org/licenses/LICENSE-2.0.
//
// SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
//
// Contributors:
//   ZettaScale Zenoh Team, <zenoh@zettascale.tech>
//

#include "zenoh-pico/transport/common/lease.h"

#include <stddef.h>

#include "zenoh-pico/config.h"
#include "zenoh-pico/session/interest.h"
#include "zenoh-pico/session/query.h"
#include "zenoh-pico/session/utils.h"
#include "zenoh-pico/transport/multicast/lease.h"
#include "zenoh-pico/utils/logging.h"
#include "zenoh-pico/utils/result.h"

#if Z_FEATURE_MULTICAST_TRANSPORT == 1 || Z_FEATURE_RAWETH_TRANSPORT == 1

z_result_t _zp_multicast_send_join(_z_transport_multicast_t *ztm) {
    _z_conduit_sn_list_t next_sn;
    next_sn._is_qos = false;
    next_sn._val._plain._best_effort = ztm->_common._sn_tx_best_effort;
    next_sn._val._plain._reliable = ztm->_common._sn_tx_reliable;

    _z_id_t zid = _Z_RC_IN_VAL(ztm->_common._session)->_local_zid;
    _z_transport_message_t jsm = _z_t_msg_make_join(Z_WHATAMI_PEER, Z_TRANSPORT_LEASE, zid, next_sn);

    return ztm->_send_f(&ztm->_common, &jsm);
}

z_result_t _zp_multicast_send_keep_alive(_z_transport_multicast_t *ztm) {
    _z_transport_message_t t_msg = _z_t_msg_make_keep_alive();
    return ztm->_send_f(&ztm->_common, &t_msg);
}

#else
z_result_t _zp_multicast_send_join(_z_transport_multicast_t *ztm) {
    _ZP_UNUSED(ztm);
    return _Z_ERR_TRANSPORT_NOT_AVAILABLE;
}

z_result_t _zp_multicast_send_keep_alive(_z_transport_multicast_t *ztm) {
    _ZP_UNUSED(ztm);
    return _Z_ERR_TRANSPORT_NOT_AVAILABLE;
}
#endif  // Z_FEATURE_MULTICAST_TRANSPORT == 1 || Z_FEATURE_RAWETH_TRANSPORT == 1

#if Z_FEATURE_MULTI_THREAD == 1 && (Z_FEATURE_MULTICAST_TRANSPORT == 1 || Z_FEATURE_RAWETH_TRANSPORT == 1)

static void _zp_multicast_failed(_z_transport_multicast_t *ztm) {
    _ZP_UNUSED(ztm);

#if Z_FEATURE_LIVELINESS == 1 && Z_FEATURE_SUBSCRIPTION == 1
    _z_liveliness_subscription_undeclare_all(_Z_RC_IN_VAL(ztm->_common._session));
#endif

#if Z_FEATURE_AUTO_RECONNECT == 1
    _z_reopen(ztm->_common._session);
#endif
}

static _z_zint_t _z_get_minimum_lease(_z_transport_peer_multicast_slist_t *peers, _z_zint_t local_lease) {
    _z_zint_t ret = local_lease;

    _z_transport_peer_multicast_slist_t *it = peers;
    while (it != NULL) {
        _z_transport_peer_multicast_t *val = _z_transport_peer_multicast_slist_value(it);
        _z_zint_t lease = val->_lease;
        if (lease < ret) {
            ret = lease;
        }

        it = _z_transport_peer_multicast_slist_next(it);
    }

    return ret;
}

static _z_zint_t _z_get_next_lease(_z_transport_peer_multicast_slist_t *peers) {
    _z_zint_t ret = SIZE_MAX;

    _z_transport_peer_multicast_slist_t *it = peers;
    while (it != NULL) {
        _z_transport_peer_multicast_t *val = _z_transport_peer_multicast_slist_value(it);
        _z_zint_t next_lease = val->_next_lease;
        if (next_lease < ret) {
            ret = next_lease;
        }

        it = _z_transport_peer_multicast_slist_next(it);
    }

    return ret;
}

void *_zp_multicast_lease_task(void *ztm_arg) {
    _z_transport_multicast_t *ztm = (_z_transport_multicast_t *)ztm_arg;
    ztm->_common._transmitted = false;

    // From all peers, get the next lease time (minimum)
    int next_lease = (int)_z_get_minimum_lease(ztm->_peers, ztm->_common._lease);
    int next_keep_alive = (int)(next_lease / Z_TRANSPORT_LEASE_EXPIRE_FACTOR);
    int next_join = Z_JOIN_INTERVAL;

    while (ztm->_common._lease_task_running) {
        if (next_lease <= 0) {
            _z_transport_peer_multicast_slist_t *prev = NULL;
            _z_transport_peer_multicast_slist_t *prev_drop = NULL;
            _z_transport_peer_mutex_lock(&ztm->_common);
            _z_transport_peer_multicast_slist_t *curr_list = ztm->_peers;
            while (curr_list != NULL) {
                bool drop_peer = false;
                _z_transport_peer_multicast_t *curr_peer = _z_transport_peer_multicast_slist_value(curr_list);
                if (curr_peer->common._received) {
                    // Reset the lease parameters
                    curr_peer->common._received = false;
                    curr_peer->_next_lease = curr_peer->_lease;
                } else {
                    _Z_INFO("Deleting peer because it has expired after %zums", curr_peer->_lease);
                    drop_peer = true;
                    prev_drop = prev;
                }
                // Update previous only if current node is not dropped
                if (!drop_peer) {
                    prev = curr_list;
                }
                // Progress list
                curr_list = _z_transport_peer_multicast_slist_next(curr_list);
                // Drop if needed
                if (drop_peer) {
                    _z_subscription_cache_invalidate(_Z_RC_IN_VAL(ztm->_common._session));
                    _z_queryable_cache_invalidate(_Z_RC_IN_VAL(ztm->_common._session));
                    _z_interest_peer_disconnected(_Z_RC_IN_VAL(ztm->_common._session), &curr_peer->common);
                    ztm->_peers = _z_transport_peer_multicast_slist_drop_element(ztm->_peers, prev_drop);
                }
            }
            _z_transport_peer_mutex_unlock(&ztm->_common);
        }

        if (next_join <= 0) {
            _zp_multicast_send_join(ztm);
            ztm->_common._transmitted = true;

            // Reset the join parameters
            next_join = Z_JOIN_INTERVAL;
        }

        if (next_keep_alive <= 0) {
            // Check if need to send a keep alive
            if (ztm->_common._transmitted == false) {
                if (_zp_multicast_send_keep_alive(ztm) < 0) {
                    _Z_INFO("Send keep alive failed.");
                    _zp_multicast_failed(ztm);
                }
            }
            // Reset the keep alive parameters
            ztm->_common._transmitted = false;
            next_keep_alive =
                (int)(_z_get_minimum_lease(ztm->_peers, ztm->_common._lease) / Z_TRANSPORT_LEASE_EXPIRE_FACTOR);
        }

        // Query timeout process
        _z_pending_query_process_timeout(_Z_RC_IN_VAL(ztm->_common._session));

        // Compute the target interval to sleep
        int interval;
        if (next_lease > 0) {
            interval = next_lease;
            if (next_keep_alive < interval) {
                interval = next_keep_alive;
            }
            if (next_join < interval) {
                interval = next_join;
            }
        } else {
            interval = next_keep_alive;
            if (next_join < interval) {
                interval = next_join;
            }
        }

        // The keep alive and lease intervals are expressed in milliseconds
        z_sleep_ms((size_t)interval);

        // Decrement all intervals
        _z_transport_peer_mutex_lock(&ztm->_common);
        _z_transport_peer_multicast_slist_t *curr_list = ztm->_peers;
        while (curr_list != NULL) {
            _z_transport_peer_multicast_t *entry = _z_transport_peer_multicast_slist_value(curr_list);
            int entry_next_lease = (int)entry->_next_lease - interval;
            if (entry_next_lease >= 0) {
                entry->_next_lease = (size_t)entry_next_lease;
            } else {
                _Z_ERROR("Negative next lease value");
                entry->_next_lease = 0;
            }
            curr_list = _z_transport_peer_multicast_slist_next(curr_list);
        }
        next_lease = (int)_z_get_next_lease(ztm->_peers);
        _z_transport_peer_mutex_unlock(&ztm->_common);
        next_keep_alive = next_keep_alive - interval;
        next_join = next_join - interval;
    }
    return 0;
}

z_result_t _zp_multicast_start_lease_task(_z_transport_multicast_t *ztm, z_task_attr_t *attr, _z_task_t *task) {
    // Init memory
    (void)memset(task, 0, sizeof(_z_task_t));
    ztm->_common._lease_task_running = true;  // Init before z_task_init for concurrency issue
    // Init task
    if (_z_task_init(task, attr, _zp_multicast_lease_task, ztm) != _Z_RES_OK) {
        ztm->_common._lease_task_running = false;
        return _Z_ERR_SYSTEM_TASK_FAILED;
    }
    // Attach task
    ztm->_common._lease_task = task;
    return _Z_RES_OK;
}

z_result_t _zp_multicast_stop_lease_task(_z_transport_multicast_t *ztm) {
    ztm->_common._lease_task_running = false;
    return _Z_RES_OK;
}
#else

void *_zp_multicast_lease_task(void *ztm_arg) {
    _ZP_UNUSED(ztm_arg);
    return NULL;
}

z_result_t _zp_multicast_start_lease_task(_z_transport_multicast_t *ztm, void *attr, void *task) {
    _ZP_UNUSED(ztm);
    _ZP_UNUSED(attr);
    _ZP_UNUSED(task);
    return _Z_ERR_TRANSPORT_NOT_AVAILABLE;
}

z_result_t _zp_multicast_stop_lease_task(_z_transport_multicast_t *ztm) {
    _ZP_UNUSED(ztm);
    return _Z_ERR_TRANSPORT_NOT_AVAILABLE;
}
#endif  // Z_FEATURE_MULTI_THREAD == 1 && (Z_FEATURE_MULTICAST_TRANSPORT == 1 || Z_FEATURE_RAWETH_TRANSPORT == 1)
