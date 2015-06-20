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


static struct line lex_line(const int src, size_t* pos)
{
    struct line line;
    int line_idx = 0;

    enum token_type state;
    (void)state;

    // Lex important part of line.
    while (true) {
        while (src_buf[*pos] == ' ' || src_buf[*pos] == '\t') {
            ++*pos;
            if (*pos >= lengthof(src_buf)) {
                v2("Filling source file buffer");
                ssize_t count = bufgrab(src, src_buf, lengthof(src_buf),
                    lengthof(src_buf));
                if (count < 0)
                    fatal_e(1, "Can't read from source file");
                else if (count == 0)
                    fatal(1, "Incomplete line");
                *pos = 0;
            }
        }

        
    }

    // Skip until end of line.
    while (src_buf[*pos] != '\n') {
        ++*pos;
        if (*pos >= lengthof(src_buf)) {
            v2("Filling source file buffer");
            ssize_t count = bufgrab(src, src_buf, lengthof(src_buf),
                lengthof(src_buf));
            if (count < 0)
                fatal_e(1, "Can't read from source file");
            else if (count == 0)
                fatal(1, "Incomplete line");
            *pos = 0;
        }
    }

    line.tokens[line_idx].type = T_NONE;

    return line;
}


bool assemble_16F1454(const int src)
{
    size_t pos = lengthof(src_buf);
    while (true) {
        lex_line(src, &pos);
    }

    return false;
}
