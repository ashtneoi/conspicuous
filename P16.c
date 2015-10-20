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
    C__LAST__,

    CD_REG,
    CD_CREG,
    CD__LAST__,

    // TODO: Implement more.
};


enum operand_type {
    NONE__ = 0, // none
    F, // register
    B, // bit number (0 - 7)
    K, // miscellaneous number
    L, // program address or label
    D, // destination select (0 = W, 1 = f)
    T, // TRIS operand (5 - 7)
    N, // FSR or INDF number (0 - 1)
    MM, // pre-/post-decrement/-increment select
    A, // bank number
    I, // unused identifier
};


struct opcode_info {
    enum opcode opc;
    const char* str;
    uint16_t word;
    enum operand_type opds[2];
    int kwid;
};


struct opcode_info opcode_info_list[] = {
    { .opc = C_ADDWF, .str = "addwf", .word = 0x0700, .opds = {F, D} },
    { .opc = C_ADDWFC, .str = "addwfc", .word = 0x3D00, .opds = {F, D} },
    { .opc = C_ANDWF, .str = "andwf", .word = 0x0500, .opds = {F, D} },
    { .opc = C_ASRF, .str = "asrf", .word = 0x3700, .opds = {F, D} },
    { .opc = C_LSLF, .str = "lslf", .word = 0x3500, .opds = {F, D} },
    { .opc = C_LSRF, .str = "lsrf", .word = 0x3600, .opds = {F, D} },
    { .opc = C_CLRF, .str = "clrf", .word = 0x0180, .opds = {F, 0} },
    { .opc = C_CLRW, .str = "clrw", .word = 0x0100, .opds = {0, 0} },
    { .opc = C_COMF, .str = "comf", .word = 0x0900, .opds = {F, D} },
    { .opc = C_DECF, .str = "decf", .word = 0x300, .opds = {F, D} },
    { .opc = C_INCF, .str = "incf", .word = 0x0A00, .opds = {F, D} },
    { .opc = C_IORWF, .str = "iorwf", .word = 0x0400, .opds = {F, D} },
    { .opc = C_MOVF, .str = "movf", .word = 0x0800, .opds = {F, D} },
    { .opc = C_MOVWF, .str = "movwf", .word = 0x0080, .opds = {F, 0} },
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

    { .opc = C_ADDLW, .str = "addlw", .word = 0x3E00, .opds = {K, 0},
        .kwid = 8 },
    { .opc = C_ANDLW, .str = "andlw", .word = 0x3900, .opds = {K, 0},
        .kwid = 8 },
    { .opc = C_IORLW, .str = "iorlw", .word = 0x3800, .opds = {K, 0},
        .kwid = 8 },
    { .opc = C_MOVLB, .str = "movlb", .word = 0x0020, .opds = {K, 0},
        .kwid = 5 },
    { .opc = C_MOVLP, .str = "movlp", .word = 0x3180, .opds = {K, 0},
        .kwid = 7 },
    { .opc = C_MOVLW, .str = "movlw", .word = 0x3000, .opds = {K, 0},
        .kwid = 8 },
    { .opc = C_SUBLW, .str = "sublw", .word = 0x3C00, .opds = {K, 0},
        .kwid = 8 },
    { .opc = C_XORLW, .str = "xorlw", .word = 0x3A00, .opds = {K, 0},
        .kwid = 8 },

    { .opc = C_BRA, .str = "bra", .word = 0x3200, .opds = {L, 0}, .kwid = 9 },
    { .opc = C_BRW, .str = "brw", .word = 0x000B, .opds = {0, 0} },
    { .opc = C_CALL, .str = "call", .word = 0x2000, .opds = {L, 0},
        .kwid = 11 },
    { .opc = C_CALLW, .str = "callw", .word = 0x000A, .opds = {0, 0} },
    { .opc = C_GOTO, .str = "goto", .word = 0x2800, .opds = {L, 0},
        .kwid = 11 },
    { .opc = C_RETFIE, .str = "retfie", .word = 0x0009, .opds = {0, 0} },
    { .opc = C_RETLW, .str = "retlw", .word = 0x3400, .opds = {K, 0},
        .kwid = 8 },
    { .opc = C_RETURN, .str = "return", .word = 0x0008, .opds = {0, 0} },

