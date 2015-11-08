#include "common.h"
#include "P16.h"

#include "bufman.h"
#include "cpic.h"
#include "dict.h"
#include "fail.h"
#include "utils.h"

#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>


#define CHUNK_LEN 128
#define BUFCAP (CHUNK_LEN * 2)
#define CFG_MEM_SIZE 0x10

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

    CD_SFR,
    CD_GPR,
    CD_REG,
    CD_CREG,
    CD_CFG,
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


struct insn {
    const char* str;
    enum opcode opc;
    uint16_t word;
    enum operand_type opds[2];
    int kwid;
} insn_array[512];


struct dict insns = {
    .array = insn_array,
    .capacity = lengthof(insn_array),
    .value_len = sizeof(struct insn),
};


struct reg {
    const char* name;
    unsigned int bank;
    unsigned int addr;
} reg_array[2048];


struct dict regs = {
    .array = reg_array,
    .capacity = lengthof(reg_array),
    .value_len = sizeof(struct reg),
};


struct sreg {
    const char* name;
    unsigned int addr;
} sreg_array[128];


struct dict sregs = {
    .array = sreg_array,
    .capacity = lengthof(sreg_array),
    .value_len = sizeof(struct sreg),
};


struct label {
    const char* name;
    unsigned int addr;
} label_array[2048];


struct dict labels = {
    .array = label_array,
    .capacity = lengthof(label_array),
    .value_len = sizeof(struct label),
};


struct insn insns_ref[] = {
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
    { .opc = C_MOVLB, .str = "movlb", .word = 0x0020, .opds = {F, 0},
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

    { .opc = CD_SFR, .str = ".sfr", .opds = {F, I} },
    { .opc = CD_GPR, .str = ".gpr", .opds = {F, F} },
    { .opc = CD_REG, .str = ".reg", .opds = {A, I} },
    { .opc = CD_CREG, .str = ".creg", .opds = {I, 0} },
    { .opc = CD_CFG, .str = ".cfg", .opds = {K, K}, .kwid = 16 },
};


struct sreg sregs_ref[] = {
    { .name = "INDF0", .addr = 0x00 },
    { .name = "INDF1", .addr = 0x01 },
    { .name = "PCL", .addr = 0x02 },
    { .name = "STATUS", .addr = 0x03 },
    { .name = "FSR0L", .addr = 0x04 },
    { .name = "FSR0H", .addr = 0x05 },
    { .name = "FSR1L", .addr = 0x06 },
    { .name = "FSR1H", .addr = 0x07 },
    { .name = "BSR", .addr = 0x08 },
    { .name = "WREG", .addr = 0x09 },
    { .name = "PCLATH", .addr = 0x0A },
    { .name = "INTCON", .addr = 0x0B },
};


struct line {
    struct line* next;

    struct insn* oi;
    bool star;

    const char* label;

    struct operand {
        int i;
        const char* s;
    } opds[2];

