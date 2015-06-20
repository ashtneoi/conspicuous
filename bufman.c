#include "bufman.h"

#include <string.h>
#include <unistd.h>


ssize_t bufgrab(int fd, char* const buf, const size_t len, size_t keep)
{
    memmove(buf, &buf[keep], len - keep);
    return read(fd, &buf[len - keep], keep);
}
