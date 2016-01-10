#include "arch_emr.h"

#include "cpic.h"
#include "dict.h"
#include "fail.h"
#include "utils.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>


#define CHUNK_LEN 256

#define OPDS_LEN 3

#define U   0x0010
#define U3  0x0013
#define U5  0x0015
#define U7  0x0017
#define U8  0x0018
#define U12 0x001C
#define I   0x0020
#define I6  0x0026
#define L9  0x0040 // 9-bit signed relative label
#define L11 0x0080 // 11-bit unsigned absolute label
#define A   0x0100
#define A5  0x0105 // register name (bank)
#define B   0x0200
#define B3  0x0203 // bit name
#define D   0x0400 // destination select
#define F   0x0800 // "FSR0", e.g.
#define R   0x1000
#define R7  0x1007 // register name (address)
#define S   0x2000 // special
#define T   0x4000 // "TRISA", e.g.


struct buffer {
    char buf[CHUNK_LEN * 2 + 1];
    const int src;
    size_t end;
    size_t tok;
    size_t pos;
};


static
ssize_t fill_buffer(struct buffer* const b)
{
    if (b->end - b->tok >= CHUNK_LEN)
        fatal(E_RARE, "Buffer is full");
    memmove(b->buf, b->buf + b->tok, b->end - b->tok);
    b->pos = b->end - b->tok;
    b->tok = 0;
    ssize_t count = read(b->src, b->buf + b->pos, CHUNK_LEN);
    if (count == -1) {
        fatal_e(E_COMMON, "Can't read from input file");
    } else if (count == 0) {
        return count;
    }

    b->end = b->pos + count;

    return count;
}


struct token {
    enum token_type {
        T_CHAR,
        T_TEXT,
        T_NUM,
        T_EOL,
        T_EOF,
    } type;
    char* text;
    int32_t num;
};


static
struct token next_token(struct buffer* const b, int l)
{
    if (b->pos == b->end && fill_buffer(b) == 0)
        return (struct token){ .type = T_EOF };

    while (b->buf[b->pos] == ' ' || b->buf[b->pos] == '\t'
            || b->buf[b->pos] == '\0') {
        b->tok = ++b->pos;
        if (b->pos == b->end && fill_buffer(b) == 0)
            return (struct token){ .type = T_EOF };
    }

    if (b->buf[b->pos] == ',' || b->buf[b->pos] == ':') {
        struct token tkn = { .type = T_CHAR, .num = b->buf[b->pos] };
        b->tok = ++b->pos;
        return tkn;
    } else if (b->buf[b->pos] == ';' || b->buf[b->pos] == '\n') {
        while (b->buf[b->pos] != '\n') {
            b->tok = ++b->pos;
            if (b->pos == b->end && fill_buffer(b) == 0)
                return (struct token){ .type = T_EOF };
        }
        b->tok = ++b->pos;
        return (struct token){ .type = T_EOL };
    }

    while (strchr(" \t,:;\n", b->buf[b->pos]) == NULL) {
        ++b->pos;
        if (b->pos == b->end && fill_buffer(b) == 0)
            return (struct token){ .type = T_EOF };
    }

    char* chr = b->buf + b->tok;
    const char* const tknend = b->buf + b->pos;

    bool negative = false;
    if (*chr == '-') {
        ++chr;
        negative = true;
    }

    if (*chr == '0') {
        ++chr;
        struct token t = { .type = T_NUM, .num = 0 };
        if (chr == tknend) {
            return t;
        } else if (*chr == 'x') {
            // hex
            while (++chr < tknend) {
                int digit;
                if ('0' <= *chr && *chr <= '9')
                    digit = *chr - '0';
                else if ('A' <= *chr && *chr <= 'F')
                    digit = 0xA + *chr - 'A';
                else if ('a' <= *chr && *chr <= 'f')
                    digit = 0xA + *chr - 'a';
                else if (*chr == '_')
                    continue;
                else
                    fatal(E_COMMON, "%d: Invalid hexadecimal integer", l);
                t.num = (t.num << 4) + digit;
            }
        } else if (*chr == 'n' || *chr == 'b') {
            // binary
            while (++chr < tknend) {
                if (*chr == '0' || *chr == '1')
                    t.num = (t.num << 1) + *chr - '0';
                else if (*chr == '_')
                    continue;
                else
                    fatal(E_COMMON, "%d: Invalid binary integer", l);
            }
        } else if (*chr == 'c') {
            // octal
            while (++chr < tknend) {
                if ('0' <= *chr && *chr <= '7')
                    t.num = (t.num << 3) + *chr - '0';
                else if (*chr == '_')
                    continue;
                else
                    fatal(E_COMMON, "%d: Invalid octal integer", l);
            }
        } else {
            fatal(E_COMMON, "%d: Invalid integer", l);
        }
        if (negative)
            t.num = -t.num;
        return t;
    } else if ('1' <= *chr && *chr <= '9') {
        // decimal
        struct token t = { .type = T_NUM, .num = 0 };
        do {
            if ('0' <= *chr && *chr <= '9')
                t.num = t.num * 10 + *chr - '0';
            else
                fatal(E_COMMON, "%d: Invalid decimal integer", l);
        } while (++chr < tknend);
        if (negative)
            t.num = -t.num;
        return t;
    }