    unsigned int num;
};


void print_line(struct line* line)
{
    if (line->oi->opc == C_NONE)
        return;

    struct insn* oi = line->oi;

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
                    printf("0x%02X", opd->i);
                    break;
                case L:
                    printf("0x%02"PRIX8, (int8_t)opd->i);
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

    printf("  ; %d", line->num);
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
            else if (t[i] != '_')
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
            if (toklen == 0)
                fatal(1, "Invalid binary number");

            token->type = T_NUMBER;
            token->num = 0;
            for (ssize_t i = 0; i < toklen; ++i) {
                if ('0' <= t[i] && t[i] <= '1')
                    token->num = token->num * 2 +
                        t[i] - '0';
                else if (t[i] != '_')
                    fatal(1, "Invalid binary number");
            }
            ++token;
        } else if ('0' <= t[1] && t[1] <= '7') {
            ++t;
            --toklen;
            if (toklen == 0)
                fatal(1, "Invalid octal number");

            token->type = T_NUMBER;
            token->num = t[0] - '0';
            for (ssize_t i = 1; i < toklen; ++i) {
                if ('0' <= t[i] && t[i] <= '7')
                    token->num = token->num * 8 +
                        t[i] - '0';
                else if (t[i] != '_')
                    fatal(1, "Invalid octal number");
            }
            ++token;
        } else if (t[1] == 'x') {
            t += 2;
            toklen -= 2;
            if (toklen == 0)
                fatal(1, "Invalid hexadecimal number");

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
                else if (t[i] != '_')
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
            if (toklen == 0)
                fatal(1, "Invalid binary number");

            ssize_t gu = ((toklen - 1) % 8) + 1; // group upper bound
            ssize_t i = 0;
            do {
                token->type = T_NUMBER;
                token->num = 0;
                for (/* */; i < gu; ++i) {
                    if ('0' <= t[i] && t[i] <= '1')
                        token->num = token->num * 2 +
                            t[i] - '0';
                    else if (t[i] != '_')
                        fatal(1, "Invalid binary number");
                }
                ++token;

                gu += 8;
            } while (gu <= toklen);
        } else if (t[2] == 'x') {
            t += 3;
            toklen -= 3;
            if (toklen == 0)
                fatal(1, "Invalid hexadecimal number");

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
                    else if (t[i] != '_')
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
        fatal(1, "%u: Expected label or opcode", l);

    v2("Setting label");
    if (token[1].type == T_COLON) {
        if (*label != NULL)
            fatal(1, "%u: Instruction already has a label", l);
        *label = token->text;
        token += 2;
        if (token->type == T_NONE) {
            v2("Carrying label to next line");
            return prev_line;
        } else if (token->type != T_TEXT) {
            fatal(1, "%u: Expected opcode", l);
        }
    }

    struct line* line = malloc(sizeof(struct line));
    line->next = NULL;
    if (prev_line != NULL)
        prev_line->next = line;

    line->label = *label;
    *label = NULL;

    line->star = (token->text[0] == '*');
    struct insn* oi = dict_get(&insns, token->text + (line->star ? 1 : 0));
    if (oi == NULL)
        fatal(1, "%u: Invalid opcode \"%s\"", l, token->text);
    ++token;

    line->oi = oi;

    if (line->label != NULL && C__LAST__ < line->oi->opc
            && line->oi->opc < CD__LAST__)
        fatal(1, "%u: Label not allowed on directive", l);

    for (unsigned int i = 0; i < 2 && oi->opds[i] != 0; ++i) {
        struct operand* opd = &line->opds[i];

        if (i == 1) {
            if (token->type != T_COMMA) {
                if (oi->opds[1] == D) {
                    opd->i = 1;
                    opd->s = NULL;
                    break;
                } else {
                    fatal(1, "%u: Expected comma", l);
                }
            }
            ++token;
        }

        if (oi->opds[i] == F) {
            if (token->type == T_TEXT) {
                opd->s = token->text;
            } else if (token->type == T_NUMBER) {
                opd->s = NULL;
                opd->i = token->num;
            } else {
                fatal(1, "%u: Expected register name or traditional "
                    "register address " , l);
            }
        } else if (oi->opds[i] == B) {
            if (token->type == T_NUMBER) {
                if (token->num > 7)
                    fatal(1, "%u: Bit number out of range", l);
                opd->i = token->num;
                opd->s = NULL;
            } else if (token->type == T_TEXT) {
                if (line->star)
                    fatal(1, "%u: Expected bit number", l);
                opd->s = token->text;
            } else {
                fatal(1, "%u: Expected bit number or name", l);
            }
        } else if (oi->opds[i] == K) {
            if (token->type != T_NUMBER)
                fatal(1, "%u: Expected literal", l);
            else if (token->num >= 1<<oi->kwid)
                fatal(1, "%u: Literal out of range", l);
            opd->i = token->num;
            opd->s = NULL;
        } else if (oi->opds[i] == L) {
            if (token->type == T_NUMBER) {
                opd->i = token->num;
                opd->s = NULL;
                    fatal(1, "%u: Expected program address", l);
            } else if (token->type == T_TEXT) {
                opd->s = token->text;
            } else {
                fatal(1, "%u: Expected program label or address", l);
            }
        } else if (oi->opds[i] == D) {
            if (token->type != T_NUMBER)
                fatal(1, "%u: Expected destination select", l);
            else if (token->num > 1)
                fatal(1, "%u: Destination select out of range", l);
            opd->i = token->num;
            opd->s = NULL;
        } else if (oi->opds[i] == T) {
            if (token->type != T_NUMBER)
                fatal(1, "%u: Expected number", l); // TODO: Fix error.
            if (token->num < 5 || 7 < token->num)
                fatal(1, "%u: Port %"PRIu16" out of range", l,
                    token->num);
            opd->i = token->num; // TODO: Verify or fix this.
            opd->s = NULL;
        } else if (oi->opds[i] == A) {
            // TODO: Implement additional restrictions.
            if (token->type != T_NUMBER)
                fatal(1, "%u: Expected bank number", l);
            opd->i = token->num;
            opd->s = NULL;
        } else if (oi->opds[i] == I) {
            if (token->type != T_TEXT)
                fatal(1, "%u: Expected unused identifier", l);
            opd->s = token->text;
        }
        ++token;
    }

    if (token->type != T_NONE)
        fatal(1, "%u: Trailing tokens", l);

    return line;
}


//// A1 (forward) ////
// .___ : process, remove
// ___f___ : insert movlb if bank not active
// [*]___f___ : resolve
// bra : change to goto if target far, star if target near
static
struct line* assemble_pass1(struct line* start, int16_t* cfg)
{
    for (unsigned int i = 0; i < CFG_MEM_SIZE; ++i)
        cfg[i] = -1;

