#pragma once


#include "common.h"

#include <sys/types.h>


#define CHUNK_LEN 128


struct buffer {
    const int src;
    char buf[CHUNK_LEN * 2 + 1];
    size_t pos;
    size_t len;
};


ssize_t fill_buffer(struct buffer* const buffer, size_t* const keep);
