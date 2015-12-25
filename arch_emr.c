#include "arch_emr.h"

#include "cpic.h"
#include "dict.h"
#include "fail.h"
#include "utils.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>


#define CHUNK_LEN 4


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
    if (b->end - b->tok >= CHUNK_LEN)
        fatal(E_RARE, "Buffer is full");
    memmove(b->buf, b->buf + b->tok, b->end - b->tok);
    b->pos = b->end - b->tok;
    b->tok = 0;
    ssize_t count = read(b->src, b->buf + b->pos, CHUNK_LEN - b->pos);
    if (count == -1) {
        fatal_e(E_COMMON, "Can't read from input file");
    } else if (count == 0) {
        return count;
    }

    b->end = b->pos + count;

    return count;
}


struct token {
    enum token_type {
        T_CHAR,
        T_TEXT,
        T_NUM,
        T_EOL,
        T_EOF,
    } type;
    char* text;
    int32_t num;
};


static
struct token next_token(struct buffer* const b)
{
    if (b->pos == b->end && fill_buffer(b) == 0)
        return (struct token){ .type = T_EOF };

    while (b->buf[b->pos] == ' ' || b->buf[b->pos] == '\t') {
        b->tok = ++b->pos;
        if (b->pos == b->end && fill_buffer(b) == 0)
            return (struct token){ .type = T_EOF };
    }

    if (b->buf[b->pos] == '\n') {
        b->tok = ++b->pos;
        return (struct token){ .type = T_EOL };
    }

    while (b->buf[b->pos] != ' ' && b->buf[b->pos] != '\t'
            /*&& b->buf[b->pos] != ',' && b->buf[b->pos] != ';'*/
            && b->buf[b->pos] != '\n') {
        ++b->pos;
        if (b->pos == b->end && fill_buffer(b) == 0)
            return (struct token){ .type = T_EOF };
    }

    return (struct token){
        .type = T_TEXT,
        .text = b->buf + b->tok,
        .num = b->pos - b->tok,
    };
}


void assemble_emr(const int src)
{
    struct buffer b = {
        .src = src,
        .end = 0,
        .tok = 0,
        .pos = 0,
    };

    if (fill_buffer(&b) == 0) {
        print("EOF\n");
        return;
    }

    while (true) {
        /*next_token(&b);*/
        struct token tkn = next_token(&b);

        if (tkn.type == T_TEXT) {
            for (size_t i = 0; i < (size_t)tkn.num; ++i)
                putchar(tkn.text[i]);
            putchar('\n');
        } else if (tkn.type == T_EOL) {
            print("EOL\n");
        } else if (tkn.type == T_EOF) {
            print("EOF\n");
            break;
        } else {
            print("???\n");
        }


        b.tok = b.end;
    }
}