    int* autoaddr = NULL;
    int autobankmin;
    int autobankmax;
    int autoaddrmax;

    dict_init(&labels);
    dict_init(&regs);
    dict_init(&sregs);
    for (unsigned int i = 0; i < lengthof(sregs_ref); ++i)
        *(struct sreg*)dict_avail(&sregs, sregs_ref[i].name) = sregs_ref[i];

    struct insn* oi_goto = dict_get(&insns, "goto");
    struct insn* oi_movlb = dict_get(&insns, "movlb");

    int addr = 0;
    unsigned int bank = UINT_MAX;
    struct line* prev = NULL;
    struct line* line = start;
    while (line != NULL) {
        enum opcode opc = line->oi->opc;

        // Handle directives.
        if (opc == CD_GPR) {
            autobankmin = line->opds[0].i >> 7;
            autobankmax = line->opds[1].i >> 7;

            autoaddr = malloc((autobankmax - autobankmin + 1) * sizeof(int));
            autoaddr[0] = line->opds[0].i & 0x7F;
            for (int b = 1; b < autobankmax - autobankmin + 1; ++b)
                autoaddr[b] = 0x20;
            autoaddrmax = line->opds[1].i & 0x7F;
        } else if (opc == CD_SFR) {
            struct reg* reg = dict_avail(&regs, line->opds[1].s);
            reg->bank = line->opds[0].i >> 7;
            reg->addr = line->opds[0].i & 0x7F;
            reg->name = line->opds[1].s;
        } else if (opc == CD_REG) {
            int b = line->opds[0].i;
            if ( !(autobankmin <= b && b <= autobankmax) )
                fatal(E_COMMON, "%u: Bank number %d out of range", line->num,
                    b);
            int* a = &(autoaddr[b - autobankmin]);
            if (*a > 0x6F || (b == autobankmax && *a > autoaddrmax))
                fatal(E_COMMON, "%u: No GPR left in bank %d", line->num, b);

            struct reg* reg = dict_avail(&regs, line->opds[1].s);
            reg->bank = b;
            reg->addr = *a;
            reg->name = line->opds[1].s;

            ++*a;
        } else if (opc == CD_CFG) {
            int addr = line->opds[0].i - 0x8000;
            if (addr < 0 || addr >= 0xF)
                fatal(E_COMMON, "%u: Address out of range", line->num);
            if (cfg[addr] >= 0)
                fatal(E_COMMON, "%u: Configuration word already set",
                    line->num);
            cfg[addr] = line->opds[1].i;
        }

        // Store label info.
        if (line->label != NULL) {
            struct label* li = dict_avail(&labels, line->label);
            li->name = line->label;
            li->addr = addr;
            bank = UINT_MAX;
        }

        // Resolve register names.
        bool is_f = (
            (C_ADDWF <= opc && opc <= C_CLRF) ||
            (C_COMF <= opc && opc <= C_BTFSS)
        );
        if (is_f) {
            if (line->opds[0].s != NULL) {
                struct reg* reg = dict_get(&regs, line->opds[0].s);
                if (reg == NULL) {
                    struct sreg* sreg = dict_get(&sregs, line->opds[0].s);
                    if (sreg == NULL)
                        fatal(E_COMMON, "%u: Unknown register name",
                            line->num);
                    line->opds[0].i = sreg->addr;
                } else {
                    line->opds[0].i = reg->addr;
                    if (reg->bank != bank) {
                        if (line->star) {
                            if (bank != UINT_MAX)
                                fatal(E_COMMON,
                                    "%u: Bank %d is active; star prevents "
                                    "changing to bank %d", line->num, bank,
                                    reg->bank);
                        } else {
                            struct line* new = malloc(sizeof(struct line));
                            new->next = prev;
                            new->oi = oi_movlb;
                            new->label = line->label;
                            new->star = false;
                            new->opds[0].i = reg->bank;

                            if (verbosity >= 1) {
                                printf("[0x%04X] ", addr);
                                print_line(new);
                                putchar('\n');
                            }

                            ++addr;
                            prev = new;
                            bank = reg->bank;
                            line->label = NULL;
                        }
                    }
                }
                line->opds[0].s = NULL;
            } else {
                line->opds[0].i &= 0x7F;
            }
        }

        if (opc == C_MOVLB) {
            if (line->opds[0].s != NULL) {
                struct reg* reg = dict_get(&regs, line->opds[0].s);
                if (reg == NULL)
                    fatal(E_COMMON, "%u: Unknown register name", line->num);
                line->opds[0].i = reg->bank;
                line->opds[0].s = NULL;
            }
            bank = line->opds[0].i;
        }

        // Handle bra.
        if (opc == C_BRA) {
            struct label* li = dict_get(&labels, line->opds[0].s);
            if (li != NULL) {
                if ((addr + 1) - li->addr > 255) { // reverse limit
                    if (line->star)
                        fatal(E_COMMON, "%u: Target out of range", line->num);
                    line->oi = oi_goto;
                } else {
                    line->star = true;
                }
            }
        }

        if (verbosity >= 1) {
            printf("[0x%04X] ", addr);
            print_line(line);
            putchar('\n');
        }

        // Increment address.
        if ( !(C__LAST__ < opc && opc <= CD__LAST__) )
            addr += (opc == C_GOTO || opc == C_CALL) ? 2 : 1;

        // Advance to next line.
        {
            struct line* old_next = line->next;
            if ( !(C__LAST__ < opc && opc <= CD__LAST__) ) {
                line->next = prev;
                prev = line;
            }
            line = old_next;
        }
    }

