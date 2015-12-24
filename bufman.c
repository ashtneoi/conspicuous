#include "bufman.h"

#include "fail.h"

#include <string.h>
#include <unistd.h>


ssize_t fill_buffer(struct buffer* const buffer, size_t* const keep)
{
    if (buffer->len - *keep > CHUNK_LEN)
        fatal(1, "Buffer is already full");

    buffer->len -= *keep;
    memmove(buffer->buf, buffer->buf + *keep, buffer->len);
    ssize_t count = read(buffer->src, buffer->buf + buffer->len, CHUNK_LEN);
    if (count >= 0)
        buffer->len += count;

    if (count < 0)
        fatal_e(1, "Can't read from source file");
    buffer->pos = buffer->pos - *keep;
    buffer->buf[buffer->len] = '\0';
    *keep = 0;
    return count;
}
