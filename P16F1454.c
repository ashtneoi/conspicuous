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
#define EXPECTED(s) do { \
    if (!optional) fatal(1, "column %u: Expected " s, col); \
    } while (0)


char src_buf[CHUNK_LEN * 2 + 1];


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
        printf("%s: %s\n", type, token->text);
    else
        printf("%s: %"PRIX16"\n", type, token->number);
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
ssize_t fill_buffer(const int src, size_t* const pos, size_t* const len,
        const size_t keep)
{
    v2("Filling source file buffer (pos = %zd, keep = %zd, len = %zd)",
        *pos, keep, *len);
    if (*len - keep > CHUNK_LEN)
        fatal(1, "Buffer is already full");
    ssize_t count = bufgrab(src, src_buf, len, CHUNK_LEN, keep);
    if (count < 0)
        fatal_e(1, "Can't read from source file");
    *pos = *pos - keep;
    src_buf[*len] = '\0';
    v2("src_buf = \"%s\" (pos = %zd, keep = %zd, len = %zd)",
        src_buf, *pos, keep, *len);
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

    enum {
        O_LABEL = 0,
        O_OCTNUM = 8,
        O_DECNUM = 10,
        O_HEXNUM = 16,
    } opd_type;;

    size_t start = *pos;
    size_t col = 1;

    bool first = true;
    bool optional = true;
    bool line_done = false;
    while (!line_done) {
        char c = src_buf[*pos];

        bool change;
        bool advance = true;
        size_t matchlen = *pos - start;

        struct token* token = &line.tokens[token_idx];

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

                    token->type = T_LABEL;
                    token->text = sym;
                    ++token_idx;

                    state = S_COLON;
                    optional = false;
                } else {
                    EXPECTED("program label");
                    state = S_OPCODE;
                    optional = true;
                }

                advance = false;
            }
        } else if (state == S_COLON) {
            if (c != ':')
                EXPECTED("colon");

            change = true;
            state = S_OPCODE;
            optional = true;
        } else if (state == S_OPCODE) {
            change = !(c >= 'a' && c <= 'z');

            if (change) {
                if (matchlen > 0) {
                    char* sym = malloc(matchlen + 1);
                    memcpy(sym, &src_buf[start], matchlen);
                    sym[matchlen] = '\0';
                    printf("%s\n", sym);

                    token->type = T_OPCODE;
                    token->text = sym;
                    ++token_idx;

                    state = S_OPERAND;
                    optional = true;
                } else {
                    EXPECTED("opcode");
                    line_done = true;
                }

                advance = false;
            }
        } else if (state == S_OPERAND) {
            if (matchlen == 0) {
                if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                        c == '_') {
                    change = false;
                    opd_type = O_LABEL;
                    token->type = T_LABEL;
                } else if (c == '0') {
                    change = false;
                    token->type = T_NUMBER;
                    token->number = 0;
                } else if (c >= '1' && c <= '9') {
                    change = false;
                    opd_type = O_DECNUM;
                    token->type = T_NUMBER;
                    token->number = c - '0';
                } else {
                    EXPECTED("operand");
                    line_done = true;
                }
            } else if (matchlen == 1 && src_buf[start] == '0') {
                if (c >= '0' && c <= '7') {
                    opd_type = O_OCTNUM;
                    change = false;
                    token->number = c - '0';
                } else if (c == 'x') {
                    opd_type = O_HEXNUM;
                    change = false;
                } else {
                    opd_type = O_DECNUM;
                    state = S_COMMA;
                    optional = true;
                    change = true;
                }
            } else {
                if ((c >= '0' && c <= '7') || c == '_') {
                    change = false;
                    if (opd_type != O_LABEL)
                        token->number = token->number * (uint16_t)opd_type +
                            (c - '0');
                } else if ((opd_type == O_HEXNUM || opd_type == O_LABEL) &&
                        (c >= 'A' && c <= 'F')) {
                    change = false;
                    if (opd_type != O_LABEL)
                        token->number = token->number * (uint16_t)opd_type +
                            (c - 'A') + 10;
                } else if ((opd_type == O_HEXNUM || opd_type == O_LABEL) &&
                        (c >= 'a' && c <= 'f')) {
                    change = false;
                    if (opd_type != O_LABEL)
                        token->number = token->number * (uint16_t)opd_type +
                            (c - 'a') + 10;
                } else if ((opd_type == O_DECNUM || opd_type == O_HEXNUM ||
                          opd_type == O_LABEL) && (c == 8 || c == 9)) {
                    change = false;
                    if (opd_type != O_LABEL)
                        token->number = token->number * (uint16_t)opd_type +
                            (c - '0');
                } else if (opd_type == O_LABEL && ((c >= 'G' && c <= 'Z') ||
                        (c >= 'g' && c <= 'z'))) {
                    change = false;
                } else {
                    change = true;
                    advance = false;

                    char* sym = malloc(matchlen + 1);
                    memcpy(sym, &src_buf[start], matchlen);
                    sym[matchlen] = '\0';
                    printf("%s\n", sym);

                    token->text = sym;
                    ++token_idx;

                    state = S_COMMA;
                    optional = true;
                }
            }
        } else if (state == S_COMMA) {
            if (c == ',') {
                state = S_OPERAND;
                optional = false;
            } else {
                state = S_COMMENT;
                advance = false;
                optional = true;
            }

            change = true;
        } else if (state == S_COMMENT) {
            change = false;
            if (matchlen == 0 && c != ';') {
                EXPECTED("semicolon");
                advance = false;
            }
        }

        if (src_buf[*pos] == '\n') {
            line_done = true;
            advance = true;
            src_buf[*pos] = ' ';
            start = *len;
        }

        while (advance || src_buf[*pos] == ' ') {
            ++*pos;
            ++col;
            if (*pos >= *len) {
                ssize_t count = fill_buffer(src, pos, len, start);
                if (count == 0) {
                    if (!optional)
                        fatal(1, "column %u: Unexpected end of line", col);
                    line_done = true;
                    break;
                }
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
    size_t pos = 0;
    size_t len = 0;

    if (fill_buffer(src, &pos, &len, 0) == 0)
        return true;

    pos = 0;

    while (true) {
        v2("Lexing line");
        struct line line = lex_line(src, &pos, &len);
        for (unsigned int i = 0; i < lengthof(line.tokens) &&
                line.tokens[i].type != T_NONE; ++i)
            print_token(&line.tokens[i]);
        if (len == 0)
            break;
    }

    return false;
}
