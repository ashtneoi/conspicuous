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
    T_PLABEL, // program label
    T_DLABEL, // data label
    T_OPCODE,
    T_OCTNUM,
    T_DECNUM,
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
    int token_idx = 0;

    enum line_state {
        S_PLABEL,
        S_COLON,
        S_OPCODE,
        S_OPERAND,
        S_COMMA,
        S_COMMENT,
    } state = S_PLABEL;

    size_t start = *pos;
    size_t col = 1;

    bool first = true;
    while (true) {
        char c = src_buf[*pos];

        bool change;
        bool advance = true;
        size_t matchlen = *pos - start;

        if (state == S_PLABEL) {
            change = !(
                (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
                || c == '_' || (!first && c >= '0' && c <= '9'));

            first = false;

            if (change) {
                if (matchlen > 0) {
                    char* sym = malloc(matchlen + 1);
                    memcpy(sym, &src_buf[start], matchlen);
                    sym[matchlen] = '\0';
                    printf("%s\n", sym);

                    line.tokens[token_idx].type = T_LABEL;
                    line.tokens[token_idx].text = sym;
                    ++token_idx;

                    state = S_COLON;
                } else {
                    state = S_OPCODE;
                }

                advance = false;
            }
        } else if (state == S_COLON) {
            if (c != ':')
                fatal(1, "column %zu: Missing colon", col);

            change = true;
            state = S_OPCODE;
        } else if (state == S_OPCODE) {
            change = !(c >= 'a' && c <= 'z');

            if (change) {
                if (matchlen > 0) {
                    char* sym = malloc(matchlen + 1);
                    memcpy(sym, &src_buf[start], matchlen);
                    sym[matchlen] = '\0';
                    printf("%s\n", sym);

                    line.tokens[token_idx].type = T_OPCODE;
                    line.tokens[token_idx].text = sym;
                    ++token_idx;

                    state = S_OPERAND;
                } else {
                    fatal(1, "column %zu: Missing opcode", col);
                }

                advance = false;
            }
        } else if (state == S_OPERAND) {
            if (matchlen == 0) {
                if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                        c == '_') {
                    change = false;
                    line.tokens[token_idx].type = T_LABEL;
                } else if (c == '0') {
                    change = false;
                } else if (c >= '1' && c <= '9') {
                    change = false;
                    line.tokens[token_idx].type = T_DECNUM;
                } else {
                    change = true;
                    advance = false;
                    /*fatal(1, "column %zu: Missing operand", col);*/
                }
            } else if (matchlen == 1 && src_buf[start] == '0') {
                if (c >= '0' && c <= '7') {
                    line.tokens[token_idx].type = T_OCTNUM;
                    change = false;
                } else if (c == 'x') {
                    line.tokens[token_idx].type = T_HEXNUM;
                    change = false;
                } else {
                    line.tokens[token_idx].type = T_DECNUM;
                    change = true;
                }
            } else {
                enum token_type type = line.tokens[token_idx].type;

                if ((c >= '0' && c <= '7') || c == '_') {
                    change = false;
                } else if (type == T_HEXNUM && (
                        (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))) {
                    change = false;
                } else if ((type == T_DECNUM || type == T_HEXNUM ||
                        type == T_LABEL) && (c == 8 || c == 9)) {
                    change = false;
                } else if (type == T_LABEL && ((c >= 'A' && c <= 'Z') ||
                        (c >= 'a' && c <= 'z'))) {
                    change = false;
                } else {
                    change = true;
                    advance = false;

                    char* sym = malloc(matchlen + 1);
                    memcpy(sym, &src_buf[start], *pos);
                    sym[matchlen] = '\0';
                    printf("%s\n", sym);

                    line.tokens[token_idx].text = sym;
                    ++token_idx;

                    state = S_COMMA;
                }
            }
        } else if (state == S_COMMA) {
            if (c == ',') {
                state = S_OPERAND;
            } else {
                state = S_COMMENT;
                advance = false;
            }

            change = true;
        } else if (state == S_COMMENT) {
            if (matchlen == 0) {
                if (c != ';')
                    fatal(1, "column %zu: expected semicolon", col);
            } else if (c == '\n') {
                break;
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

        if (change)
            start = *pos;
    }

    line.tokens[token_idx].type = T_NONE;

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
