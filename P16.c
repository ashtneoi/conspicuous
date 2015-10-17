#include "P16.h"

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
#define OPCODE_INFO_CAP 256
#define LABEL_INFO_CAP 1024
#define REG_INFO_CAP 1024

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

    CS_GOTO,

    CD_REG,
    CD_SREG,

    // TODO: Implement more.
};


enum operand {
    N, // none
    F, // register address (0x0..0x7F)
    FN, // register name
    B, // bit number (0..7)
    K, // miscellaneous constant
    KL, // label name
    D, // destination select (0,1)
    T, // TRIS operand (5..7)

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
    { .opc = C_TRIS, .str = "tris", .word = 0x0060, .opds = {T, N} },

    { .opc = CD_ADDWF, .str = ".addwf", .word = 0x0700, .opds = {FN, D} },
    { .opc = CD_ADDWFC, .str = ".addwfc", .word = 0x3D00, .opds = {FN, D} },
    { .opc = CD_ANDWF, .str = ".andwf", .word = 0x0500, .opds = {FN, D} },
    { .opc = CD_ASRF, .str = ".asrf", .word = 0x3700, .opds = {FN, D} },
    { .opc = CD_LSLF, .str = ".lslf", .word = 0x3500, .opds = {FN, D} },
    { .opc = CD_LSRF, .str = ".lsrf", .word = 0x3600, .opds = {FN, D} },
    { .opc = CD_CLRF, .str = ".clrf", .word = 0x0180, .opds = {FN, N} },
    { .opc = CD_COMF, .str = ".comf", .word = 0x0900, .opds = {FN, D} },
    { .opc = CD_DECF, .str = ".decf", .word = 0x300, .opds = {FN, D} },
    { .opc = CD_INCF, .str = ".incf", .word = 0x0A00, .opds = {FN, D} },
    { .opc = CD_IORWF, .str = ".iorwf", .word = 0x0400, .opds = {FN, D} },
    { .opc = CD_MOVF, .str = ".movf", .word = 0x0800, .opds = {FN, D} },
    { .opc = CD_MOVWF, .str = ".movwf", .word = 0x0080, .opds = {FN, N} },
    { .opc = CD_RLF, .str = ".rlf", .word = 0x0D00, .opds = {FN, D} },
    { .opc = CD_RRF, .str = ".rrf", .word = 0x0C00, .opds = {FN, D} },
    { .opc = CD_SUBWF, .str = ".subwf", .word = 0x0200, .opds = {FN, D} },
    { .opc = CD_SUBWFB, .str = ".subwfb", .word = 0x3B00, .opds = {FN, D} },
    { .opc = CD_SWAPF, .str = ".swapf", .word = 0x0E00, .opds = {FN, D} },
    { .opc = CD_XORWF, .str = ".xorwf", .word = 0x0600, .opds = {FN, D} },

    { .opc = CD_DECFSZ, .str = ".decfsz", .word = 0x0C00, .opds = {FN, D} },
    { .opc = CD_INCFSZ, .str = ".incfsz", .word = 0x0F00, .opds = {FN, D} },

    { .opc = CD_BCF, .str = ".bcf", .word = 0x1000, .opds = {FN, B} },
    { .opc = CD_BSF, .str = ".bsf", .word = 0x1400, .opds = {FN, B} },

    { .opc = CD_BTFSC, .str = ".btfsc", .word = 0x1800, .opds = {FN, B} },
    { .opc = CD_BTFSS, .str = ".btfss", .word = 0x1C00, .opds = {FN, B} },

    { .opc = CD_BRA, .str = ".bra", .word = 0x3200, .opds = {KL, N},
        .kwid = 9 },
    { .opc = CD_CALL, .str = ".call", .word = 0x2000, .opds = {KL, N},
        .kwid = 11 },

    { .opc = CS_GOTO, .str = "$goto$", .word = 0x2800, .opds = {K, N},
        .kwid = 11 },

    { .opc = CD_REG, .str = ".reg"},
    { .opc = CD_SREG, .str = ".sreg"},
};


