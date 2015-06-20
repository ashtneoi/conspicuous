#pragma once


#include <sys/types.h>


ssize_t bufgrab(int fd, char* const buf, const ssize_t len, ssize_t keep);
