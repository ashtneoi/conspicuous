#include "P16F1454.h"

#include "bufman.h"
#include "fail.h"
#include "cpic.h"
#include "utils.h"

#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
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
    else if (token->type == T_COLON)
        print("  colon\n");
    else if (token->type == T_COMMA)
        print("  comma\n");
    else
        fatal(1, "Invalid token");
}


enum opcode {
    C_ADDWF,
    C_ADDWFC,
    C_ANDWF,
    C_ASRF,
    C_LSLF,
    C_LSRF,
    C_CLRF,
    C_CLRW,
    C_COMF,
    C_DECF,
    C_INCF,
    C_IORWF,
    C_MOVF,
    C_MOVWF,
    C_RLF,
    C_RRF,
    C_SUBWF,
    C_SUBWFB,
    C_SWAPF,
    C_XORWF,

    C_DECFSZ,
    C_INCFSZ,

    C_BCF,
    C_BSF,

    C_BTFSC,
    C_BTFSS,

    C_ADDLW,
    C_ANDLW,
    C_IORLW,
    C_MOVLB,
    C_MOVLP,
    C_MOVLW,
    C_SUBLW,
    C_XORLW,

    C_BRA,
    C_BRW,
    C_CALL,
    C_CALLW,
    C_GOTO,
    C_RETFIE,
    C_RETLW,
    C_RETURN,

    C_CLRWDT,
    C_NOP,
    C_OPTION,
    C_RESET,
    C_SLEEP,
    C_TRIS,

    // TODO: implement ADDFSR, MOVIW, and MOVWI
};


struct insn {
    char* label;
    enum opcode opcode;
    unsigned int f; // register file address
    unsigned int b; // bit number
    char* b_str;
    int k; // literal
    char* k_str;
    unsigned int d; // destination select (0 = W, 1 = f)
    unsigned int n; // FSR or INDF number
    unsigned int mm; // pre-/post-decrement/-increment select

    struct insn* next;
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
        token->type = T_NUMBER;
        token->number = 0;

