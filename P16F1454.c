#include "P16F1454.h"

#include "bufman.h"
#include "fail.h"
#include "cpic.h"
#include "utils.h"

#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>


#define CHUNK_LEN 32
#define BUFCAP (CHUNK_LEN * 2)


char buf[CHUNK_LEN * 2 + 1];


enum token_type {
    T_LABEL,
    T_PLABEL, // program label
    T_DLABEL, // data label
    T_OPCODE,
    T_NUMBER,
    T_NONE,
};


struct token {
    enum token_type type;
    char* text;
    uint16_t number;
};


static inline
void print_token(struct token* token)
{
    const char* type;
    bool is_text;

    if (token->type == T_LABEL) {
        type = "label";
        is_text = true;
    } else if (token->type == T_PLABEL) {
        type = "program label";
        is_text = true;
    } else if (token->type == T_DLABEL) {
        type = "data label";
        is_text = true;
    } else if (token->type == T_OPCODE) {
        type = "opcode";
        is_text = true;
    } else if (token->type == T_NUMBER) {
        type = "number";
        is_text = false;
    } else {
        fatal(1, "Invalid token");
    }

    if (is_text)
        printf("  %s: \"%s\"\n", type, token->text);
    else
        printf("  %s: %"PRIX16"\n", type, token->number);
}


struct line {
    struct token tokens[8];
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
ssize_t fill_buffer(const int src, size_t* const bufpos, size_t* const buflen,
        size_t* const keep)
{
    v2("Filling source file buffer (bufpos = %zd, keep = %zd, buflen = %zd)",
        *bufpos, *keep, *buflen);
    if (*buflen - *keep > CHUNK_LEN)
        fatal(1, "Buffer is already full");
    ssize_t count = bufgrab(src, buf, buflen, CHUNK_LEN, *keep);
    if (count < 0)
        fatal_e(1, "Can't read from source file");
    *bufpos = *bufpos - *keep;
    buf[*buflen] = '\0';
    *keep = 0;
    v2("buf = \"%s\" (pos = %zd, keep = %zd, len = %zd)",
        buf, *bufpos, *keep, *buflen);
    return count;
}


static
struct line lex_line(const int src, size_t* bufpos, size_t* buflen)
{
    struct line line;
    unsigned int token_idx = 0;
    size_t tokstart = *bufpos + 1;
    char c;
    size_t toklen;
    bool first_buf = true;

    do {
        //
        // Advance char.
        //

        ++*bufpos;
        if (*bufpos >= *buflen) {
            if (fill_buffer(src, bufpos, buflen, &tokstart) == 0) {
                if (first_buf)
                    break;
                else
                    fatal(1, "Unexpected end of file");
            }
            first_buf = false;
        }
        c = buf[*bufpos];
        toklen = *bufpos - tokstart;

        //
        // Process char.
        //

        if (c == ' ' || c == '\t' || c == '\n') {
            if (toklen > 0) {
                char copy[toklen + 1];
                memcpy(copy, &buf[tokstart], toklen);
                copy[toklen] = '\0';
                v1(copy);
            }
        } else {
            continue;
        }

        //
        // Start new token.
        //

        tokstart = *bufpos + 1;
    } while (c != '\n');

    line.tokens[token_idx].type = T_NONE;
    return line;
}


bool assemble_16F1454(const int src)
{
    size_t bufpos = 0;
    size_t buflen = 1;

    while (true) {
        v2("Lexing line");
        struct line line = lex_line(src, &bufpos, &buflen);
        for (unsigned int i = 0; i < lengthof(line.tokens) &&
                line.tokens[i].type != T_NONE; ++i)
            print_token(&line.tokens[i]);
        if (buflen == 0)
            break;
    }

    return false;
}
