#include "bufman.h"

#include <string.h>


ssize_t bufgrab(int fd, char* const buf, const ssize_t len, ssize_t keep)
{
    memmove(buf, &buf[keep], len - keep);
    return read(fd, &buf[len - keep], keep);
}