    return (struct token){
        .type = T_TEXT,
        .text = b->buf + b->tok,
        .num = b->pos - b->tok,
    };
}


struct cmdinfo {
    const char* str;
    struct cmdinfo_shape* shape;
    uint8_t wids[OPDS_LEN];
};


enum cmd {
    C__NONE__ = 0,

    C_ADDWF,
    C_MOVLW,

    C__LAST__,

    D_SFR,

    D__LAST__,
};


enum {
    SH_R7 = 0,
    SH_R7D,
    SH_U8,
    SH_U12S,
};


// All shapes must end in 0; chained shapes must be in nondecreasing order by
// length.
struct cmdinfo_shape {
    uint16_t opds[OPDS_LEN];
    struct cmdinfo_shape* next;
} shapes[] = {
    [SH_R7] = { .opds = {R7, 0}, .next = shapes + SH_R7D },
    [SH_R7D] = { .opds = {R7, D, 0}, .next = NULL },
    [SH_U8] = { .opds = {U8, 0}, .next = NULL },
    [SH_U12S] = { .opds = {U12, S, 0}, .next = NULL },
};


struct cmdinfo cmdinfo_init[] = {
    [C__NONE__] = { .str = NULL },

    [C_ADDWF] = { .str = "addwf", .shape = shapes + SH_R7 },
    [C_MOVLW] = { .str = "movlw", .shape = shapes + SH_U8 },

    [C__LAST__] = { .str = NULL },

    [D_SFR] = { .str = ".sfr", .shape = shapes + SH_U12S },

    [D__LAST__] = { .str = NULL },
};


struct cmdinfo_item {
    const char* str;
    struct cmdinfo* cmd;
} cmdinfo_array[256];


dict_define(cmdinfo, cmdinfo_array);


struct reginfo {
    const char* name;
    int bank;
    int addr;
} reginfo_array[256];

struct reginfo reginfo_init[] = {
    { .name = "INDF0", .bank = -1, .addr = 0x00 },
    { .name = "INDF1", .bank = -1, .addr = 0x01 },
    { .name = "PCL", .bank = -1, .addr = 0x02 },
    { .name = "STATUS", .bank = -1, .addr = 0x03 },
    { .name = "FSR0L", .bank = -1, .addr = 0x04 },
    { .name = "FSR0H", .bank = -1, .addr = 0x05 },
    { .name = "FSR1L", .bank = -1, .addr = 0x06 },
    { .name = "FSR1H", .bank = -1, .addr = 0x07 },
    { .name = "BSR", .bank = -1, .addr = 0x08 },
    { .name = "WREG", .bank = -1, .addr = 0x09 },
    { .name = "PCLATH", .bank = -1, .addr = 0x0A },
    { .name = "INTCON", .bank = -1, .addr = 0x0B },
};


dict_define(reginfo, reginfo_array);


struct line {
    char* label;
    struct line* prev;
    struct line* next;
    int num;
    bool star;
    struct cmdinfo* cmd;
    struct cmdinfo_shape* shape;
    union {
        int32_t i;
        char* s;
        struct {
            int8_t b; // -1 means common
            uint8_t a;
        } r;
    } opds[OPDS_LEN];
};


void print_line(struct line* line)
{
    printf("[%4d]  ", line->num);
    if (line->label != NULL)
        printf("%s: ", line->label);

    printf("%s", line->cmd->str);

    for (int o = 0; o < OPDS_LEN; ++o) {
        uint16_t* opds = line->shape->opds;

        if (opds[o] == 0)
            break;
        if (o != 0)
            putchar(',');
        if (opds[o] & (U | A | R)) {
            int bits = opds[o] & 0xF;
            if (bits <= 5)
                printf(" %d", line->opds[o].i);
            else if (bits <= 8)
                printf(" 0x%02X", line->opds[o].i);
            else
                printf(" 0x%04X", line->opds[o].i);
        } else if (opds[o] & I) {
            if (line->opds[o].i >= 0)
                printf(" 0x%02X", line->opds[o].i);
            else
                printf(" -0x%02X", -line->opds[o].i);
        } else if (opds[o] == B) {
            printf(" %d", line->opds[o].i);
        } else if (opds[o] == D) {
            printf(" %c", line->opds[o].i ? 'f' : 'w');
        } else if (opds[o] == F) {
            printf(" FSR%d", line->opds[o].i);
        } else if (opds[o] & (L9 | L11)) {
            printf(" ???");
        } else if (opds[o] == S) {
            printf(" %s", line->opds[o].s);
        }
    }

    putchar('\n');
}