    if (verbosity >= 1)
        putchar('\n');

    return prev;
}


//// A2 (reverse) ////
// bra : change to goto if target far or not seen
// label : store
static
struct line* assemble_pass2(struct line* start, int* len)
{
    dict_init(&labels);

    struct insn* oi_goto = dict_get(&insns, "goto");

    int addr = 0;
    struct line* prev = NULL;
    struct line* line = start;
    while (line != NULL) {
        enum opcode opc = line->oi->opc;

        // Store label info.
        if (line->label != NULL) {
            struct label* li = dict_avail(&labels, line->label);
            li->name = line->label;
            li->addr = addr;
            v2("label %s = %i", li->name, li->addr);
        }

        // Handle bra.
        if (opc == C_BRA) {
            struct label* li = dict_get(&labels, line->opds[0].s);
            if (li != NULL) {
                if ((addr - 1) - li->addr > 256) { // forward limit
                    if (line->star)
                        fatal(E_COMMON, "%u: Target out of range", line->num);
                    line->oi = oi_goto;
                } else {
                    line->star = true;
                }
            }
        }

        if (verbosity >= 1) {
            printf("[0x%04X] ", addr);
            print_line(line);
            putchar('\n');
        }

        // Increment addr.
        addr += (opc == C_GOTO || opc == C_CALL) ? 2 : 1;

        // Advance to next line.
        {
            struct line* old_next = line->next;
            line->next = prev;
            prev = line;
            line = old_next;
        }
    }

    *len = addr;

    if (verbosity >= 1)
        putchar('\n');

    return prev;
}


//// A3 (forward) ////
// [*]bra : resolve
// goto, call : resolve relative if target stored
static
struct line* assemble_pass3(struct line* start, int len)
{
    int addr = 0;
    struct line* line = start;
    while (line != NULL) {
        enum opcode opc = line->oi->opc;

        // Store label info.
        if (line->label != NULL) {
            struct label* li = dict_avail(&labels, line->label);
            li->name = line->label;
            li->addr = addr;
        }

        // Handle bra.
        if (opc == C_BRA) {
            struct label* li = dict_get(&labels, line->opds[0].s);
            if (li == NULL)
                fatal(E_RARE, "%u: Target should not be unknown", line->num);
            line->opds[0].s = NULL;
            line->opds[0].i = ((len - 1) - li->addr) - (addr + 1);
        } else if (opc == C_GOTO || opc == C_CALL) {
            struct label* li = dict_get(&labels, line->opds[0].s);
            if (li != NULL) {
                line->opds[0].s = NULL;
                line->opds[0].i = ((len - 1) - li->addr) - (addr + 1);
                if (line->opds[0].i < 0)
                    --line->opds[0].i;
            }
        }

        if (verbosity >= 1) {
            printf("[0x%04X] ", addr);
            print_line(line);
            putchar('\n');
        }

        // Increment addr.
        addr += (opc == C_GOTO || opc == C_CALL) ? 2 : 1;

        // Advance to next line.
        line = line->next;
    }

