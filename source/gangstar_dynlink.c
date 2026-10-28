#include "gangstar_dynlink.h"
#include "utils/init.h"
#include "utils/dialog.h"
#include "utils/logger.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern so_module so_mod;
static int platform_handle;
static int global_handle;
static char last_error[192];
static int error_pending;

static void dynamic_error(const char *message, const char *name) {
    snprintf(last_error, sizeof(last_error), "%s: %s", message, name ? name : "NULL");
    error_pending = 1;
    l_warn("libdl: %s", last_error);
}

char *gangstar_dlerror(void) {
    if (!error_pending) return NULL;
    error_pending = 0;
    return last_error;
}

void *gangstar_dlopen(const char *path, int flags) {
    // The observed loader uses RTLD_NOW (2). Honor supported NOW/LAZY/GLOBAL
    // bits; never claim success for an unknown library or unsupported mode.
    if (flags & ~(3 | 0x100)) { dynamic_error("Unsupported dlopen flags", path); return NULL; }
    int kind = gangstar_library_kind(path);
    l_info("dlopen(%s, 0x%x)", path ? path : "NULL", flags);
    if (kind < 0) { dynamic_error("Unknown library", path); return NULL; }
    if (kind == 0) return path ? &platform_handle : &global_handle;
    if (kind == 1) return &so_mod;
    dynamic_error("Unknown library", path); return NULL;
}

static void *lookup_module(so_module *module, const char *symbol) {
    return (void *)so_symbol(module, symbol);
}

void *gangstar_dlsym(void *handle, const char *symbol) {
    if (!symbol) { dynamic_error("Null symbol", NULL); return NULL; }
    int global = !handle || handle == &global_handle || (uintptr_t)handle == UINTPTR_MAX || (uintptr_t)handle == UINTPTR_MAX - 1;
    int valid = global || handle == &platform_handle || handle == &so_mod;
    void *result = NULL;
    if (handle == &so_mod || global) result = lookup_module(&so_mod, symbol);
    if (!valid) { dynamic_error("Invalid library handle", symbol); return NULL; }
    if (!result && handle != &platform_handle) result = lookup_module(&so_mod, symbol);
    if (!result) result = gangstar_builtin_symbol(symbol);
    if (!result) dynamic_error("Unknown symbol", symbol);
    else l_debug("dlsym(%p, %s) = %p", handle, symbol, result);
    return result;
}

int gangstar_dlclose(void *handle) {
    if (handle == &platform_handle || handle == &global_handle || handle == &so_mod) return 0;
    dynamic_error("Invalid dlclose handle", NULL); return -1;
}
