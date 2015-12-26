#include "arch_emr.h"

#include "cpic.h"
#include "dict.h"
#include "fail.h"
#include "utils.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>


#define CHUNK_LEN 256


struct buffer {
    char buf[CHUNK_LEN * 2 + 1];
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
    ssize_t count = read(b->src, b->buf + b->pos, CHUNK_LEN);
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
struct token next_token(struct buffer* const b, int l)
{
    if (b->pos == b->end && fill_buffer(b) == 0)
        return (struct token){ .type = T_EOF };

    while (b->buf[b->pos] == ' ' || b->buf[b->pos] == '\t') {
        b->tok = ++b->pos;
        if (b->pos == b->end && fill_buffer(b) == 0)
            return (struct token){ .type = T_EOF };
    }

    if (b->buf[b->pos] == ',') {
        b->tok = ++b->pos;
        return (struct token){ .type = T_CHAR, .num = ',' };
    } else if (b->buf[b->pos] == ';' || b->buf[b->pos] == '\n') {
        while (b->buf[b->pos] != '\n') {
            b->tok = ++b->pos;
            if (b->pos == b->end && fill_buffer(b) == 0)
                return (struct token){ .type = T_EOF };
        }
        b->tok = ++b->pos;
        return (struct token){ .type = T_EOL };
    }

    while (strchr(" \t,;\n", b->buf[b->pos]) == NULL) {
        ++b->pos;
        if (b->pos == b->end && fill_buffer(b) == 0)
            return (struct token){ .type = T_EOF };
    }

    char* chr = b->buf + b->tok;
    const char* const tknend = b->buf + b->pos;

    bool negative = false;
    if (*chr == '-') {
        ++chr;
        negative = true;
    }

    if (*chr == '0') {
        ++chr;
        struct token t = { .type = T_NUM, .num = 0 };
        if (chr == tknend) {
            return t;
        } else if (*chr == 'x') {
            // hex
            while (++chr < tknend) {
                int digit;
                if ('0' <= *chr && *chr <= '9')
                    digit = *chr - '0';
                else if ('A' <= *chr && *chr <= 'F')
                    digit = 0xA + *chr - 'A';
                else if ('a' <= *chr && *chr <= 'f')
                    digit = 0xA + *chr - 'a';
                else if (*chr == '_')
                    continue;
                else
                    fatal(E_COMMON, "%d: Invalid hexadecimal integer", l);
                t.num = (t.num << 4) + digit;
            }
        } else if (*chr == 'n' || *chr == 'b') {
            // binary
            while (++chr < tknend) {
                if (*chr == '0' || *chr == '1')
                    t.num = (t.num << 1) + *chr - '0';
                else if (*chr == '_')
                    continue;
                else
                    fatal(E_COMMON, "%d: Invalid binary integer", l);
            }
        } else if (*chr == 'c') {
            // octal
            while (++chr < tknend) {
                if ('0' <= *chr && *chr <= '7')
                    t.num = (t.num << 3) + *chr - '0';
                else if (*chr == '_')
                    continue;
                else
                    fatal(E_COMMON, "%d: Invalid octal integer", l);
            }
        } else {
            fatal(E_COMMON, "%d: Invalid integer", l);
        }
        if (negative)
            t.num = -t.num;
        return t;
    } else if ('1' <= *chr && *chr <= '9') {
        // decimal
        struct token t = { .type = T_NUM, .num = 0 };
        do {
            if ('0' <= *chr && *chr <= '9')
                t.num = t.num * 10 + *chr - '0';
            else
                fatal(E_COMMON, "%d: Invalid decimal integer", l);
        } while (++chr < tknend);
        if (negative)
            t.num = -t.num;
        return t;
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

    int l = 1;
    while (true) {
        struct token tkn = next_token(&b, l);

        if (tkn.type == T_TEXT) {
            putchar('\"');
            for (size_t i = 0; i < (size_t)tkn.num; ++i)
                putchar(tkn.text[i]);
            print("\"\n");
        } else if (tkn.type == T_EOL) {
            print("EOL\n");
            ++l;
        } else if (tkn.type == T_EOF) {
            print("EOF\n");
            break;
        } else if (tkn.type == T_CHAR) {
            printf("'%c'\n", (char)tkn.num);
        } else if (tkn.type == T_NUM) {
            printf("0x%X\n", tkn.num);
        }
    }
}
