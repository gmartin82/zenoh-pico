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

#include <emscripten/html5.h>
#include <errno.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "zenoh-pico/config.h"
#include "zenoh-pico/system/platform.h"

/*------------------ Random ------------------*/
uint8_t z_random_u8(void) { return (emscripten_random() * 255.0); }

uint16_t z_random_u16(void) { return (emscripten_random() * 65535.0); }

uint32_t z_random_u32(void) { return (emscripten_random() * 4294967295.0); }

uint64_t z_random_u64(void) { return (emscripten_random() * 18446744073709551615.0); }

void z_random_fill(void *buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        *((uint8_t *)buf) = z_random_u8();
    }
}

/*------------------ Memory ------------------*/
void *z_malloc(size_t size) { return malloc(size); }

void *z_realloc(void *ptr, size_t size) { return realloc(ptr, size); }

void z_free(void *ptr) { free(ptr); }

#if Z_FEATURE_MULTI_THREAD == 1
/*------------------ Task ------------------*/
z_result_t _z_task_init(_z_task_t *task, pthread_attr_t *attr, void *(*fun)(void *), void *arg) {
    _Z_CHECK_SYS_ERR(pthread_create(task, attr, fun, arg));
}

z_result_t _z_task_join(_z_task_t *task) { _Z_CHECK_SYS_ERR(pthread_join(*task, NULL)); }

z_result_t _z_task_detach(_z_task_t *task) { _Z_CHECK_SYS_ERR(pthread_detach(*task)); }

z_result_t _z_task_cancel(_z_task_t *task) { _Z_CHECK_SYS_ERR(pthread_cancel(*task)); }

void _z_task_exit(void) { pthread_exit(NULL); }

void _z_task_free(_z_task_t **task) {
    _z_task_t *ptr = *task;
    z_free(ptr);
    *task = NULL;
}

/*------------------ Mutex ------------------*/
z_result_t _z_mutex_init(_z_mutex_t *m) { _Z_CHECK_SYS_ERR(pthread_mutex_init(m, NULL)); }

z_result_t _z_mutex_drop(_z_mutex_t *m) { _Z_CHECK_SYS_ERR(pthread_mutex_destroy(m)); }

z_result_t _z_mutex_lock(_z_mutex_t *m) { _Z_CHECK_SYS_ERR(pthread_mutex_lock(m)); }

z_result_t _z_mutex_try_lock(_z_mutex_t *m) { _Z_CHECK_SYS_ERR(pthread_mutex_trylock(m)); }

z_result_t _z_mutex_unlock(_z_mutex_t *m) { _Z_CHECK_SYS_ERR(pthread_mutex_unlock(m)); }

z_result_t _z_mutex_rec_init(_z_mutex_rec_t *m) {
    pthread_mutexattr_t attr;
    _Z_RETURN_IF_SYS_ERR(pthread_mutexattr_init(&attr));
    _Z_RETURN_IF_SYS_ERR(pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE));
    _Z_RETURN_IF_SYS_ERR(pthread_mutex_init(m, &attr));
    _Z_CHECK_SYS_ERR(pthread_mutexattr_destroy(&attr));
}

z_result_t _z_mutex_rec_drop(_z_mutex_rec_t *m) { _Z_CHECK_SYS_ERR(pthread_mutex_destroy(m)); }

z_result_t _z_mutex_rec_lock(_z_mutex_rec_t *m) { _Z_CHECK_SYS_ERR(pthread_mutex_lock(m)); }

z_result_t _z_mutex_rec_try_lock(_z_mutex_rec_t *m) { _Z_CHECK_SYS_ERR(pthread_mutex_trylock(m)); }

z_result_t _z_mutex_rec_unlock(_z_mutex_rec_t *m) { _Z_CHECK_SYS_ERR(pthread_mutex_unlock(m)); }