void insert_line(struct line* next, struct line* line)
{
    line->label = next->label;
    line->prev = next->prev;
    line->next = next;
    line->num = next->num;

    if (next->prev != NULL)
        next->prev->next = line;

    next->prev = line;
    next->label = NULL;
}


struct line parse_line(struct buffer* b, int l)
{
    struct line line = {
        .label = NULL,
        .num = l,
        .cmd = NULL,
    };

    struct token first = next_token(b, l);

    if (first.type == T_EOL)
        return line;
    else if (first.type == T_EOF)
        return (struct line){ .label = NULL, .num = -1, .cmd = NULL };
    else if (first.type != T_TEXT)
        fatal(E_COMMON, "%d: Expected label, opcode, or directive", l);

    char first_text[first.num + 1];
    memcpy(first_text, first.text, first.num);
    first_text[first.num] = '\0';

    struct token tkn = next_token(b, l);

    struct token cmd;
    struct cmdinfo_item* ci;

    if (tkn.type == T_CHAR) {
        if (tkn.num != ':')
            fatal(E_COMMON, "%d: Expected colon, operand, or newline", l);
        line.label = malloc(first.num + 1);
        if (line.label == NULL)
            fatal(E_RARE, "Can't allocate label");
        strcpy(line.label, first_text);

        cmd = next_token(b, l);

        if (cmd.type == T_EOL)
            return line;
        else if (cmd.type != T_TEXT)
            fatal(E_COMMON, "%d: Expected opcode or directive", l);

        b->buf[b->pos] = '\0';

        line.star = (cmd.text[0] == '*');
        if (line.star)
            ++cmd.text;
        ci = dict_get(&cmdinfo, cmd.text);
        if (ci == NULL)
            fatal(E_COMMON, "%d: Invalid opcode or directive \"%s\"", l,
                cmd.text);

        tkn = next_token(b, l);
    } else {
        line.star = (first_text[0] == '*');
        if (line.star)
            ci = dict_get(&cmdinfo, first_text + 1);
        else
            ci = dict_get(&cmdinfo, first_text);
        if (ci == NULL)
            fatal(E_COMMON, "%d: Invalid opcode or directive \"%s\"", l,
                first_text);
    }
    line.cmd = ci->cmd;

    /*struct cmdinfo_shape* shape = line.cmd->shape;*/
    line.shape = line.cmd->shape;
    int o;
    for (o = 0; /**/; ++o) {
        // Handle separator. //
        if (o > 0) {
            if (tkn.type == T_CHAR && tkn.num == ',')
                tkn = next_token(b, l);
            else if (tkn.type != T_EOL)
                fatal(E_COMMON, "%d: Expected comma or EOL", l);
        }

        for (/**/; tkn.type != T_EOL && line.shape != NULL;
                line.shape = line.shape->next) {
            uint16_t* opds = line.shape->opds;

            if (line.shape->opds[o] == 0)
                continue;

            if (opds[o] & (U | I)) {
                if (tkn.type != T_NUM)
                    fatal(E_COMMON, "%d: Expected number", l);

                int32_t min;
                int32_t max;
                switch (opds[o]) {
                    case U3:
                        min = 0; max = 0x7; break;
                    case U5:
                        min = 0; max = 0x1F; break;
                    case U7:
                        min = 0; max = 0x7F; break;
                    case U8:
                        min = 0; max = 0xFF; break;
                    case U12:
                        min = 0; max = 0xFFF; break;
                    case I6:
                        min = -0x20; max = 0x1F; break;
                    default:
                        fatal(E_RARE, "Impossible state");
                }
                if ( !(min <= tkn.num && tkn.num <= max) )
                    continue;

                line.opds[o].i = tkn.num;
            } else if (opds[o] & D) {
                if ( !(tkn.type == T_TEXT && tkn.num == 1) )
                    continue;

                if (tkn.text[0] == 'w' || tkn.text[0] == 'W')
                    line.opds[o].i = 0;
                else if (tkn.text[0] == 'f' || tkn.text[0] == 'F')
                    line.opds[o].i = 1;
                else
                    fatal(E_COMMON, "%d: Expected destination select", l);
            } else if (opds[o] & R) {
                if (tkn.type == T_TEXT) {
                    char c = tkn.text[tkn.num];
                    tkn.text[tkn.num] = '\0';
                    struct reginfo* ri = dict_get(&reginfo, tkn.text);
                    if (ri == NULL)
                        fatal(E_COMMON, "%d: Invalid register \"%s\"", l,
                            tkn.text);
                    tkn.text[tkn.num] = c;
                    line.opds[o].r.b = ri->bank;
                    line.opds[o].r.a = ri->addr;
                } else if (tkn.type == T_NUM) {
                    line.opds[o].r.b = -1;
                    line.opds[o].r.a = tkn.num;
                } else {
                    continue;
                }
            } else if (opds[o] & S) {
                if (tkn.type != T_TEXT)
                    continue;
                line.opds[o].s = malloc(tkn.num + 1);
                memcpy(line.opds[o].s, tkn.text, tkn.num);
                line.opds[o].s[tkn.num] = '\0';
            } else {
                fatal(E_RARE, "%d: Unimplemented operand type", l);
            }

            break; // I'm so sorry.
        }

        if (line.shape == NULL)
            fatal(E_COMMON, "%d: Operand %d is invalid", l, o + 1);

        if (tkn.type == T_EOL)
            break;

        tkn = next_token(b, l);
    }

