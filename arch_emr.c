#include "arch_emr.h"

#include "cpic.h"
#include "dict.h"
#include "fail.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>


#define CHUNK_LEN 3

struct buffer {
    char buf[CHUNK_LEN * 2];
    const int src;
    size_t end;
    size_t tok;
    size_t pos;
};


static
ssize_t fill_buffer(struct buffer* const b)
{
    memmove(b->buf + b->tok, b->buf, b->end - b->tok);
    b->pos = b->end - b->tok;
    b->tok = 0;
    if (CHUNK_LEN - b->tok == 0)
        fatal(E_RARE, "Buffer is full");
    ssize_t count = read(b->src, b->buf + b->pos, CHUNK_LEN - b->pos);
    if (count == -1) {
        fatal_e(E_COMMON, "Can't read from input file");
    } else if (count == 0) {
        return count;
    }

    b->end = b->pos + count;

    return count;
}


void assemble_emr(const int src)
{
    struct buffer b = {
        .src = src,
        .end = 0,
        .tok = 0,
        .pos = 0,
    };

    while (true) {
        if (fill_buffer(&b) == 0) {
            print("\nEOF\n");
            break;
        }

        for (size_t i = b.pos; i < b.end; ++i)
            putchar(b.buf[i]);

        b.tok = b.end;
    }
}
