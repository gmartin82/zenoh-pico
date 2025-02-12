//
// Copyright (c) 2025 ZettaScale Technology
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

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "zenoh-pico/utils/pointers.h"
#include "zenoh-pico/utils/query_params.h"
#include "zenoh-pico/utils/time_range.h"

#undef NDEBUG
#include <assert.h>

void test_query_params(void) {
#define TEST_PARAMS(str, expected, n)                                 \
    {                                                                 \
        _z_str_se_t params = _z_bstrnew(str);                         \
                                                                      \
        for (int i = 0; i <= n; i++) {                                \
            _z_query_param_t param = _z_query_params_next(&params);   \
            if (i < n) {                                              \
                assert(param.key.start == expected[i].key.start);     \
                assert(param.key.end == expected[i].key.end);         \
                assert(param.value.start == expected[i].value.start); \
                assert(param.value.end == expected[i].value.end);     \
            } else {                                                  \
                assert(param.key.start == NULL);                      \
                assert(param.key.end == NULL);                        \
                assert(param.value.start == NULL);                    \
                assert(param.value.end == NULL);                      \
            }                                                         \
        }                                                             \
        assert(params.start == NULL);                                 \
        assert(params.end == NULL);                                   \
    }

    const char *params1 = "";
    const _z_query_param_t params1_expected[0];
    TEST_PARAMS(params1, params1_expected, 0);

    const char *params2 = "a=1";
    const _z_query_param_t params2_expected[] = {{
        .key.start = params2,
        .key.end = _z_cptr_char_offset(params2, 1),
        .value.start = _z_cptr_char_offset(params2, 2),
        .value.end = _z_cptr_char_offset(params2, 3),
    }};
    TEST_PARAMS(params2, params2_expected, 1);

    const char *params3 = "a=1;bee=string";
    const _z_query_param_t params3_expected[] = {{
                                                     .key.start = params3,
                                                     .key.end = _z_cptr_char_offset(params3, 1),
                                                     .value.start = _z_cptr_char_offset(params3, 2),
                                                     .value.end = _z_cptr_char_offset(params3, 3),
                                                 },
                                                 {
                                                     .key.start = _z_cptr_char_offset(params3, 4),
                                                     .key.end = _z_cptr_char_offset(params3, 7),
                                                     .value.start = _z_cptr_char_offset(params3, 8),
                                                     .value.end = _z_cptr_char_offset(params3, 14),
                                                 }};
    TEST_PARAMS(params3, params3_expected, 2);

    const char *params4 = ";";
    const _z_query_param_t params4_expected[] = {{
        .key.start = NULL,
        .key.end = NULL,
        .value.start = NULL,
        .value.end = NULL,
    }};
    TEST_PARAMS(params4, params4_expected, 1);

    const char *params5 = "a";
    const _z_query_param_t params5_expected[] = {{
        .key.start = params5,
        .key.end = _z_cptr_char_offset(params5, 1),
        .value.start = NULL,
        .value.end = NULL,
    }};
    TEST_PARAMS(params5, params5_expected, 1);

    const char *params6 = "a=";
    const _z_query_param_t params6_expected[] = {{
        .key.start = params6,
        .key.end = _z_cptr_char_offset(params6, 1),
        .value.start = NULL,
        .value.end = NULL,
    }};
    TEST_PARAMS(params6, params6_expected, 1);
}

bool compare_double_result(const double expected, const double result) {
    static const double EPSILON = 1e-6;
    return (result - expected) < EPSILON;
}