struct opcode_info* opcode_info[2 * OPCODE_INFO_CAP];


struct reg_info {
    const char* name;
    unsigned int bank;
    unsigned int addr;
} reg_info[2 * REG_INFO_CAP];


struct label_info {
    const char* name;
    unsigned int addr;
} label_info[2 * LABEL_INFO_CAP];


union line {
    struct {
        struct opcode_info* oi;
        union line* next;

        const char* label;
        unsigned int f; // register file address
        char* f_name;
        unsigned int b; // bit number
        char* b_str;
        int k; // literal
        const char* k_lbl;
        unsigned int d; // destination select (0 = W, 1 = f)
        //unsigned int n; // FSR or INDF number
        //unsigned int mm; // pre-/post-decrement/-increment select
    } i;
    struct {
        struct opcode_info* oi;
        union line* next;

        const char* name;
        int addr1;
        int addr2;
    } d;
};


void print_line(union line* line)
{
    if (line->i.oi->opc == C_NONE)
        return;

    struct opcode_info* oi = line->i.oi;

    if (C_ADDWF <= oi->opc && oi->opc <= CS_GOTO) {
        if (line->i.label != NULL)
            printf("%s: ", line->i.label);
        print(oi->str);
        for (unsigned int i = 0; i < 2; ++i) {
            if (i == 0 && oi->opds[0] != N)
                putchar(' ');
            else if (i == 1 && oi->opds[1] != N)
                print(", ");

            switch (oi->opds[i]) {
                case N:
                    return;
                case F:
                    printf("0x%02X", line->i.f);
                    break;
                case FN:
                    print(line->i.f_name);
                    break;
                case B:
                    printf("%d", line->i.b);
                    break;
                case K:
                    printf("0x%02X", line->i.k);
                    break;
                case KL:
                    print(line->i.k_lbl);
                    break;
                case D:
                    putchar(line->i.d + '0');
                    break;
                case T:
                    fatal(2, "Not implemented");
            }
        }
    } else {
        fatal(2, "Not implemented");
    }
}


// djb2 by Dan Bernstein
static
unsigned long hash(const char* str)
{
    unsigned long hash = 5381;
    int c;

    while ((c = *(str++)))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}


static
void opcode_info_init()
{
    for (unsigned int i = 0; i < lengthof(opcode_info); ++i)
        opcode_info[i] = NULL;

    for (unsigned int i = 0; i < lengthof(opcode_info_list); ++i) {
        unsigned int h = hash(opcode_info_list[i].str) % OPCODE_INFO_CAP;
        while (opcode_info[h] != NULL)
            ++h;
        opcode_info[h] = &opcode_info_list[i];
    }
}


static
struct opcode_info* opcode_info_get(const char* const opcode)
{
    unsigned int h = hash(opcode) % OPCODE_INFO_CAP;
    while (true) {
        if (h >= lengthof(opcode_info) || opcode_info[h] == NULL)
            return NULL;
        if (strcmp(opcode, opcode_info[h]->str) == 0)
            break;
        ++h;
    }

    return opcode_info[h];
}


static
void label_info_init()
{
    for (unsigned int i = 0; i < lengthof(label_info); ++i)
        label_info[i].name = NULL;
}


static
struct label_info* label_info_avail(const char* const label)
{
    unsigned int h = hash(label) % LABEL_INFO_CAP;
    while (label_info[h].name != NULL) {
        ++h;
        if (h >= lengthof(label_info))
            fatal(1, "Label dict is full");
    }
    return &label_info[h];
}


static
struct label_info* label_info_get(const char* const name)
{
    unsigned int h = hash(name) % LABEL_INFO_CAP;
    while (true) {
        if (h >= lengthof(label_info) || label_info[h].name == NULL)
            return NULL;
        if (strcmp(label_info[h].name, name) == 0)
            break;
        ++h;
    }

    return &label_info[h];
}


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


