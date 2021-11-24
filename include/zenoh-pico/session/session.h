/*
 * Copyright (c) 2017, 2021 ADLINK Technology Inc.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Apache License, Version 2.0
 * which is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *
 * Contributors:
 *   ADLINK zenoh team, <zenoh@adlink-labs.tech>
 */

#ifndef ZENOH_PICO_SESSION_TYPES_H
#define ZENOH_PICO_SESSION_TYPES_H

#include <stdint.h>
#include <string.h>
#include "zenoh-pico/collections/list.h"
#include "zenoh-pico/collections/string.h"
#include "zenoh-pico/link/link.h"
#include "zenoh-pico/protocol/core.h"
#include "zenoh-pico/transport/transport.h"
#include "zenoh-pico/transport/manager.h"
#include "zenoh-pico/system/platform.h"

/**
 * A zenoh-net session.
 */
typedef struct
{
    z_mutex_t mutex_inner;

    // Session counters // FIXME: move to transport check
    z_zint_t resource_id;
    z_zint_t entity_id;
    z_zint_t pull_id;
    z_zint_t query_id;

    // Session declarations
    z_list_t *local_resources;
    z_list_t *remote_resources;

    // Session subscriptions
    z_list_t *local_subscriptions;
    z_list_t *remote_subscriptions;
    zn_int_list_map_t rem_res_loc_sub_map;

    // Session queryables
    z_list_t *local_queryables;
    zn_int_list_map_t rem_res_loc_qle_map;

    z_list_t *pending_queries;

    // Session transport.
    // Zenoh-pico is considering a single transport per session.
    _zn_transport_t *tp;
    _zn_transport_manager_t *tp_manager;
} zn_session_t;

/**
 * Return type when declaring a publisher.
 */
typedef struct
{
    zn_session_t *zn;
    z_zint_t id;
    zn_reskey_t key;
} zn_publisher_t;

/**
 * Return type when declaring a subscriber.
 */
typedef struct
{
    zn_session_t *zn;
    z_zint_t id;
} zn_subscriber_t;

/**
 * Return type when declaring a queryable.
 */
typedef struct
{
    zn_session_t *zn;
    z_zint_t id;
} zn_queryable_t;

/**
 * The query to be answered by a queryable.
 */
typedef struct
{
    zn_session_t *zn;
    z_zint_t qid;
    unsigned int kind;
    z_str_t rname;
    z_str_t predicate;
} zn_query_t;

/**
 * The possible values of :c:member:`zn_reply_t.tag`
 *
 *     - **zn_reply_t_Tag_DATA**: The reply contains some data.
 *     - **zn_reply_t_Tag_FINAL**: The reply does not contain any data and indicates that there will be no more replies for this query.
 */
typedef enum zn_reply_t_Tag
{
    zn_reply_t_Tag_DATA,
    zn_reply_t_Tag_FINAL,
} zn_reply_t_Tag;

/**
 * An reply to a :c:func:`zn_query` (or :c:func:`zn_query_collect`).
 *
 * Members:
 *   zn_sample_t data: a :c:type:`zn_sample_t` containing the key and value of the reply.
 *   unsigned int replier_kind: The kind of the replier that sent this reply.
 *   z_bytes_t replier_id: The id of the replier that sent this reply.
 *
 */
typedef struct
{
    zn_sample_t data;
    unsigned int replier_kind;
    z_bytes_t replier_id;
} zn_reply_data_t;

/**
 * An reply to a :c:func:`zn_query`.
 *
 * Members:
 *   zn_reply_t_Tag tag: Indicates if the reply contains data or if it's a FINAL reply.
 *   zn_reply_data_t data: The reply data if :c:member:`zn_reply_t.tag` equals :c:member:`zn_reply_t_Tag.zn_reply_t_Tag_DATA`.
 *
 */
typedef struct
{
    zn_reply_t_Tag tag;
    zn_reply_data_t data;
} zn_reply_t;

/**
 * An array of :c:type:`zn_reply_data_t`.
 * Result of :c:func:`zn_query_collect`.
 *
 * Members:
 *   char *const *val: A pointer to the array.
 *   unsigned int len: The length of the array.
 *
 */
typedef struct
{
    const zn_reply_data_t *val;
    size_t len;
} zn_reply_data_array_t;

/**
 * The callback signature of the functions handling data messages.
 */
typedef void (*zn_data_handler_t)(const zn_sample_t *sample, const void *arg);
/**
 * The callback signature of the functions handling query replies.
 */
typedef void (*zn_query_handler_t)(zn_reply_t reply, const void *arg);
/**
 * The callback signature of the functions handling query messages.
 */
typedef void (*zn_queryable_handler_t)(zn_query_t *query, const void *arg);

/* private */
#define _ZN_IS_REMOTE 0
#define _ZN_IS_LOCAL 1

typedef struct
{
    z_zint_t id;
    zn_reskey_t key;
} _zn_resource_t;

typedef struct
{
    z_zint_t id;
    zn_reskey_t key;
    zn_subinfo_t info;
    zn_data_handler_t callback;
    void *arg;
} _zn_subscriber_t;

typedef struct
{
    z_zint_t id;
    zn_reskey_t key;
} _zn_publisher_t;

typedef struct
{
    zn_reply_t reply;
    z_timestamp_t tstamp;
} _zn_pending_reply_t;

typedef struct
{
    z_zint_t id;
    zn_reskey_t key;
    z_str_t predicate;
    zn_query_target_t target;
    zn_query_consolidation_t consolidation;
    z_list_t *pending_replies;
    zn_query_handler_t callback;
    void *arg;
} _zn_pending_query_t;

typedef struct
{
    z_mutex_t mutex;
    z_condvar_t cond_var;
    z_vec_t replies;
} _zn_pending_query_collect_t;

typedef struct
{
    z_zint_t id;
    zn_reskey_t key;
    unsigned int kind;
    zn_queryable_handler_t callback;
    void *arg;
} _zn_queryable_t;

#endif /* ZENOH_PICO_SESSION_TYPES_H */
