#include "P16F1454.h"

#include "bufman.h"
#include "fail.h"
#include "cpic.h"
#include "utils.h"

#include <stdbool.h>
#include <string.h>
#include <sys/types.h>


#define CHUNK_LEN 32


char src_buf[CHUNK_LEN * 2];


enum token_type {
    T_LABEL,
    T_OPCODE,
    T_HEXNUM,
    T_NONE,
};


struct token {
    enum token_type type;
    char* text;
    int number;
};


struct line {
    struct token tokens[4];
};


struct insn {
    char* label;
    int opcode;
    int f; // register file address
    int b; // bit number
    int k; // literal
    char* k_str;
    int d; // destination select (0 = W, 1 = f)
    int n; // FSR or INDF number
    int mm; // pre-/post-decrement/-increment select
};


static inline
ssize_t fill_buffer(const int src, size_t* const pos, size_t* const len,
        const size_t keep)
{
    if (*len - keep > CHUNK_LEN)
        fatal(1, "Buffer is already full");
    v2("Filling source file buffer");
    ssize_t count = bufgrab(src, src_buf, len, CHUNK_LEN, keep);
    if (count < 0)
        fatal_e(1, "Can't read from source file");
    *pos = *len;
    return count;
}


static
struct line lex_line(const int src, size_t* pos, size_t* len)
{
    struct line line;
    int line_idx = 0;

    enum line_state {
        S_LABEL,
        S_COLON,
        S_OPCODE,
        S_ARG,
        S_COMMA,
        S_COMMENT,
    } state = S_LABEL;

    size_t start = *pos;
    size_t col = 1;

    bool first = true;
    while (true) {
        char c = src_buf[*pos];
        /*putchar(c);*/
        /*putchar('\n');*/

        bool next;
        bool advance = true;
        size_t matchlen = *pos - start;

        if (state == S_LABEL) {
            next = !(
                (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
                || c == '_' || (!first && c >= '0' && c <= '9'));

            first = false;

            if (next) {
                if (matchlen > 0) {
                    char sym[lengthof(src_buf) + 1];
                    memcpy(sym, &src_buf[start], *pos);
                    sym[*pos] = '\0';
                    printf("%s\n", sym);

                    state = S_COLON;
                } else {
                    state = S_OPCODE;
                }

                advance = false;
            }
        } else if (state == S_COLON) {
            if (c != ':')
                fatal(1, "column %zu: Missing colon", col);

            next = true;
            state = S_OPCODE;
        } else if (state == S_OPCODE) {
            next = !(c >= 'a' && c <= 'z');

            if (next) {
                if (matchlen > 0) {
                    char sym[lengthof(src_buf) + 1];
                    memcpy(sym, &src_buf[start], *pos);
                    sym[*pos] = '\0';
                    printf("%s\n", sym);

                    state = S_ARG;
                } else {
                    fatal(1, "column %zu: Missing opcode", col);
                }

                advance = false;
            }
        }

        while (advance || src_buf[*pos] == ' ') {
            ++*pos;
            ++col;
            if (*pos >= *len) {
                if (fill_buffer(src, pos, len, start) == 0)
                    fatal(1, "Incomplete line");
                start = 0;
            }

            advance = false;
        }

        if (next)
            start = *pos;
    }

    line.tokens[line_idx].type = T_NONE;

    return line;
}


bool assemble_16F1454(const int src)
{
    size_t pos;
    size_t len = 0;

    if (fill_buffer(src, &pos, &len, 0) == 0)
        return true;

    pos = 0;

    while (true) {
        lex_line(src, &pos, &len);
    }

    return false;
}
