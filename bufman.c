#include "bufman.h"

#include <string.h>
#include <unistd.h>


ssize_t bufgrab(const int fd, char* const buf, size_t* const len,
        const size_t chunklen, const size_t keep)
{
    *len -= keep;
    memmove(buf, &buf[keep], *len);
    ssize_t count = read(fd, &buf[*len], chunklen);
    if (count >= 0)
        *len += count;
    return count;
}
