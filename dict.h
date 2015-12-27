#pragma once


#include "common.h"
#include "utils.h"

#include <stddef.h>


#define dict_define(n, a) struct dict n = { \
    .array = a, .capacity = lengthof(a), .item_len = sizeof(a[0]) }


struct dict {
    void* const array;
    const size_t capacity;
    const size_t item_len;
};


void dict_init(const struct dict* dict);
void* dict_avail(const struct dict* dict, const char* key);
void* dict_get(const struct dict* dict, const char* key);