/*------------------ Condvar ------------------*/
z_result_t _z_condvar_init(_z_condvar_t *cv) {
    pthread_condattr_t attr;
    pthread_condattr_init(&attr);
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    _Z_CHECK_SYS_ERR(pthread_cond_init(cv, &attr));
}

z_result_t _z_condvar_drop(_z_condvar_t *cv) { _Z_CHECK_SYS_ERR(pthread_cond_destroy(cv)); }

z_result_t _z_condvar_signal(_z_condvar_t *cv) { _Z_CHECK_SYS_ERR(pthread_cond_signal(cv)); }

z_result_t _z_condvar_signal_all(_z_condvar_t *cv) { _Z_CHECK_SYS_ERR(pthread_cond_broadcast(cv)); }

z_result_t _z_condvar_wait(_z_condvar_t *cv, _z_mutex_t *m) { _Z_CHECK_SYS_ERR(pthread_cond_wait(cv, m)); }

z_result_t _z_condvar_wait_until(_z_condvar_t *cv, _z_mutex_t *m, const z_clock_t *abstime) {
    struct timespec ts;
    ts.tv_sec = (time_t)(*abstime / 1000);
    ts.tv_nsec = (long)((*abstime - (ts.tv_sec * 1000)) * 1000000);

    int error = pthread_cond_timedwait(cv, m, &ts);

    if (error == ETIMEDOUT) {
        return Z_ETIMEDOUT;
    }

    _Z_CHECK_SYS_ERR(error);
}
#endif  // Z_FEATURE_MULTI_THREAD == 1

/*------------------ Sleep ------------------*/
z_result_t z_sleep_us(size_t time) {
    emscripten_sleep((time / 1000) + (time % 1000 == 0 ? 0 : 1));
    return 0;
}

z_result_t z_sleep_ms(size_t time) {
    emscripten_sleep(time);
    return 0;
}

z_result_t z_sleep_s(size_t time) {
    z_time_t start = z_time_now();

    // Most sleep APIs promise to sleep at least whatever you asked them to.
    // This may compound, so this approach may make sleeps longer than expected.
    // This extra check tries to minimize the amount of extra time it might sleep.
    while (z_time_elapsed_s(&start) < time) {
        z_sleep_ms(1000);
    }

    return 0;
}

/*------------------ Instant ------------------*/
z_clock_t z_clock_now(void) { return z_time_now(); }

unsigned long z_clock_elapsed_us(z_clock_t *instant) { return z_clock_elapsed_ms(instant) * 1000; }

unsigned long z_clock_elapsed_ms(z_clock_t *instant) { return z_time_elapsed_ms(instant); }

unsigned long z_clock_elapsed_s(z_clock_t *instant) { return z_time_elapsed_ms(instant) / 1000; }

void z_clock_advance_us(z_clock_t *clock, unsigned long duration) { *clock += (double)(duration / 1000); }

void z_clock_advance_ms(z_clock_t *clock, unsigned long duration) { *clock += (double)duration; }

void z_clock_advance_s(z_clock_t *clock, unsigned long duration) { *clock += (double)(duration * 1000); }

/*------------------ Time ------------------*/
z_time_t z_time_now(void) { return emscripten_get_now(); }

const char *z_time_now_as_str(char *const buf, unsigned long buflen) {
    snprintf(buf, buflen, "%f", z_time_now());
    return buf;
}

unsigned long z_time_elapsed_us(z_time_t *time) { return z_time_elapsed_ms(time) * 1000; }

unsigned long z_time_elapsed_ms(z_time_t *time) {
    z_time_t now = emscripten_get_now();

    unsigned long elapsed = now - *time;
    return elapsed;
}

unsigned long z_time_elapsed_s(z_time_t *time) { return z_time_elapsed_ms(time) * 1000; }

z_result_t _z_get_time_since_epoch(_z_time_since_epoch *t) {
    double date = emscripten_date_now();
    t->secs = (uint32_t)(date / 1000);
    t->nanos = (uint32_t)((date - t->secs * 1000) * 1000000);
    return 0;
}
