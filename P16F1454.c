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
#define OPCODE_DICT_CAP 256
#define LABEL_ADDR_DICT_CAP 1024

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
    uint16_t num;
};


static inline
void print_token(struct token* token)
{
    if (token->type == T_TEXT)
        printf("  text: \"%s\"\n", token->text);
    else if (token->type == T_NUMBER)
        printf("  number: 0x%04"PRIX16"\n", token->num);
    else if (token->type == T_COLON)
        print("  colon\n");
    else if (token->type == T_COMMA)
        print("  comma\n");
    else
        fatal(1, "Invalid token");
}


enum opcode {
    C_NONE,

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

    CD_ADDWF,
    CD_ADDWFC,
    CD_ANDWF,
    CD_ASRF,
    CD_LSLF,
    CD_LSRF,
    CD_CLRF,
    CD_COMF,
    CD_DECF,
    CD_INCF,
    CD_IORWF,
    CD_MOVF,
    CD_MOVWF,
    CD_RLF,
    CD_RRF,
    CD_SUBWF,
    CD_SUBWFB,
    CD_SWAPF,
    CD_XORWF,

    CD_DECFSZ,
    CD_INCFSZ,

    CD_BCF,
    CD_BSF,

    CD_BTFSC,
    CD_BTFSS,

    CD_BRA,
    CD_CALL,
    CD_GOTO,
};


enum operand {
    N,
    F,
    B,
    K,
    D,
    // TODO: Implement N and MM.
};


struct opcode_info {
    enum opcode opc;
    const char* str;
    uint16_t word;
    enum operand opds[2];
    int kwid;
};


struct opcode_info opcode_info_list[] = {
    { .opc = C_ADDWF, .str = "addwf", .word = 0x0700, .opds = {F, D} },
    { .opc = C_ADDWFC, .str = "addwfc", .word = 0x3D00, .opds = {F, D} },
    { .opc = C_ANDWF, .str = "andwf", .word = 0x0500, .opds = {F, D} },
    { .opc = C_ASRF, .str = "asrf", .word = 0x3700, .opds = {F, D} },
    { .opc = C_LSLF, .str = "lslf", .word = 0x3500, .opds = {F, D} },
    { .opc = C_LSRF, .str = "lsrf", .word = 0x3600, .opds = {F, D} },
    { .opc = C_CLRF, .str = "clrf", .word = 0x0180, .opds = {F, N} },
    { .opc = C_CLRW, .str = "clrw", .word = 0x0100, .opds = {N, N} },
    { .opc = C_COMF, .str = "comf", .word = 0x0900, .opds = {F, D} },
    { .opc = C_DECF, .str = "decf", .word = 0x300, .opds = {F, D} },
    { .opc = C_INCF, .str = "incf", .word = 0x0A00, .opds = {F, D} },
    { .opc = C_IORWF, .str = "iorwf", .word = 0x0400, .opds = {F, D} },
    { .opc = C_MOVF, .str = "movf", .word = 0x0800, .opds = {F, D} },
    { .opc = C_MOVWF, .str = "movwf", .word = 0x0080, .opds = {F, N} },
    { .opc = C_RLF, .str = "rlf", .word = 0x0D00, .opds = {F, D} },
    { .opc = C_RRF, .str = "rrf", .word = 0x0C00, .opds = {F, D} },
    { .opc = C_SUBWF, .str = "subwf", .word = 0x0200, .opds = {F, D} },
    { .opc = C_SUBWFB, .str = "subwfb", .word = 0x3B00, .opds = {F, D} },
    { .opc = C_SWAPF, .str = "swapf", .word = 0x0E00, .opds = {F, D} },
    { .opc = C_XORWF, .str = "xorwf", .word = 0x0600, .opds = {F, D} },

    { .opc = C_DECFSZ, .str = "decfsz", .word = 0x0C00, .opds = {F, D} },
    { .opc = C_INCFSZ, .str = "incfsz", .word = 0x0F00, .opds = {F, D} },

    { .opc = C_BCF, .str = "bcf", .word = 0x1000, .opds = {F, B} },
    { .opc = C_BSF, .str = "bsf", .word = 0x1400, .opds = {F, B} },

