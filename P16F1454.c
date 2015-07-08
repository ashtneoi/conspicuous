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
    (void)src;
    (void)pos;
    (void)len;
    unsigned int token_idx = 0;
    struct line line;
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