    if (line.shape->opds[o] != 0)
        fatal(E_COMMON, "%d: Too few operands", l);

    if (tkn.type != T_EOL)
        fatal(E_COMMON, "%d: Trailing characters", l);

    return line;
}


static
struct line* assemble_file(struct buffer* b)
{
    struct line* start = NULL;
    struct line* prev = NULL;

    int l = 1;
    int bsr = -1;
    char* label = NULL;
    while (true) {
        struct line* line = malloc(sizeof(struct line));
        if (line == NULL)
            fatal(E_RARE, "Can't allocate line");
        *line = parse_line(b, l);
        if (line->num == -1) {
            break;
        } else if (line->cmd == NULL) {
            if (line->label != NULL)
                label = line->label;
        } else {
            if (line->label == NULL)
                line->label = label;
            label = NULL;

            if (start == NULL) {
                start = line;
            } else {
                line->prev = prev;
                prev->next = line;
            }

            if (line->label)
                bsr = -1;

            if (line->cmd > cmdinfo_init + C__LAST__) {
                if (line->cmd == cmdinfo_init + D_SFR) {
                    struct reginfo* ri = dict_avail(&reginfo, line->opds[1].s);
                    ri->name = line->opds[1].s;
                    ri->bank = line->opds[0].i >> 7;
                    ri->addr = line->opds[0].i & ((2 << 7) - 1);
                }
            } else {
                if (line->cmd->shape->opds[0] & R) {
                    if (line->opds[0].r.b == -1) {
                        line->opds[0].i = line->opds[0].r.a;
                    } else if (line->star) {
                        if (bsr != -1 && line->opds[0].r.b != bsr)
                            fatal(E_COMMON,
                                "Star prevents changing to bank %d",
                                line->opds[0].r.b);
                        line->opds[0].i = line->opds[0].r.a;
                    } else {
                        if (line->opds[0].r.b != bsr) {
                            struct line* new = malloc(sizeof(struct line));
                            if (new == NULL)
                                fatal(E_RARE, "Can't allocate line");
                            new->star = false;
                            new->cmd = cmdinfo_init + C_MOVLW;
                            new->opds[0].i = line->opds[0].r.b;
                            insert_line(line, new);
                        }
                    }
                }
            }

            print_line(line);
        }
        ++l;
        prev = line;
    }

    return start;
}


void assemble_emr(const int src)
{
    dict_init(&cmdinfo);

    for (enum cmd c = 0; c <= D__LAST__; ++c) {
        if (cmdinfo_init[c].str == NULL)
            continue;
        struct cmdinfo_item* item = dict_avail(&cmdinfo, cmdinfo_init[c].str);
        item->str = cmdinfo_init[c].str;
        item->cmd = cmdinfo_init + c;
    }

    dict_init(&reginfo);

    for (size_t i = 0; i < lengthof(reginfo_init); ++i)
        *(struct reginfo*)dict_avail(&reginfo, reginfo_init[i].name) =
            reginfo_init[i];

    struct buffer b = {
        .src = src,
        .end = 0,
        .tok = 0,
        .pos = 0,
    };

    if (fill_buffer(&b) == 0) {
        print("EOF\n");
        return;
    }

    assemble_file(&b);
}
