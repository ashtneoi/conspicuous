#include "P16F1454.h"

#include "bufman.h"
#include "fail.h"
#include "cpic.h"
#include "utils.h"

#include <stdbool.h>
#include <string.h>
#include <sys/types.h>


char src_buf[32];


enum token_type {
    T_LABEL,
    T_OPCODE,
    T_HEXNUM,
    T_NONE,
};


enum line_state {
    S_LABEL,
    S_OPCODE,
    S_ARGS,
    S_COMMENT,
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
ssize_t fill_buffer(const int src, size_t* const pos, const size_t keep)
{
    /*if (keep == 0)*/
        /*fatal(1, "Buffer is already full");*/
    v2("Filling source file buffer");
    ssize_t count = bufgrab(src, &src_buf[*pos], lengthof(src_buf), keep);
    if (count < 0)
        fatal_e(1, "Can't read from source file");
    *pos = lengthof(src_buf) - keep + count;
    return count;
}


static
struct line lex_line(const int src, size_t* pos)
{
    struct line line;
    int line_idx = 0;

    enum line_state state;
    (void)state;

    size_t start = *pos;

    // Lex important part of line.
    bool first = true;
    while (true) {
        char c = src_buf[*pos];
        bool match = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
            || c == '_' || (!first && c >= '0' && c <= '9');

        if (!match) {
            start = *pos;

            char sym[lengthof(src_buf) + 1];
            memcpy(sym, &src_buf[start], *pos);
            sym[*pos] = '\0';
            printf("%s\n", sym);

            break;
        }

        first = false;

        ++*pos;
        if (*pos >= lengthof(src_buf)
                && fill_buffer(src, pos, start) == 0)
            fatal(1, "Incomplete line");
    }

    /*while (src_buf[*pos] == ' ' || src_buf[*pos] == '\t') {*/
        /*++*pos;*/
        /*if (*pos >= lengthof(src_buf)) {*/
            /*v2("Filling source file buffer");*/
            /*ssize_t count = fill_buffer(src, pos, lengthof(src_buf));*/
            /*if (count < 0)*/
                /*fatal_e(1, "Can't read from source file");*/
            /*else if (count == 0)*/
                /*fatal(1, "Incomplete line");*/
            /**pos = 0;*/
        /*}*/
    /*}*/


    line.tokens[line_idx].type = T_NONE;

    return line;
}


bool assemble_16F1454(const int src)
{
    size_t pos = lengthof(src_buf);

    if (fill_buffer(src, &pos, lengthof(src_buf)) == 0)
        return true;

    while (true) {
        lex_line(src, &pos);
    }

    return false;
}