        if (toklen == 1) {
            ++token;
            return token;
        } else if (t[1] == 'b' || t[1] == 'n') {
            t += 2;
            toklen -= 2;

            token->type = T_NUMBER;
            token->number = 0;
            for (ssize_t i = 0; i < toklen; ++i) {
                if ('0' <= t[i] && t[i] <= '1')
                    token->number = token->number * 2 +
                        t[i] - '0';
                else
                    fatal(1, "Invalid binary number");
            }
            ++token;
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

            for (ssize_t i = 0; i < toklen; ++i) {
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
        } else {
            fatal(1, "Invalid number");
        }
    } else if (t[0] == '#' && t[1] == '0') {
        if (t[2] == 'b' || t[2] == 'n') {
            t += 3;
            toklen -= 3;

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
        } else if (t[2] == 'x') {
            t += 3;
            toklen -= 3;

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
        } else {
            fatal(1, "Invalid number");
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
    bool ignore = false;

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
        c = buf[*bufpos];

        if (ignore)
            continue;

        ++col;
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
                } else if (t[0] == '#' || ('0' <= t[0] && t[0] <= '9')) {
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
                ignore = true;
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


struct insn* parse_line(struct insn* const prev_insn,
        const struct token* token, unsigned int l)
{
    struct insn* insn = malloc(sizeof(struct insn));
    prev_insn->next = insn;

    if (token[0].type != T_TEXT)
        fatal(1, "line %u: Expected label or opcode", l);

    if (token[1].type == T_COLON) {
        insn->label = token[0].text;
        token += 2;
        if (token->type != T_TEXT)
            fatal(1, "line %u: Expected opcode", l);
    } else {
        insn->label = NULL;
    }

    enum opcode opc = insn->opcode;
    enum {
        F,
        B,
        K,
        D,
        X,
        // TODO: Implement N and MM.
    } opd[2] = {X, X};


    if (strcasecmp(token->text, "addwf") == 0) {
        opc = C_ADDWF;
        opd[0] = F;
        opd[1] = D;
    } else if (strcasecmp(token->text, "addwfc") == 0) {
        opc = C_ADDWFC;
        opd[0] = F;
        opd[1] = D;
    } else if (strcasecmp(token->text, "andwf") == 0) {
        opc = C_ANDWF;
        opd[0] = F;
        opd[1] = D;
    } else if (strcasecmp(token->text, "asrf") == 0) {
        opc = C_ASRF;
        opd[0] = F;
        opd[1] = D;
    } else if (strcasecmp(token->text, "lslf") == 0) {
        opc = C_LSLF;
        opd[0] = F;
        opd[1] = D;
    } else if (strcasecmp(token->text, "clrf") == 0) {
        opc = C_CLRF;
        opd[0] = F;
    } else if (strcasecmp(token->text, "clrw") == 0) {
        opc = C_CLRW;
    } else if (strcasecmp(token->text, "comf") == 0) {
        opc = C_COMF;
        opd[0] = F;
        opd[1] = D;
    } else if (strcasecmp(token->text, "decf") == 0) {
        opc = C_DECF;
        opd[0] = F;
        opd[1] = D;
    } else if (strcasecmp(token->text, "incf") == 0) {
        opc = C_INCF;
        opd[0] = F;
        opd[1] = D;
    } else if (strcasecmp(token->text, "iorwf") == 0) {
        opc = C_IORWF;
        opd[0] = F;
        opd[1] = D;
    } else if (strcasecmp(token->text, "movf") == 0) {
        opc = C_MOVF;
        opd[0] = F;
        opd[1] = D;
    } else if (strcasecmp(token->text, "movwf") == 0) {
        opc = C_MOVWF;
        opd[0] = F;
    } else if (strcasecmp(token->text, "rlf") == 0) {
        opc = C_RLF;
        opd[0] = F;
        opd[1] = D;
    } else if (strcasecmp(token->text, "rrf") == 0) {
        opc = C_RRF;
        opd[0] = F;
        opd[1] = D;
    } else if (strcasecmp(token->text, "subwf") == 0) {
        opc = C_SUBWF;
        opd[0] = F;
        opd[1] = D;
    } else if (strcasecmp(token->text, "subwfb") == 0) {
        opc = C_SUBWFB;
        opd[0] = F;
        opd[1] = D;
    } else if (strcasecmp(token->text, "swapf") == 0) {
        opc = C_SWAPF;
        opd[0] = F;
        opd[1] = D;
    } else if (strcasecmp(token->text, "xorwf") == 0) {
        opc = C_XORWF;
        opd[0] = F;
        opd[1] = D;
    } else if (strcasecmp(token->text, "decfsz") == 0) {
        opc = C_DECFSZ;
        opd[0] = F;
        opd[1] = D;
    } else if (strcasecmp(token->text, "incfsz") == 0) {
        opc = C_INCFSZ;
        opd[0] = F;
        opd[1] = D;
    } else if (strcasecmp(token->text, "bcf") == 0) {
        opc = C_BCF;
        opd[0] = F;
        opd[1] = B;
    } else if (strcasecmp(token->text, "bsf") == 0) {
        opc = C_BSF;
        opd[0] = F;
        opd[1] = D;
    } else if (strcasecmp(token->text, "btfsc") == 0) {
        opc = C_BTFSC;
        opd[0] = F;
        opd[1] = D;
    } else if (strcasecmp(token->text, "btfss") == 0) {
        opc = C_BTFSS;
        opd[0] = F;
        opd[1] = D;
    } else if (strcasecmp(token->text, "addlw") == 0) {
        opc = C_ADDLW;
        opd[0] = K;
    } else if (strcasecmp(token->text, "andlw") == 0) {
        opc = C_ANDLW;
        opd[0] = F;
    } else if (strcasecmp(token->text, "iorlw") == 0) {
        opc = C_IORLW;
        opd[0] = F;
    } else if (strcasecmp(token->text, "movlb") == 0) {
        opc = C_MOVLB;
        opd[0] = F;
    } else if (strcasecmp(token->text, "movlp") == 0) {
        opc = C_MOVLP;
        opd[0] = F;
    } else if (strcasecmp(token->text, "movlw") == 0) {
        opc = C_MOVLW;
        opd[0] = F;
    } else if (strcasecmp(token->text, "sublw") == 0) {
        opc = C_SUBLW;
        opd[0] = F;
    } else if (strcasecmp(token->text, "xorlw") == 0) {
        opc = C_XORLW;
        opd[0] = F;
    } else if (strcasecmp(token->text, "bra") == 0) {
        opc = C_BRA;
        opd[0] = F;
    } else if (strcasecmp(token->text, "brw") == 0) {
        opc = C_BRW;
    } else if (strcasecmp(token->text, "call") == 0) {
        opc = C_CALL;
        opd[0] = F;
    } else if (strcasecmp(token->text, "callw") == 0) {
        opc = C_CALLW;
    } else if (strcasecmp(token->text, "goto") == 0) {
        opc = C_GOTO;
        opd[0] = F;
    } else if (strcasecmp(token->text, "retfie") == 0) {
        opc = C_RETFIE;
        opd[0] = F;
    } else if (strcasecmp(token->text, "retlw") == 0) {
        opc = C_RETLW;
        opd[0] = F;
    } else if (strcasecmp(token->text, "return") == 0) {
        opc = C_RETURN;
    } else if (strcasecmp(token->text, "clrwdt") == 0) {
        opc = C_CLRWDT;
    } else if (strcasecmp(token->text, "nop") == 0) {
        opc = C_NOP;
    } else if (strcasecmp(token->text, "option") == 0) {
        opc = C_OPTION;
    } else if (strcasecmp(token->text, "reset") == 0) {
        opc = C_RESET;
    } else if (strcasecmp(token->text, "sleep") == 0) {
        opc = C_SLEEP;
    } else if (strcasecmp(token->text, "tris") == 0) {
        opc = C_TRIS;
        opd[0] = F;
    } else {
        fatal(1, "line %u: Invalid opcode", l);
    }

    insn->opcode = opc;
    ++token;

    for (unsigned int i = 0; i < 2 && opd[i] != X; ++i) {
        if (i == 1) {
            if (token->type != T_COMMA) {
                if (opd[1] == D)
                    break;
                else
                    fatal(1, "line %u: Expected comma", l);
            }
            ++token;
        }

        if (opd[i] == F) {
            if (token->type != T_NUMBER)
                fatal(1, "line %u: Expected register", l);
            insn->f = token->number;
        } else if (opd[i] == B) {
            if (token->type != T_NUMBER)
                fatal(1, "line %u: Expected bit number", l);
            else if (token->number > 7)
                fatal(1, "line %u: Bit number out of range", l);
            insn->b = token->number;
        } else if (opd[i] == K) {
            if (token->type != T_NUMBER)
                fatal(1, "line %u: Expected constant", l);
            insn->k = token->number;
        } else if (opd[i] == D) {
            if (token->type != T_NUMBER)
                fatal(1, "line %u: Expected destination select", l);
            else if (token->number > 1)
                fatal(1, "line %u: Destination select out of range", l);
            insn->d = token->number;
        }

        ++token;
    }

    if (token->type != T_NONE)
        fatal(1, "line %u: Trailing tokens", l);

        /*
         *if (token[1].type != T_NUMBER)
         *    fatal(1, "line %u: Expected register", l);
         *insn.f = token[1].number;
         *if (token[2].type != T_COMMA)
         *    fatal(1, "line %u: Expected comma", l);
         *if (token[3].type != T_NUMBER)
         *    fatal(1, "line %u: Expected number", l);
         *insn.d = token[3].number;
         */

    return insn;
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
