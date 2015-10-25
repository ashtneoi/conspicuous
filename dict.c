#include "common.h"
#include "dict.h"

#include "fail.h"
#include "utils.h"


#include <stdbool.h>
#include <string.h>


#define arrind(d, i) ((char*)((d)->array) + (i) * (d)->value_len)


// djb2 by Dan Bernstein
static
unsigned long hash(const char* str)
{
    unsigned long hash = 5381;
    int c;
    while ((c = *(str++)))
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    return hash;
}


void dict_init(const struct dict* const dict)
{
    for (size_t i = 0; i < dict->capacity; ++i)
        *(char**)arrind(dict, i) = NULL;
}


void* dict_avail(const struct dict* const dict, const char* const key)
{
    size_t h = hash(key) % dict->capacity;
    while (*(char**)arrind(dict, h) != NULL) {
        ++h;
        if (h >= dict->capacity)
            fatal(1, "Dict is full");
    }
    return (void*)arrind(dict, h);
}


void* dict_get(const struct dict* const dict, const char* const key)
{
    size_t h = hash(key) % dict->capacity;
    while (h < dict->capacity && *(char**)arrind(dict, h) != NULL) {
        if (0 == strcmp(*(char**)arrind(dict, h), key))
            return arrind(dict, h);
        ++h;
    }
    return NULL;
}