    if (verbosity >= 1)
        putchar('\n');

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
    struct insn* oi_movlp = dict_get(&insns, "movlp");

    int addr = 0;
    struct line* prev = NULL;
    struct line* line = start;
    while (line != NULL) {
        enum opcode opc = line->oi->opc;

        if (opc == C_GOTO || opc == C_CALL) {
            struct line* new = malloc(sizeof(struct line));
            if (prev != NULL)
                prev->next = new;
            else
                start = new;
            new->next = line;
            new->oi = oi_movlp;
            new->label = line->label;

            ++addr;
            int target = addr + line->opds[0].i;
            new->opds[0].i = target >> 8;
            line->opds[0].i = target & ((1 << 11) - 1);

            line->label = NULL;

            if (verbosity >= 1) {
                printf("[0x%04X] ", addr - 1);
                print_line(new);
                putchar('\n');
            }
        }

        if (verbosity >= 1) {
            printf("[0x%04X] ", addr);
            print_line(line);
            putchar('\n');
        }

        // Increment addr.
        ++addr;

        // Advance to next line.
        prev = line;
        line = line->next;
    }

    if (verbosity >= 1)
        putchar('\n');

    return start;
}


static
uint16_t dump_line(struct line* line)
{
    uint16_t word = line->oi->word;

    enum operand_type type = line->oi->opds[0];
    if (type == 0)
        return word;
    if (line->opds[0].s != NULL)
        fatal(E_RARE, "%u: Unresolved symbol", line->num);
    uint16_t num = line->opds[0].i;

    switch (line->oi->opds[0]) {
        case F:
        case T:
        case K:
            word |= num;
            break;
        case L:
            word |= num & ((1 << line->oi->kwid) - 1);
            break;
        default:
            fatal(E_RARE, "Unrecognized operand type (%d)", line->oi->opds[0]);
    }

    type = line->oi->opds[1];
    if (type == 0)
        return word;
    if (line->opds[1].s != NULL)
        fatal(E_RARE, "%u: Unresolved symbol", line->num);
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


void dump_hex(struct line* start, int len, int16_t* cfg)
{
    int addr = 0;
    struct line* line = start;
    while (line != NULL) {
        int line_count = (len - addr >= 8) ? 8 : len - addr;
        uint8_t sum = line_count * 2 + addr * 2;
        printf(":%02X%04X00", line_count * 2, addr * 2);
        for (int i = 0; i < line_count; ++i, ++addr) {
            if (verbosity >= 2) {
                print_line(line);
                putchar('\n');
            }
            uint16_t line_bin = dump_line(line);
            printf("%02"PRIX8"%02"PRIX8, line_bin & 0xFF, line_bin >> 8);
            if (verbosity >= 2)
                putchar('\n');
            sum += (line_bin & 0xFF) + (line_bin >> 8);
            line = line->next;
        }
        printf("%02"PRIX8"\n", (uint8_t)-sum);
    }

    print(":020000040001F9\n");
    for (unsigned int a = 0; a < CFG_MEM_SIZE; ++a) {
        if (cfg[a] < 0)
            continue;
        int data_hi = cfg[a] >> 8;
        int data_lo = cfg[a] & 0xFF;
        uint8_t sum = 2 + a + data_hi + data_lo;
        printf(":02%04X00%02X%02X%02"PRIX8"\n", a * 2, data_lo, data_hi,
            (uint8_t)-sum);
    }
    print(":00000001FF\n");
}


void assemble_P16(const int src)
{
    size_t bufpos = 0;
    size_t buflen = 1;

    struct line* start = NULL;
    struct line* prev_line = NULL;

    dict_init(&insns);
    for (unsigned int i = 0; i < lengthof(insns_ref); ++i)
        *(struct insn*)dict_avail(&insns, insns_ref[i].str) = insns_ref[i];

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
        if (line != NULL)
            line->num = l;
        if (start == NULL && line != NULL)
            start = line;
        prev_line = line;
    }

    int len;
    int16_t cfg[CFG_MEM_SIZE];
    start = assemble_pass1(start, cfg);
    start = assemble_pass2(start, &len);
    start = assemble_pass3(start, len);
    start = link_pass1(start);
    start = link_pass2(start);

    dump_hex(start, len, cfg);
}
