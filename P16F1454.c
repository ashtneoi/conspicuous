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
    T_SEMICOLON,
    T_NONE,
};


struct token {
    enum token_type type;
    char* text;
    uint8_t number;
};


static inline
void print_token(struct token* token)
{
    if (token->type == T_TEXT)
        printf("  text: \"%s\"\n", token->text);
    else if (token->type == T_NUMBER)
        printf("  number: %"PRIX16"\n", token->number);
    else if (token->type == T_COLON)
        print("  colon\n");
    else if (token->type == T_COMMA)
        print("  comma\n");
    else if (token->type == T_SEMICOLON)
        print("  semicolon\n");
    else
        fatal(1, "Invalid token");
}


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
struct token* parse_number(struct token* token, const char* t, ssize_t toklen)
{
    if ('1' <= t[0] && t[0] <= '9') {
        token->type = T_NUMBER;
        token->number = t[0] - '0';
        for (ssize_t i = 1; i < toklen; ++i) {
            if ('0' <= t[i] && t[i] <= '9')
                token->number = token->number * 10 +
                    t[i] - '0';
            else
                fatal(1, "Invalid decimal number");
        }
        ++token;
    } else if (t[0] == '0') {
        if (t[1] == 'b' || t[1] == 'n') {
            t += 2;
            toklen -= 2;

            ssize_t gu = ((toklen - 1) % 8) + 1; // group upper bound
            ssize_t i = 0;
            do {
                token->type = T_NUMBER;
                token->number = 0;
                for (/* */; i < gu; ++i) {
                    if ('0' <= t[i] && t[i] <= '1')
                        token->number = token->number * 2 +
                            t[i] - '0';
                    else
                        fatal(1, "Invalid binary number");
                }
                ++token;

                gu += 8;
            } while (gu <= toklen);
        } else if ('0' <= t[1] && t[1] <= '7') {
            ++t;
            --toklen;

            token->type = T_NUMBER;
            token->number = t[0] - '0';
            for (ssize_t i = 1; i < toklen; ++i) {
                if ('0' <= t[i] && t[i] <= '7')
                    token->number = token->number * 8 +
                        t[i] - '0';
                else
                    fatal(1, "Invalid octal number");
            }
        ++token;

        } else if (t[1] == 'x') {
            t += 2;
            toklen -= 2;

            ssize_t gu = ((toklen - 1) % 2) + 1; // group upper bound
            ssize_t i = 0;
            do {
                token->type = T_NUMBER;
                token->number = 0;
                for (/* */; i < gu; ++i) {
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
                ++token;

                gu += 2;
            } while (gu <= toklen);
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
                    ++token;
                } else if ('0' <= t[0] && t[0] <= '9') {
                    token = parse_number(token, t, toklen);
                } else {
                    fatal(1, "col %u: Invalid token", col);
                }
            }

            if (c == ':') {
                token->type = T_COLON;
                token++;
            } else if (c == ',') {
                token->type = T_COMMA;
                token++;
            } else if (c == ';') {
                token->type = T_SEMICOLON;
                token++;
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
        struct token tokens[16];
        lex_line(tokens, src, &bufpos, &buflen);
        for (unsigned int i = 0; i < lengthof(tokens) &&
                tokens[i].type != T_NONE; ++i)
            print_token(&tokens[i]);
        if (buflen == 0)
            break;
    }

    return false;
}