    { .opc = C_BTFSC, .str = "btfsc", .word = 0x1800, .opds = {F, B} },
    { .opc = C_BTFSS, .str = "btfss", .word = 0x1C00, .opds = {F, B} },

    { .opc = C_ADDLW, .str = "addlw", .word = 0x3E00, .opds = {K, N},
        .kwid = 8 },
    { .opc = C_ANDLW, .str = "andlw", .word = 0x3900, .opds = {K, N},
        .kwid = 8 },
    { .opc = C_IORLW, .str = "iorlw", .word = 0x3800, .opds = {K, N},
        .kwid = 8 },
    { .opc = C_MOVLB, .str = "movlb", .word = 0x0020, .opds = {K, N},
        .kwid = 5 },
    { .opc = C_MOVLP, .str = "movlp", .word = 0x3180, .opds = {K, N},
        .kwid = 7 },
    { .opc = C_MOVLW, .str = "movlw", .word = 0x3000, .opds = {K, N},
        .kwid = 8 },
    { .opc = C_SUBLW, .str = "sublw", .word = 0x3C00, .opds = {K, N},
        .kwid = 8 },
    { .opc = C_XORLW, .str = "xorlw", .word = 0x3A00, .opds = {K, N},
        .kwid = 8 },

    { .opc = C_BRA, .str = "bra", .word = 0x3200, .opds = {K, N},
        .kwid = 9 },
    { .opc = C_BRW, .str = "brw", .word = 0x000B, .opds = {N, N} },
    { .opc = C_CALL, .str = "call", .word = 0x2000, .opds = {K, N},
        .kwid = 11 },
    { .opc = C_CALLW, .str = "callw", .word = 0x000A, .opds = {N, N} },
    { .opc = C_GOTO, .str = "goto", .word = 0x2800, .opds = {K, N},
        .kwid = 11 },
    { .opc = C_RETFIE, .str = "retfie", .word = 0x0009, .opds = {N, N} },
    { .opc = C_RETLW, .str = "retlw", .word = 0x3400, .opds = {K, N},
        .kwid = 8 },
    { .opc = C_RETURN, .str = "return", .word = 0x0008, .opds = {N, N} },

    { .opc = C_CLRWDT, .str = "clrwdt", .word = 0x0064, .opds = {N, N} },
    { .opc = C_NOP, .str = "nop", .word = 0x0000, .opds = {N, N} },
    { .opc = C_OPTION, .str = "option", .word = 0x0062, .opds = {N, N} },
    { .opc = C_RESET, .str = "reset", .word = 0x0001, .opds = {N, N} },
    { .opc = C_SLEEP, .str = "sleep", .word = 0x0063, .opds = {N, N} },
    { .opc = C_TRIS, .str = "tris", .word = 0x0060, .opds = {F, N} },

    { .opc = CD_ADDWF, .str = ".addwf", .word = 0x0700, .opds = {F, D} },
    { .opc = CD_ADDWFC, .str = ".addwfc", .word = 0x3D00, .opds = {F, D} },
    { .opc = CD_ANDWF, .str = ".andwf", .word = 0x0500, .opds = {F, D} },
    { .opc = CD_ASRF, .str = ".asrf", .word = 0x3700, .opds = {F, D} },
    { .opc = CD_LSLF, .str = ".lslf", .word = 0x3500, .opds = {F, D} },
    { .opc = CD_LSRF, .str = ".lsrf", .word = 0x3600, .opds = {F, D} },
    { .opc = CD_CLRF, .str = ".clrf", .word = 0x0180, .opds = {F, N} },
    { .opc = CD_COMF, .str = ".comf", .word = 0x0900, .opds = {F, D} },
    { .opc = CD_DECF, .str = ".decf", .word = 0x300, .opds = {F, D} },
    { .opc = CD_INCF, .str = ".incf", .word = 0x0A00, .opds = {F, D} },
    { .opc = CD_IORWF, .str = ".iorwf", .word = 0x0400, .opds = {F, D} },
    { .opc = CD_MOVF, .str = ".movf", .word = 0x0800, .opds = {F, D} },
    { .opc = CD_MOVWF, .str = ".movwf", .word = 0x0080, .opds = {F, N} },
    { .opc = CD_RLF, .str = ".rlf", .word = 0x0D00, .opds = {F, D} },
    { .opc = CD_RRF, .str = ".rrf", .word = 0x0C00, .opds = {F, D} },
    { .opc = CD_SUBWF, .str = ".subwf", .word = 0x0200, .opds = {F, D} },
    { .opc = CD_SUBWFB, .str = ".subwfb", .word = 0x3B00, .opds = {F, D} },
    { .opc = CD_SWAPF, .str = ".swapf", .word = 0x0E00, .opds = {F, D} },
    { .opc = CD_XORWF, .str = ".xorwf", .word = 0x0600, .opds = {F, D} },