static
union line* parse_line(union line* const prev_line,
        const struct token* token, unsigned int l, char** const label)
{
    if (token[0].type == T_NONE)
        return prev_line;

    if (token->type != T_TEXT)
        fatal(1, "line %u: Expected label or opcode", l);

    v2("Setting label");
    if (token[1].type == T_COLON) {
        if (*label != NULL)
            fatal(1, "line %u: Instruction already has a label", l);
        *label = token->text;
        token += 2;
        if (token->type == T_NONE) {
            v2("Carrying label to next line");
            return prev_line;
        } else if (token->type != T_TEXT) {
            fatal(1, "line %u: Expected opcode", l);
        }
    }

    union line* line = malloc(sizeof(union line));
    line->i.next = NULL;
    if (prev_line != NULL)
        prev_line->i.next = line;

    line->i.label = *label;
    *label = NULL;

    struct opcode_info* oi = opcode_info_get(token->text);
    if (oi == NULL)
        fatal(1, "line %u: Invalid opcode \"%s\"", l, token->text);
    ++token;

    line->i.oi = oi;

    if (oi->opc == CD_REG) {
        if (token->type != T_TEXT)
            fatal(1, "line %u: Expected register name", l);
        line->d.name = token->text;
        ++token;
        if (token->type == T_NONE)
            line->d.addr1 = -1;
        else if (token->type != T_COMMA)
            fatal(1, "line %u: Expected comma or end of line", l);
        ++token;
        if (token->type != T_NUMBER)
            fatal(1, "line %u: Expected traditional register address", l);
        line->d.addr1 = token->num;
        ++token;
    } else if (oi->opc == CD_SREG) {
        if (token->type != T_TEXT)
            fatal(1, "line %u: Expected register name", l);
        line->d.name = token->text;
        line->d.addr1 = -1;
    } else for (unsigned int i = 0; i < 2 && oi->opds[i] != N; ++i) {
        if (i == 1) {
            if (token->type != T_COMMA) {
                if (oi->opds[1] == D) {
                    line->i.d = 1;
                    break;
                } else {
                    fatal(1, "line %u: Expected comma", l);
                }
            }
            ++token;
        }

        if (oi->opds[i] == F) {
            if (token->type != T_NUMBER)
                fatal(1, "line %u: Expected register address", l);
            else if (token->num > 0x7F)
                fatal(1, "line %u: Address 0x%"PRIX16" out of range", l,
                    token->num);
            line->i.f = token->num;
            line->i.f_name = NULL;
        } else if (oi->opds[i] == FN) {
            if (token->type != T_TEXT)
                fatal(1, "line %u: Expected register name", l);
            line->i.f_name = token->text;
        } else if (oi->opds[i] == B) {
            if (token->type != T_NUMBER)
                fatal(1, "line %u: Expected bit number", l);
            else if (token->num > 7)
                fatal(1, "line %u: Bit number out of range", l);
            line->i.b = token->num;
        } else if (oi->opds[i] == K) {
            if (token->type != T_NUMBER)
                fatal(1, "line %u: Expected constant", l);
            else if (token->num >= 1<<oi->kwid)
                fatal(1, "line %u: Literal out of range", l);
            line->i.k = token->num;
            line->i.k_lbl = NULL;
        } else if (oi->opds[i] == KL) {
            if (token->type != T_TEXT)
                fatal(1, "line %u: Expected label", l);
            line->i.k_lbl = token->text;
        } else if (oi->opds[i] == D) {
            if (token->type != T_NUMBER)
                fatal(1, "line %u: Expected destination select", l);
            else if (token->num > 1)
                fatal(1, "line %u: Destination select out of range", l);
            line->i.d = token->num;
        } else if (oi->opds[i] == T) {
            if (token->num < 5 || 7 < token->num)
                fatal(1, "line %u: Port %"PRIu16" out of range", l,
                    token->num);
        }

        ++token;
    }

    if (token->type != T_NONE)
        fatal(1, "line %u: Trailing tokens", l);

    return line;
}


// directives
static
union line* resolve_pass1(union line* start)
{
    for (unsigned int i = 0; i < lengthof(reg_info); ++i)
        reg_info[i].name = NULL;

