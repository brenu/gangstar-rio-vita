/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2022      Rinnegatamante
 * Copyright (C) 2022-2024 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "reimpl/log.h"
#include "utils/logger.h"
#include <string.h>
#include <stdatomic.h>
#include <psp2/kernel/clib.h>
#include <stdlib.h>

#define print_common \
    switch (prio) { \
        case ANDROID_LOG_INFO: \
            l_info("[ALOG][%s] %s", tag, text); \
            break; \
        case ANDROID_LOG_WARN: \
            l_warn("[ALOG][%s] %s", tag, text); \
            break; \
        case ANDROID_LOG_ERROR: \
        case ANDROID_LOG_FATAL: \
            l_error("[ALOG][%s] %s", tag, text); \
            break; \
        case ANDROID_LOG_UNKNOWN: \
        case ANDROID_LOG_DEFAULT: \
        case ANDROID_LOG_VERBOSE: \
        case ANDROID_LOG_DEBUG: \
        case ANDROID_LOG_SILENT: \
        default: \
            l_debug("[ALOG][%s] %s", tag, text); \
            break; \
    }

static int report_message(int prio, const char *text) {
    if (!text || strncmp(text, "OpenGL error:", 13)) return 1;
#ifdef GANGSTAR_QUIET_DIAGNOSTICS
    static atomic_uint known_enum;
    const char *known = "OpenGL error: 0x500 GL_INVALID_ENUM";
    size_t length = strlen(known);
    if (prio == ANDROID_LOG_INFO && !strncmp(text, known, length) &&
        (!text[length] || (text[length] == '\n' && !text[length + 1]))) {
        unsigned occurrence = atomic_fetch_add(&known_enum, 1);
        // Preserve initial examples and all per-window counts in PERF output.
        // New messages, other error codes and error/fatal severity still print.
        return occurrence < 4;
    }
#endif
    return 1;
}

int __android_log_write(int prio, const char* tag, const char* text) {
    if (!report_message(prio, text)) return 0;
    print_common
    return 0;
}

int __android_log_print(int prio, const char* tag, const char* fmt, ...) {
    va_list list;
    char text[1024];

    va_start(list, fmt);
    sceClibVsnprintf(text, sizeof(text), fmt, list);
    va_end(list);
    if (!report_message(prio, text)) return 0;

    print_common

    return 0;
}

int __android_log_vprint(int prio, const char* tag, const char* fmt, va_list ap) {
    char text[1024];

    sceClibVsnprintf(text, sizeof(text), fmt, ap);
    if (!report_message(prio, text)) return 0;

    print_common

    return 0;
}

void __android_log_assert(const char* cond, const char* tag, const char* fmt, ...) {
    if (fmt) {
        va_list list;
        char text[1024];

        va_start(list, fmt);
        sceClibVsnprintf(text, sizeof(text), fmt, list);
        va_end(list);

        l_fatal("[ALOG][ASSERT] %s", text);
    } else {
        if (cond) {
            l_fatal("[ALOG][ASSERT] Assertion failed: %s", cond);
        } else {
            l_fatal("[ALOG][ASSERT] Unspecified assertion failed");
        }
    }

    abort();
}
