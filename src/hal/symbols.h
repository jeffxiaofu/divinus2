#pragma once

#include <dlfcn.h>
#include <stdio.h>

#include "macros.h"

#define LOAD_SYMBOL(module_tag, handle, symbol_name, func_ptr)                                      \
    do                                                                                              \
    {                                                                                               \
        if (!((func_ptr) = (typeof(func_ptr))hal_symbol_load(module_tag, (handle), (symbol_name)))) \
        {                                                                                           \
            printf("Failed to load symbol: %s\n", (symbol_name));                                   \
            return (EXIT_FAILURE);                                                                  \
        }                                                                                           \
    } while (0)
static void inline *hal_symbol_load(const char *module, void *handle, const char *symbol)
{
    void *function = dlsym(handle, symbol);
    if (!function)
    {
        HAL_DANGER(module, "Failed to acquire symbol %s!\n", symbol);
        return (void *)0;
    }
    return function;
}