    union line* prev = NULL;

    for (union line* line = start; line != NULL; line = line->d.next) {
        if (line->d.oi->opc == CD_REG) {
            if (line->d.addr1 == -1)
                fatal(1, "NOT IMPLEMENTED"); // TODO
            unsigned int h = hash(line->d.name) % REG_INFO_CAP;
            while (reg_info[h].name != NULL) {
                ++h;
                if (h >= lengthof(reg_info))
                    fatal(1, "Register dict is full");
            }
            reg_info[h].name = line->d.name;
            reg_info[h].bank = line->d.addr1 >> 7;
            reg_info[h].addr = line->d.addr1 % (1<<7);

        } else if (line->d.oi->opc == CD_SREG) {
            fatal(1, "NOT IMPLEMENTED"); // TODO
        } else {
            continue;
        }

        if (prev == NULL)
            start = line->d.next;
        else
            prev->d.next = line->d.next;
        prev = line;
    }

    return start;
}


// *F, .*F, .BRA?
static
union line* resolve_pass2(union line* start)
{
    struct opcode_info* const oi_movlb = opcode_info_get("movlb");
    struct opcode_info* const oid_goto = opcode_info_get(".goto");

    label_info_init();

    union line* line = start;
    union line* prev = NULL;

    unsigned int addr = 0;

    while (line != NULL) {
        union line* next = line->i.next;
        line->i.next = prev;

        if (line->i.label != NULL) {
            struct label_info* li = label_info_avail(line->i.label);
            li->name = line->i.label;
            li->addr = addr;
        }

        enum opcode opc = line->i.oi->opc;

        if ((C_ADDWF <= opc && opc <= C_BTFSS && opc != C_CLRW) ||
                (CD_ADDWF <= opc && opc <= CD_BTFSS)) {
            ++addr;

            bool smart = (CD_ADDWF <= opc && opc <= CD_BTFSS);

            if (line->i.f_name != NULL) {
                unsigned int h = hash(line->i.f_name) % REG_INFO_CAP;
                while (true) {
                    if (h >= lengthof(reg_info) || reg_info[h].name == NULL)
                        fatal(1, "Register %s is not defined", line->i.f_name);
                    if (strcmp(reg_info[h].name, line->i.f_name) == 0)
                        break;
                    ++h;
                }

                line->i.f = reg_info[h].addr;
                free(line->i.f_name); // possibly harmful
                line->i.f_name = NULL; // possibly unnecessary

                if (smart) {
                    union line* inter_line = malloc(sizeof(union line));
                    inter_line->i.next = prev;
                    inter_line->i.oi = oi_movlb;
                    inter_line->i.k = reg_info[h].bank;
                    inter_line->i.k_lbl = NULL;

                    line->i.next = inter_line;
                }
            }
        } else if (opc == CD_BRA) {
            struct label_info* li = label_info_get(line->i.k_lbl);
            if (li == NULL) {
                // Can't tell how far yet. Assume worst case (MOVLP, GOTO).
                addr += 2;
            } else {
                line->i.k = li->addr - addr + 1;
                if (-256 <= line->i.k && line->i.k <= 255) {
                    ++addr;
                } else {
                    addr += 2;
                    line->i.oi = oid_goto;
                }
            }
        } else {
            ++addr;
        }

        prev = line;
        line = next;
    }

    return prev;
}


// .BRA?, labels
static
union line* resolve_pass3(union line* start, unsigned int* addr_offset)
{
    (void)addr_offset;

    unsigned int addr = 0;
    for (union line* line = start; line != NULL; line = line->i.next,
            ++addr) {
        /*printf("%p\n", line->i.label);*/
        /*if (line->i.label != NULL)*/
            /*printf("%s\n", line->i.label);*/
        if ((CD_REG <= line->i.oi->opc && line->i.oi->opc <= CD_SREG) ||
                line->i.label == NULL)
            continue;
        printf("label %s = %d\n", line->i.label, addr);
        struct label_info* li = label_info_avail(line->i.label);
        li->name = line->i.label;
        li->addr = addr;
    }