    { .opc = CD_DECFSZ, .str = ".decfsz", .word = 0x0C00, .opds = {F, D} },
    { .opc = CD_INCFSZ, .str = ".incfsz", .word = 0x0F00, .opds = {F, D} },

    { .opc = CD_BCF, .str = ".bcf", .word = 0x1000, .opds = {F, B} },
    { .opc = CD_BSF, .str = ".bsf", .word = 0x1400, .opds = {F, B} },

    { .opc = CD_BTFSC, .str = ".btfsc", .word = 0x1800, .opds = {F, B} },
    { .opc = CD_BTFSS, .str = ".btfss", .word = 0x1C00, .opds = {F, B} },

    { .opc = CD_BRA, .str = ".bra", .word = 0x3200, .opds = {K, N},
        .kwid = 9 },
    { .opc = CD_CALL, .str = ".call", .word = 0x2000, .opds = {K, N},
        .kwid = 11 },
    { .opc = CD_GOTO, .str = ".goto", .word = 0x2800, .opds = {K, N},
        .kwid = 11 },
};


struct opcode_info* opcode_info[2 * OPCODE_DICT_CAP];


// djb2 by Dan Bernstein
static unsigned long hash(const char* str)
{
    unsigned long hash = 5381;
    int c;

    while ((c = *(str++)))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}


static
void init_opcode_dict()
{
    for (unsigned int i = 0; i < lengthof(opcode_info); ++i)
        opcode_info[i] = NULL;

    for (unsigned int i = 0; i < lengthof(opcode_info_list); ++i) {
        unsigned int h = hash(opcode_info_list[i].str) % OPCODE_DICT_CAP;
        while (opcode_info[h] != NULL)
            ++h;
        opcode_info[h] = &opcode_info_list[i];
    }
}


struct label_addr {
    char* label;
    unsigned int addr;
} label_addrs[2 * LABEL_ADDR_DICT_CAP];


