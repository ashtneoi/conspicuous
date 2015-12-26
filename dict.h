#pragma once


#include "common.h"

#include <stddef.h>


struct dict {
    void* const array;
    const size_t capacity;
    const size_t value_len;
};


void dict_init(const struct dict* dict);
void* dict_avail(const struct dict* dict, const char* key);
void* dict_get(const struct dict* dict, const char* key);
