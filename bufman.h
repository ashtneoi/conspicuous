#pragma once


#include "common.h"

#include <sys/types.h>


ssize_t bufgrab(const int fd, char* const buf, size_t* const pos,
        const size_t chunklen, const size_t keep);
