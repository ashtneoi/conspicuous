#pragma once


#include <sys/types.h>


ssize_t bufgrab(const int fd, char* const buf, const size_t len,
        const size_t keep);
