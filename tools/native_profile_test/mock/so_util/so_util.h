#pragma once
#include <stdint.h>
#include <stddef.h>
typedef struct { uintptr_t load_addr, text_base; size_t text_size; } so_module;
typedef struct { uintptr_t addr; } so_hook;
so_hook hook_addr(uintptr_t addr, uintptr_t dst);
size_t so_exec_available(void);
void *so_alloc_exec(size_t size);
