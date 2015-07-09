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

#define min(x, y) ((x) < (y) ? (x) : (y))
#define max(x, y) ((x) > (y) ? (x) : (y))


char buf[CHUNK_LEN * 2 + 1];


enum token_type {
    T_TEXT,
    T_NUMBER,
    T_COLON,
    T_COMMA,
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
    if (token->type == T_TEXT)
        printf("  text: \"%s\"\n", token->text);
    else if (token->type == T_NUMBER)
        printf("  number: %"PRIX16"\n", token->number);
    else
        fatal(1, "Invalid token");
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
struct token* lex_number(struct token* token, const char* t,
        ssize_t* const toklen)
{
    if ('1' <= t[0] && t[0] <= '9') {
        token->type = T_NUMBER;
        token->number = t[0] - '0';
        for (ssize_t i = 1; i < *toklen; ++i) {
            if (t[i] <= '0' && '9' <= t[0])
                fatal(1, "Invalid decimal number");
            token->number = token->number * 10 +
                t[i] - '0';
        }
        print_token(token);
        ++token;
    } else if (t[0] == '0') {
        if (t[1] == 'b' || t[1] == 'n') {
            t += 2;
            *toklen -= 2;

            ssize_t gu = *toklen; // group upper bound
            do {
                token->type = T_NUMBER;
                token->number = 0;
                for (ssize_t i = max(0, gu - 8); i < gu; ++i) {
                    if ('0' <= t[i] && t[i] <= '1')
                        token->number = token->number * 2 +
                            t[i] - '0';
                    else
                        fatal(1, "Invalid binary number");
                }
                print_token(token);
                ++token;

                gu -= 8;
            } while (gu > 0);
        } else if ('0' <= t[1] && t[1] <= '7') {
            ++t;
            --*toklen;

            ssize_t gu = *toklen; // group upper bound
            do {
                token->type = T_NUMBER;
                token->number = 0;
                for (ssize_t i = max(0, gu - 3); i < gu; ++i) {
                    if ('0' <= t[i] && t[i] <= '7')
                        token->number = token->number * 8 +
                            t[i] - '0';
                    else
                        fatal(1, "Invalid octal number");
                }
                print_token(token);
                ++token;

                gu -= 3;
            } while (gu > 0);
        } else if (t[1] == 'x') {
            t += 2;
            *toklen -= 2;

            ssize_t gu = *toklen; // group upper bound
            do {
                token->type = T_NUMBER;
                token->number = 0;
                for (ssize_t i = max(0, gu - 2); i < gu; ++i) {
                    if ('0' <= t[i] && t[i] <= '9')
                        token->number = token->number * 16 +
                            t[i] - '0';
                    else if ('A' <= t[i] && t[i] <= 'F')
                        token->number = token->number * 16 +
                            t[i] - 'A' + 10;
                    else if ('a' <= t[i] && t[i] <= 'f')
                        token->number = token->number * 16 +
                            t[i] - 'a' + 10;
                    else
                        fatal(1, "Invalid hexadecimal number");
                }
                print_token(token);
                ++token;

                gu -= 2;
            } while (gu > 0);
        }
    }

    return token;
}


static
void lex_line(struct token* token, const int src, size_t* const bufpos,
    size_t* const buflen)
{
    size_t tokstart = *bufpos + 1;
    char c;
    ssize_t toklen;
    bool first_buf = true;
    unsigned int col = 0;

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
                    fatal(1, "col %u: Unexpected end of file", col);
            }
            first_buf = false;
        }
        ++col;
        c = buf[*bufpos];
        toklen = *bufpos - tokstart;

        //
        // Process char.
        //

        const bool is_sep = (strchr(":,; \t\n", c) != NULL);

        if (is_sep) {
            if (toklen > 0) {
                char* t = &buf[tokstart];
                if (
                        ('A' <= t[0] && t[0] <= 'Z') ||
                        ('a' <= t[0] && t[0] <= 'z') ||
                        t[0] == '_') {
                    token->type = T_TEXT;
                    token->text = malloc(toklen + 1);
                    memcpy(token->text, t, toklen);
                    token->text[toklen] = '\0';
                    print_token(token);
                    ++token;
                } else if ('0' <= t[0] && t[0] <= '9') {
                    token = lex_number(token, t, &toklen);
                } else {
                    fatal(1, "col %u: Invalid token", col);
                }
            }
        } else {
            continue;
        }

        //
        // Start new token.
        //

        tokstart = *bufpos + 1;
    } while (c != '\n');

    token->type = T_NONE;
}


bool assemble_16F1454(const int src)
{
    size_t bufpos = 0;
    size_t buflen = 1;

    while (true) {
        v2("Lexing line");
        struct line line;
        lex_line(line.tokens, src, &bufpos, &buflen);
        /*for (unsigned int i = 0; i < lengthof(line.tokens) &&*/
                /*line.tokens[i].type != T_NONE; ++i)*/
            /*print_token(&line.tokens[i]);*/
        if (buflen == 0)
            break;
    }

    return false;
}