struct insn {
    char* label;
    enum opcode opc;
    unsigned int f; // register file address
    unsigned int b; // bit number
    char* b_str;
    int k; // literal
    char* k_lbl;
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
        token->num = t[0] - '0';
        for (ssize_t i = 1; i < toklen; ++i) {
            if ('0' <= t[i] && t[i] <= '9')
                token->num = token->num * 10 +
                    t[i] - '0';
            else
                fatal(1, "Invalid decimal number");
        }
        ++token;
    } else if (t[0] == '0') {
        token->type = T_NUMBER;
        token->num = 0;

        if (toklen == 1) {
            ++token;
            return token;
        } else if (t[1] == 'b' || t[1] == 'n') {
            t += 2;
            toklen -= 2;

            token->type = T_NUMBER;
            token->num = 0;
            for (ssize_t i = 0; i < toklen; ++i) {
                if ('0' <= t[i] && t[i] <= '1')
                    token->num = token->num * 2 +
                        t[i] - '0';
                else
                    fatal(1, "Invalid binary number");
            }
            ++token;
        } else if ('0' <= t[1] && t[1] <= '7') {
            ++t;
            --toklen;

            token->type = T_NUMBER;
            token->num = t[0] - '0';
            for (ssize_t i = 1; i < toklen; ++i) {
                if ('0' <= t[i] && t[i] <= '7')
                    token->num = token->num * 8 +
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
                    token->num = token->num * 16 +
                        t[i] - '0';
                else if ('A' <= t[i] && t[i] <= 'F')
                    token->num = token->num * 16 +
                        t[i] - 'A' + 10;
                else if ('a' <= t[i] && t[i] <= 'f')
                    token->num = token->num * 16 +
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
                token->num = 0;
                for (/* */; i < gu; ++i) {
                    if ('0' <= t[i] && t[i] <= '1')
                        token->num = token->num * 2 +
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
                token->num = 0;
                for (/* */; i < gu; ++i) {
                    if ('0' <= t[i] && t[i] <= '9')
                        token->num = token->num * 16 +
                            t[i] - '0';
                    else if ('A' <= t[i] && t[i] <= 'F')
                        token->num = token->num * 16 +
                            t[i] - 'A' + 10;
                    else if ('a' <= t[i] && t[i] <= 'f')
                        token->num = token->num * 16 +
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
                        t[0] == '.' || t[0] == '_') {
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
    if (token[0].type == T_NONE)
        return NULL;

    struct insn* insn = malloc(sizeof(struct insn));
    if (prev_insn != NULL)
        prev_insn->next = insn;

    if (token->type != T_TEXT)
        fatal(1, "line %u: Expected label or opcode", l);

    if (token[1].type == T_COLON) {
        insn->label = token->text;
        token += 2;
        if (token->type == T_NONE)
            return insn;
        else if (token->type != T_TEXT)
            fatal(1, "line %u: Expected opcode", l);
    } else {
        insn->label = NULL;
    }

    struct opcode_info* oi;
    {
        unsigned int h = hash(token->text) % OPCODE_DICT_CAP;
        while (true) {
            if (h >= lengthof(opcode_info) || opcode_info[h] == NULL)
                fatal(1, "line %u: Invalid opcode", l);
            if (strcmp(token->text, opcode_info[h]->str) == 0)
                break;
            ++h;
        }

        oi = opcode_info[h];
        ++token;
    }
    insn->opc = oi->opc;

    for (unsigned int i = 0; i < 2 && oi->opds[i] != N; ++i) {
        if (i == 1) {
            if (token->type != T_COMMA) {
                if (oi->opds[1] == D) {
                    insn->d = 1;
                    break;
                } else {
                    fatal(1, "line %u: Expected comma", l);
                }
            }
            ++token;
        }

        if (oi->opds[i] == F) {
            if (token->type != T_NUMBER)
                fatal(1, "line %u: Expected register", l);
            else if (token->num >= 1<<8 ||
                    (oi->opc == C_TRIS && (token->num < 5 || 7 < token->num)))
                fatal(1, "line %u: Address out of range", l);
            insn->f = token->num;
        } else if (oi->opds[i] == B) {
            if (token->type != T_NUMBER)
                fatal(1, "line %u: Expected bit number", l);
            else if (token->num > 7)
                fatal(1, "line %u: Bit number out of range", l);
            insn->b = token->num;
        } else if (oi->opds[i] == K) {
            if (token->type == T_NUMBER) {
                if (token->num >= 1<<oi->kwid)
                    fatal(1, "line %u: Literal out of range", l);
                insn->k = token->num;
                insn->k_lbl = NULL;
            } else if (token->type == T_TEXT) {
                insn->k_lbl = token->text;
            } else {
                fatal(1, "line %u: Expected constant or label", l);
            }
        } else if (oi->opds[i] == D) {
            if (token->type != T_NUMBER)
                fatal(1, "line %u: Expected destination select", l);
            else if (token->num > 1)
                fatal(1, "line %u: Destination select out of range", l);
            insn->d = token->num;
        }

        ++token;
    }

    if (token->type != T_NONE)
        fatal(1, "line %u: Trailing tokens", l);

    return insn;
}


static
uint16_t assemble_insn(struct insn* insn, unsigned int addr)
{
    (void)addr;

    enum opcode opc = insn->opc;

    uint16_t word = opcode_info_list[opc].word;

    if (
            opc == C_ADDWF || opc == C_ADDWFC ||
            opc == C_ANDWF || opc == C_LSLF ||
            opc == C_LSRF || opc == C_CLRF ||
            opc == C_COMF || opc == C_DECF ||
            opc == C_INCF || opc == C_IORWF ||
            opc == C_MOVF || opc == C_MOVWF ||
            opc == C_RLF || opc == C_RRF ||
            opc == C_SUBWF || opc == C_SUBWFB ||
            opc == C_SWAPF || opc == C_XORWF ||
            opc == C_DECFSZ || opc == C_INCFSZ) {
        if (opc == C_CLRF || opc == C_MOVWF || insn->d)
            word |= 0x0080;
        word |= insn->f;
    } else if (
            opc == C_BCF || opc == C_BSF ||
            opc == C_BTFSC || opc == C_BTFSS) {
        word |= (insn->b << 7) | insn->f;
    } else if (
            opc == C_ADDLW || opc == C_ANDLW ||
            opc == C_IORLW || opc == C_MOVLB ||
            opc == C_MOVLP || opc == C_MOVLW ||
            opc == C_SUBLW || opc == C_XORLW ||
            opc == C_BRA || opc == C_CALL ||
            opc == C_GOTO || opc == C_RETLW) {
        if (insn->k_lbl != NULL) {
            unsigned int h = hash(insn->k_lbl) % LABEL_ADDR_DICT_CAP;
            while (strcmp(label_addrs[h].label, insn->k_lbl) != 0
                    && h < lengthof(label_addrs))
                ++h;
            if (h >= lengthof(label_addrs))
                fatal(1, "Label address dict is full");
            insn->k = label_addrs[h].addr;
        }

        word |= insn->k;
    } else if (opc == C_TRIS) {
        word |= insn->f;
    }

    return word;
}


void dump_hex(struct insn* start, const int out)
{
    (void)out;

    for (unsigned int i = 0; i < lengthof(label_addrs); ++i)
        label_addrs[i].label = NULL;

    unsigned int addr;
    struct insn* insn;

    for (insn = start, addr = 0; insn != NULL; insn = insn->next, ++addr) {
        if (insn->label == NULL)
            continue;
        printf("label %s = %d\n", insn->label, addr);
        unsigned int h = hash(insn->label) % LABEL_ADDR_DICT_CAP;
        while (label_addrs[h].label != NULL && h < lengthof(label_addrs))
            ++h;
        label_addrs[h].label = insn->label;
        label_addrs[h].addr = addr;
    }

    int bank = -1;
    for (insn = start, addr = 0; insn != NULL; insn = insn->next, ++addr) {
        if (insn->label != NULL)
            bank = -1;
        else if (insn->opc == C_MOVLB)
            bank = insn->k;
        printf("0x%04"PRIX16"\n", assemble_insn(insn, addr));
        printf("  bank = %d\n", bank);
    }

    /*while (true) {*/
        /*char data[16 * 2];*/
        /*unsigned int i;*/
        /*for (i = 0; i < 16 * 2; i += 2, ++addr, insn = insn->next) {*/
            /*if (insn == NULL) {*/
                /*fputs(":00000001FF\n", out);*/
                /*return;*/
            /*}*/
            /*sprintf(&data[i], "%02X", assemble_insn(*/

}


bool assemble_16F1454(const int src)
{
    size_t bufpos = 0;
    size_t buflen = 1;

    struct insn* start = NULL;
    struct insn* prev_insn = NULL;

    init_opcode_dict();

    for (unsigned int l = 1; /* */; ++l) {
        v2("Lexing line");
        struct token tokens[16];
        lex_line(tokens, src, &bufpos, &buflen);
        for (unsigned int i = 0; i < lengthof(tokens) &&
                tokens[i].type != T_NONE; ++i)
            print_token(&tokens[i]);
        if (buflen == 0)
            break;
        struct insn* insn = parse_line(prev_insn, tokens, l);
        if (insn != NULL) {
            if (start == NULL)
                start = insn;
            prev_insn = insn;
        }
    }

    dump_hex(start, 0);

    return false;
}