    return start;
}


// .BRA, .CALL, .GOTO
static
union line* resolve_pass4(union line* start, unsigned int addr_offset)
{
    (void)addr_offset;
    return start;
}


static
uint16_t assemble_line(union line* line, unsigned int addr)
{
    (void)addr;

    enum opcode opc = line->i.oi->opc;

    uint16_t word = line->i.oi->word;

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
        if (opc == C_CLRF || opc == C_MOVWF || line->i.d)
            word |= 0x0080;
        word |= line->i.f;
    } else if (
            opc == C_BCF || opc == C_BSF ||
            opc == C_BTFSC || opc == C_BTFSS) {
        word |= (line->i.b << 7) | line->i.f;
    } else if (
            opc == C_ADDLW || opc == C_ANDLW ||
            opc == C_IORLW || opc == C_MOVLB ||
            opc == C_MOVLP || opc == C_MOVLW ||
            opc == C_SUBLW || opc == C_XORLW ||
            opc == C_BRA || opc == C_CALL ||
            opc == C_GOTO || opc == C_RETLW) {
        if (line->i.k_lbl != NULL) {
            fatal(1, "NOT IMPLEMENTED");
            //line->i.k = label_info[h].addr;
        }

        // To be portable, we can't assume this machine uses a two's complement
        // representation.
        if (line->i.k >= 0)
            word |= line->i.k;
        else
            word |= (1 << line->i.oi->kwid) + line->i.k;
    } else if (opc == C_TRIS) {
        word |= line->i.f;
    }

    return word;
}


void dump_hex(union line* start, const int out)
{
    (void)out;

    label_info_init();

    unsigned int addr_offset = 0;

    for (union line* line = start; line != NULL; line = line->i.next) {
        print_line(line);
        putchar('\n');
    }
    putchar('\n');

    start = resolve_pass1(start);

    for (union line* line = start; line != NULL; line = line->i.next) {
        print_line(line);
        putchar('\n');
    }
    putchar('\n');

    start = resolve_pass2(start);

    for (union line* line = start; line != NULL; line = line->i.next) {
        print_line(line);
        putchar('\n');
    }
    putchar('\n');

    start = resolve_pass3(start, &addr_offset);

    for (union line* line = start; line != NULL; line = line->i.next) {
        print_line(line);
        putchar('\n');
    }
    putchar('\n');

    start = resolve_pass4(start, addr_offset);

    //int bank = 0;
    //for (union line line = start, addr = 0; line != NULL; line = line->i.next,
            //++addr) {
        //if (line->i.label != NULL)
            //bank = -1;
        //else if (line->i.oi->opc == C_MOVLB)
            //bank = line->i.k;
        //printf("0x%04"PRIX16"\n", assemble_line(line, addr));
        //printf("  bank = %d\n", bank);
    //}

    /*while (true) {*/
        /*char data[16 * 2];*/
        /*unsigned int i;*/
        /*for (i = 0; i < 16 * 2; i += 2, ++addr, line = line->i.next) {*/
            /*if (line == NULL) {*/
                /*fputs(":00000001FF\n", out);*/
                /*return;*/
            /*}*/
            /*sprintf(&data[i], "%02X", assemble_line(*/
}


bool assemble_P16(const int src)
{
    size_t bufpos = 0;
    size_t buflen = 1;

    union line* start = NULL;
    union line* prev_line = NULL;

    opcode_info_init();

    char* label = NULL;
    for (unsigned int l = 1; /* */; ++l) {
        v2("Lexing line");
        struct token tokens[16];
        lex_line(tokens, src, &bufpos, &buflen);
        if (verbosity >= 2)
            for (unsigned int i = 0; i < lengthof(tokens) &&
                    tokens[i].type != T_NONE; ++i)
                print_token(&tokens[i]);
        if (buflen == 0)
            break;
        union line* line = parse_line(prev_line, tokens, l, &label);
        if (start == NULL && line != NULL)
            start = line;
        prev_line = line;
    }

    dump_hex(start, 0);

    return false;
}
