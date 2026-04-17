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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils/assert_helpers.h"
#include "zenoh-pico.h"
#include "zenoh-pico/api/macros.h"
#include "zenoh-pico/api/primitives.h"

static void test_open_timeout_single_locator(void) {
    z_owned_config_t c;
    z_config_default(&c);

    zp_config_insert(z_loan_mut(c), Z_CONFIG_MODE_KEY, "peer");
    zp_config_insert(z_loan_mut(c), Z_CONFIG_CONNECT_KEY, "tcp/127.0.0.1:18002");
    zp_config_insert(z_loan_mut(c), Z_CONFIG_CONNECT_TIMEOUT_KEY, "1000");

    z_owned_session_t s;

    z_clock_t start = z_clock_now();
    z_result_t ret = z_open(&s, z_move(c), NULL);
    unsigned long elapsed_ms = z_clock_elapsed_ms(&start);

    ASSERT_ERR(ret, _Z_ERR_TRANSPORT_OPEN_FAILED);
    ASSERT_TRUE(elapsed_ms >= 1000);
}

#if Z_FEATURE_UNICAST_PEER == 1
static void _test_open_timeout_partial_connectivity(const char *connect_exit_on_failure, z_result_t expected_ret) {
    z_owned_config_t c1;
    z_owned_config_t c2;

    z_config_default(&c1);
    z_config_default(&c2);

    // Listener peer
    zp_config_insert(z_loan_mut(c1), Z_CONFIG_MODE_KEY, "peer");
    zp_config_insert(z_loan_mut(c1), Z_CONFIG_LISTEN_KEY, "tcp/127.0.0.1:18001");

    // Connecting peer: one good endpoint, one bad endpoint
    zp_config_insert(z_loan_mut(c2), Z_CONFIG_MODE_KEY, "peer");
    zp_config_insert(z_loan_mut(c2), Z_CONFIG_CONNECT_KEY, "tcp/127.0.0.1:18001");
    zp_config_insert(z_loan_mut(c2), Z_CONFIG_CONNECT_KEY, "tcp/127.0.0.1:18002");
    zp_config_insert(z_loan_mut(c2), Z_CONFIG_CONNECT_TIMEOUT_KEY, "1000");
    zp_config_insert(z_loan_mut(c2), Z_CONFIG_CONNECT_EXIT_ON_FAILURE_KEY, connect_exit_on_failure);

    z_owned_session_t s1;
    z_owned_session_t s2;

    ASSERT_OK(z_open(&s1, z_move(c1), NULL));
    z_sleep_s(1);  // Ensure the listener is fully up before connecting

    z_clock_t start = z_clock_now();
    z_result_t ret = z_open(&s2, z_move(c2), NULL);
    unsigned long elapsed_ms = z_clock_elapsed_ms(&start);

    ASSERT_ERR(ret, expected_ret);

    if (expected_ret == _Z_ERR_TRANSPORT_OPEN_FAILED) {
        ASSERT_TRUE(elapsed_ms >= 1000);
    }

    z_drop(z_move(s2));
    z_drop(z_move(s1));
}

static void test_open_timeout_partial_connectivity_exit_on_failure_false(void) {
    _test_open_timeout_partial_connectivity("false", _Z_RES_OK);
}

static void test_open_timeout_partial_connectivity_exit_on_failure_true(void) {
    _test_open_timeout_partial_connectivity("true", _Z_ERR_TRANSPORT_OPEN_PARTIAL_CONNECTIVITY);
}
#endif

#if Z_FEATURE_MULTI_THREAD == 1
typedef struct {
    z_owned_config_t config;
    z_owned_session_t session;
    uint32_t delay_ms;
    z_result_t ret;
} delayed_open_arg_t;

static void *delayed_open_task(void *arg) {
    delayed_open_arg_t *ctx = (delayed_open_arg_t *)arg;

    z_sleep_ms(ctx->delay_ms);
    ctx->ret = z_open(&ctx->session, z_move(ctx->config), NULL);

    return NULL;
}

static void test_open_late_joining_endpoint(void) {
    z_owned_config_t c1;
    z_owned_config_t c2;

    z_config_default(&c1);
    z_config_default(&c2);

    // Late listener peer
    zp_config_insert(z_loan_mut(c1), Z_CONFIG_MODE_KEY, "peer");
    zp_config_insert(z_loan_mut(c1), Z_CONFIG_LISTEN_KEY, "tcp/127.0.0.1:18001");

    // Connecting peer
    zp_config_insert(z_loan_mut(c2), Z_CONFIG_MODE_KEY, "peer");
    zp_config_insert(z_loan_mut(c2), Z_CONFIG_CONNECT_KEY, "tcp/127.0.0.1:18001");
    zp_config_insert(z_loan_mut(c2), Z_CONFIG_CONNECT_TIMEOUT_KEY, "5000");

    delayed_open_arg_t ctx;
    ctx.config = c1;
    ctx.delay_ms = 1000;
    ctx.ret = _Z_ERR_GENERIC;

    _z_task_t task;
    ASSERT_OK(_z_task_init(&task, NULL, delayed_open_task, &ctx));

    z_owned_session_t s2;
    z_clock_t start = z_clock_now();
    z_result_t ret = z_open(&s2, z_move(c2), NULL);
    unsigned long elapsed_ms = z_clock_elapsed_ms(&start);

    ASSERT_OK(ret);
    ASSERT_TRUE(elapsed_ms >= 1000);

    ASSERT_OK(_z_task_join(&task));
    ASSERT_OK(ctx.ret);

    z_drop(z_move(s2));
    z_drop(z_move(ctx.session));
}
#endif

int main(void) {
    test_open_timeout_single_locator();
#if Z_FEATURE_UNICAST_PEER == 1
    test_open_timeout_partial_connectivity_exit_on_failure_false();
    test_open_timeout_partial_connectivity_exit_on_failure_true();
#endif
#if Z_FEATURE_MULTI_THREAD == 1
    test_open_late_joining_endpoint();
#endif
    return 0;
}