    { .opc = C_CLRWDT, .str = "clrwdt", .word = 0x0064, .opds = {0, 0} },
    { .opc = C_NOP, .str = "nop", .word = 0x0000, .opds = {0, 0} },
    { .opc = C_OPTION, .str = "option", .word = 0x0062, .opds = {0, 0} },
    { .opc = C_RESET, .str = "reset", .word = 0x0001, .opds = {0, 0} },
    { .opc = C_SLEEP, .str = "sleep", .word = 0x0063, .opds = {0, 0} },
    { .opc = C_TRIS, .str = "tris", .word = 0x0060, .opds = {T, 0} },

    { .opc = CD_REG, .str = ".reg", .opds = {A, I} },
    { .opc = CD_CREG, .str = ".creg", .opds = {I, 0} },
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


struct line {
    struct line* next;

    struct opcode_info* oi;
    bool star;

    const char* label;

    struct operand {
        int i;
        const char* s;
    } opds[2];
};


void print_line(struct line* line)
{
    if (line->oi->opc == C_NONE)
        return;

    struct opcode_info* oi = line->oi;

    if ( !(C_NONE < oi->opc && oi->opc < CD__LAST__) )
        fatal(2, "Not implemented");

    if (line->label != NULL)
        printf("%s: ", line->label);
    if (line->star)
        putchar('*');
    print(oi->str);

    for (unsigned int i = 0; i < 2 && oi->opds[i] != 0; ++i) {
        struct operand* opd = &line->opds[i];

        if (i == 1 && oi->opds[1] == D && opd->i == 1)
            return;

        if (i == 0)
            putchar(' ');
        else
            print(", ");

        if (opd->s != NULL) {
            print(opd->s);
        } else {
            switch (oi->opds[i]) {
                case F:
                case K:
                case L:
                    printf("%#02X", opd->i);
                    break;
                case B:
                case A:
                    printf("%d", opd->i);
                    break;
                case D:
                    putchar('0'); // (Already handled 1.)
                    break;
                default:
                    fatal(E_RARE, "Impossible situation");
            }
        }
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
void lex_line(struct token* token, const int src, unsigned int l,
        size_t* const bufpos, size_t* const buflen)
{
    size_t tokstart = *bufpos + 1;
    char c;
    ssize_t toklen;
    bool first_buf = true;
    unsigned int col = 1;
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
                    fatal(1, "%u,%u: Unexpected end of file", l, col);
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
                        t[0] == '.' || t[0] == '_' || t[0] == '*') {
                    token->type = T_TEXT;
                    token->text = malloc(toklen + 1);
                    memcpy(token->text, t, toklen);
                    token->text[toklen] = '\0';
                    ++token;
                } else if (t[0] == '#' || ('0' <= t[0] && t[0] <= '9')) {
                    token = parse_number(token, t, toklen);
                } else {
                    fatal(1, "%u,%u: Invalid token", l, col);
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
struct line* parse_line(struct line* const prev_line,
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

    struct line* line = malloc(sizeof(struct line));
    line->next = NULL;
    if (prev_line != NULL)
        prev_line->next = line;

    line->label = *label;
    *label = NULL;


    line->star = (token->text[0] == '*');
    struct opcode_info* oi = opcode_info_get(
        token->text + (line->star ? 1 : 0));
    if (oi == NULL)
        fatal(1, "line %u: Invalid opcode \"%s\"", l, token->text);
    ++token;

    line->oi = oi;

    for (unsigned int i = 0; i < 2 && oi->opds[i] != 0; ++i) {
        struct operand* opd = &line->opds[i];

        if (i == 1) {
            if (token->type != T_COMMA) {
                if (oi->opds[1] == D) {
                    opd->i = 1;
                    opd->s = NULL;
                    break;
                } else {
                    fatal(1, "line %u: Expected comma", l);
                }
            }
            ++token;
        }

        if (oi->opds[i] == F) {
            if (line->star) {
                if (token->type != T_NUMBER)
                    fatal(1, "line %u: Expected register address", l);
                else if (token->num > 0x7F)
                    fatal(1, "line %u: Address 0x%"PRIX16" out of range", l,
                        token->num);
                opd->i = token->num;
                opd->s = NULL;
            } else {
                if (token->type != T_TEXT)
                    fatal(1, "line %u: Expected register name", l);
                opd->s = token->text;
            }
        } else if (oi->opds[i] == B) {
            if (token->type == T_NUMBER) {
                if (token->num > 7)
                    fatal(1, "line %u: Bit number out of range", l);
                opd->i = token->num;
                opd->s = NULL;
            } else if (token->type == T_TEXT) {
                if (line->star)
                    fatal(1, "line %u: Expected bit number", l);
                opd->s = token->text;
            } else {
                fatal(1, "line %u: Expected bit number or name", l);
            }
        } else if (oi->opds[i] == K) {
            if (token->type != T_NUMBER)
                fatal(1, "line %u: Expected literal", l);
            else if (token->num >= 1<<oi->kwid)
                fatal(1, "line %u: Literal out of range", l);
            opd->i = token->num;
            opd->s = NULL;
        } else if (oi->opds[i] == L) {
            if (line->star) {
                if (token->type != T_NUMBER)
                    fatal(1, "line %u: Expected program address", l);
                opd->i = token->num;
                opd->s = NULL;
            } else {
                if (token->type != T_TEXT)
                    fatal(1, "line %u: Expected program label", l);
                opd->s = token->text;
            }
        } else if (oi->opds[i] == D) {
            if (token->type != T_NUMBER)
                fatal(1, "line %u: Expected destination select", l);
            else if (token->num > 1)
                fatal(1, "line %u: Destination select out of range", l);
            opd->i = token->num;
            opd->s = NULL;
        } else if (oi->opds[i] == T) {
            if (token->type != T_NUMBER)
                fatal(1, "line %u: Expected number", l); // TODO: Fix error.
            if (token->num < 5 || 7 < token->num)
                fatal(1, "line %u: Port %"PRIu16" out of range", l,
                    token->num);
            opd->i = token->num; // TODO: Verify or fix this.
            opd->s = NULL;
        } else if (oi->opds[i] == A) {
            // TODO: Implement additional restrictions.
            if (token->type != T_NUMBER)
                fatal(1, "line %u: Expected bank number", l);
            opd->i = token->num;
            opd->s = NULL;
        } else if (oi->opds[i] == I) {
            if (token->type != T_TEXT)
                fatal(1, "line %u: Expected unused identifier", l);
            opd->s = token->text;
        }
        ++token;
    }

    if (token->type != T_NONE)
        fatal(1, "line %u: Trailing tokens", l);

    return line;
}


//// A1 (forward) ////
// .___ : process, remove
// ___f___ : insert movlb if bank not active
// [*]___f___ : resolve
// bra : change to goto if destination far, star if destination near
static
struct line* assemble_pass1(struct line* start)
{
    label_info_init();

    struct opcode_info* oi_goto = opcode_info_get("goto");

    int addr = 0;
    struct line* prev = NULL;
    struct line* line = start;
    while (line != NULL) {
        enum opcode opc = line->oi->opc;

        // Resolve register names.
        bool is_f = (
            (C_ADDWF <= opc && opc <= C_LSRF) ||
            (C_COMF <= opc && opc <= C_BTFSS)
        );
        if (is_f && line->opds[0].s != NULL)
            fatal(E_RARE, "Register name resolution and bank auto-switching "
                "not implemented");

        // Store label info.
        if (line->label != NULL) {
            struct label_info* li = label_info_avail(line->label);
            li->name = line->label;
            li->addr = addr;
        }

        // Handle bra.
        if (opc == C_BRA) {
            struct label_info* li = label_info_get(line->opds[0].s);
            if (li != NULL) {
                if ((addr + 1) - li->addr > 255) // reverse limit
                    line->oi = oi_goto;
                else
                    line->star = true;
            }
        }

        bool is_two = (opc == C_GOTO || opc == C_CALL);
        addr += is_two ? 2 : 1;

        {
            struct line* old_next = line->next;
            line->next = prev;
            prev = line;
            line = old_next;
        }
    }

    return prev;
}


//// A2 (reverse) ////
// bra : change to goto if destination far or not seen
// label : store
static
struct line* assemble_pass2(struct line* start, int* len)
{
    label_info_init();

    struct opcode_info* oi_goto = opcode_info_get("goto");

    int addr = 0;
    struct line* prev = NULL;
    struct line* line = start;
    while (line != NULL) {
        enum opcode opc = line->oi->opc;

        // Store label info.
        if (line->label != NULL) {
            struct label_info* li = label_info_avail(line->label);
            li->name = line->label;
            li->addr = addr;
        }

        // Handle bra.
        if (opc == C_BRA) {
            struct label_info* li = label_info_get(line->opds[0].s);
            if (li != NULL) {
                if ((addr - 1) - li->addr > 256) // forward limit
                    line->oi = oi_goto;
            }
        }

        bool is_two = (opc == C_GOTO || opc == C_CALL);
        addr += is_two ? 2 : 1;

        {
            struct line* old_next = line->next;
            line->next = prev;
            prev = line;
            line = old_next;
        }
    }

    *len = addr;

    return prev;
}


//// A3 (forward) ////
// [*]bra : resolve
// goto, call : resolve relative if destination stored
static
struct line* assemble_pass3(struct line* start, int len)
{
    (void)len;
    return start;
}


//// L1 (forward) ////
// label info : resolve absolute, store
static
struct line* link_pass1(struct line* start)
{
    return start;
}


//// L2 (forward) ////
// goto, call : resolve absolute, insert movlp
static
struct line* link_pass2(struct line* start)
{
    return start;
}


static
uint16_t dump_line(struct line* line, unsigned int addr)
{
    (void)addr;

    uint16_t word = line->oi->word;

    enum operand_type type = line->oi->opds[0];
    if (type == 0)
        return word;
    if (line->opds[0].s != NULL)
        fatal(E_RARE, "Unresolved symbol");
    int num = line->opds[0].i;

    switch (line->oi->opds[0]) {
        case F:
        case T:
        case K:
        case L:
            word |= num;
            break;
        default:
            fatal(E_RARE, "Unrecognized operand type (%d)", line->oi->opds[0]);
    }

    type = line->oi->opds[1];
    if (type == 0)
        return word;
    if (line->opds[1].s != NULL)
        fatal(E_RARE, "Unresolved symbol");
    num = line->opds[1].i;
    switch (line->oi->opds[1]) {
        case B:
        case D:
            word |= num << 7;
            break;
        default:
            fatal(E_RARE, "Unrecognized operand type (%d)", line->oi->opds[1]);
    }

    return word;
}


void dump_hex(struct line* start, const int out)
{
    (void)out;

    label_info_init();

    /*unsigned int addr_offset = 0;*/

    for (struct line* line = start; line != NULL; line = line->next) {
        print_line(line);
        putchar('\n');
    }
    putchar('\n');

    start = assemble_pass1(start);

    for (struct line* line = start; line != NULL; line = line->next) {
        print_line(line);
        putchar('\n');
    }
    putchar('\n');

    int len;
    start = assemble_pass2(start, &len);

    for (struct line* line = start; line != NULL; line = line->next) {
        print_line(line);
        putchar('\n');
    }
    putchar('\n');

    start = assemble_pass3(start, len);

    for (struct line* line = start; line != NULL; line = line->next) {
        print_line(line);
        putchar('\n');
    }
    putchar('\n');

    start = link_pass1(start);

    for (struct line* line = start; line != NULL; line = line->next) {
        print_line(line);
        putchar('\n');
    }
    putchar('\n');

    start = link_pass2(start);

    for (struct line* line = start; line != NULL; line = line->next) {
        print_line(line);
        putchar('\n');
    }
    putchar('\n');

    for (struct line* line = start; line != NULL; line = line->next) {
        printf("%04X\n", dump_line(line, 0));
    }

    //int bank = 0;
    //for (struct line line = start, addr = 0; line != NULL; line = line->next,
            //++addr) {
        //if (line->label != NULL)
            //bank = -1;
        //else if (line->oi->opc == C_MOVLB)
            //bank = line->k;
        //printf("0x%04"PRIX16"\n", assemble_line(line, addr));
        //printf("  bank = %d\n", bank);
    //}

    /*while (true) {*/
        /*char data[16 * 2];*/
        /*unsigned int i;*/
        /*for (i = 0; i < 16 * 2; i += 2, ++addr, line = line->next) {*/
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

    struct line* start = NULL;
    struct line* prev_line = NULL;

    opcode_info_init();

    char* label = NULL;
    for (unsigned int l = 1; /* */; ++l) {
        v2("Lexing line");
        struct token tokens[16];
        lex_line(tokens, src, l, &bufpos, &buflen);
        if (verbosity >= 2)
            for (unsigned int i = 0; i < lengthof(tokens) &&
                    tokens[i].type != T_NONE; ++i)
                print_token(&tokens[i]);
        if (buflen == 0)
            break;
        struct line* line = parse_line(prev_line, tokens, l, &label);
        if (start == NULL && line != NULL)
            start = line;
        prev_line = line;
    }

    dump_hex(start, 0);

    return false;
}