void test_time_range(void) {
    _z_time_range_t result;
    const char *range = "";

    // Range tests
    range = "[..]";
    assert(_z_time_range_from_str(range, strlen(range), &result) == true);
    assert(result.start.bound == _Z_TIME_BOUND_UNBOUNDED);
    assert(result.end.bound == _Z_TIME_BOUND_UNBOUNDED);

    range = "[now()..now(5)]";
    assert(_z_time_range_from_str(range, strlen(range), &result) == true);
    assert(result.start.bound == _Z_TIME_BOUND_INCLUSIVE);
    assert(compare_double_result(0.0, result.start.now_offset));
    assert(result.end.bound == _Z_TIME_BOUND_EXCLUSIVE);
    assert(compare_double_result(5.0, result.end.now_offset));

    range = "[now(-999.9u)..now(100.5ms)]";
    assert(_z_time_range_from_str(range, strlen(range), &result) == true);
    assert(result.start.bound == _Z_TIME_BOUND_INCLUSIVE);
    assert(compare_double_result(-0.0009999, result.start.now_offset));
    assert(result.end.bound == _Z_TIME_BOUND_EXCLUSIVE);
    assert(compare_double_result(0.1005, result.end.now_offset));

    range = "]now(-87.6s)..now(1.5m)[";
    assert(_z_time_range_from_str(range, strlen(range), &result) == true);
    assert(result.start.bound == _Z_TIME_BOUND_EXCLUSIVE);
    assert(compare_double_result(-87.6, result.start.now_offset));
    assert(result.end.bound == _Z_TIME_BOUND_INCLUSIVE);
    assert(compare_double_result(90.0, result.end.now_offset));

    range = "[now(-24.5h)..now(6.75d)]";
    assert(_z_time_range_from_str(range, strlen(range), &result) == true);
    assert(result.start.bound == _Z_TIME_BOUND_INCLUSIVE);
    assert(compare_double_result(-88200.0, result.start.now_offset));
    assert(result.end.bound == _Z_TIME_BOUND_EXCLUSIVE);
    assert(compare_double_result(583200.0, result.end.now_offset));

    range = "[now(-1.75w)..now()]";
    assert(_z_time_range_from_str(range, strlen(range), &result) == true);
    assert(result.start.bound == _Z_TIME_BOUND_INCLUSIVE);
    assert(compare_double_result(-1058400.0, result.start.now_offset));
    assert(result.end.bound == _Z_TIME_BOUND_EXCLUSIVE);
    assert(compare_double_result(0.0, result.end.now_offset));

    // Duration tests
    range = "[now();7.3]";
    assert(_z_time_range_from_str(range, strlen(range), &result) == true);
    assert(result.start.bound == _Z_TIME_BOUND_INCLUSIVE);
    assert(compare_double_result(0.0, result.start.now_offset));
    assert(result.end.bound == _Z_TIME_BOUND_EXCLUSIVE);
    assert(compare_double_result(7.3, result.end.now_offset));

    range = "[now();97.4u]";
    assert(_z_time_range_from_str(range, strlen(range), &result) == true);
    assert(result.start.bound == _Z_TIME_BOUND_INCLUSIVE);
    assert(compare_double_result(0.0, result.start.now_offset));
    assert(result.end.bound == _Z_TIME_BOUND_EXCLUSIVE);
    assert(compare_double_result(0.0000974, result.end.now_offset));

    range = "[now();568.4ms]";
    assert(_z_time_range_from_str(range, strlen(range), &result) == true);
    assert(result.start.bound == _Z_TIME_BOUND_INCLUSIVE);
    assert(compare_double_result(0.0, result.start.now_offset));
    assert(result.end.bound == _Z_TIME_BOUND_EXCLUSIVE);
    assert(compare_double_result(0.5684, result.end.now_offset));

    range = "[now();9.4s]";
    assert(_z_time_range_from_str(range, strlen(range), &result) == true);
    assert(result.start.bound == _Z_TIME_BOUND_INCLUSIVE);
    assert(compare_double_result(0.0, result.start.now_offset));
    assert(result.end.bound == _Z_TIME_BOUND_EXCLUSIVE);
    assert(compare_double_result(9.4, result.end.now_offset));

    range = "[now();6.89m]";
    assert(_z_time_range_from_str(range, strlen(range), &result) == true);
    assert(result.start.bound == _Z_TIME_BOUND_INCLUSIVE);
    assert(compare_double_result(0.0, result.start.now_offset));
    assert(result.end.bound == _Z_TIME_BOUND_EXCLUSIVE);
    assert(compare_double_result(413.4, result.end.now_offset));

    range = "[now();1.567h]";
    assert(_z_time_range_from_str(range, strlen(range), &result) == true);
    assert(result.start.bound == _Z_TIME_BOUND_INCLUSIVE);
    assert(compare_double_result(0.0, result.start.now_offset));
    assert(result.end.bound == _Z_TIME_BOUND_EXCLUSIVE);
    assert(compare_double_result(5641.2, result.end.now_offset));

    range = "[now();2.7894d]";
    assert(_z_time_range_from_str(range, strlen(range), &result) == true);
    assert(result.start.bound == _Z_TIME_BOUND_INCLUSIVE);
    assert(compare_double_result(0.0, result.start.now_offset));
    assert(result.end.bound == _Z_TIME_BOUND_EXCLUSIVE);
    assert(compare_double_result(241004.16, result.end.now_offset));

    range = "[now();5.9457w]";
    assert(_z_time_range_from_str(range, strlen(range), &result) == true);
    assert(result.start.bound == _Z_TIME_BOUND_INCLUSIVE);
    assert(compare_double_result(0.0, result.start.now_offset));
    assert(result.end.bound == _Z_TIME_BOUND_EXCLUSIVE);
    assert(compare_double_result(3595959.36, result.end.now_offset));

    // Error cases
    range = "";
    assert(_z_time_range_from_str(range, strlen(range), &result) == false);

    range = "[;]";
    assert(_z_time_range_from_str(range, strlen(range), &result) == false);

    range = "[now();]";
    assert(_z_time_range_from_str(range, strlen(range), &result) == false);

    range = "[now()..5.6]";
    assert(_z_time_range_from_str(range, strlen(range), &result) == false);

    range = "[now();s]";
    assert(_z_time_range_from_str(range, strlen(range), &result) == false);

    range = "[now();one]";
    assert(_z_time_range_from_str(range, strlen(range), &result) == false);
}

int main(void) {
    test_query_params();
    test_time_range();
    return 0;
}
