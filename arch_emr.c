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

#define R 0x0001
#define D 0x0002
#define B 0x0004
#define I 0x0008
#define U 0x0010
#define M 0x0020
#define X 0x0040
#define F 0x0080
#define A 0x0100
#define L 0x0200
#define T 0x0400
#define N 0x0800
#define E 0x1000


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
    uint16_t opds[OPDS_LEN];
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


struct cmdinfo cmdinfo_init[] = {
    [C__NONE__] = { .str = NULL },

    [C_ADDWF] = { .str = "addwf", .opds = {R, D, 0} },
    [C_MOVLW] = { .str = "movlw", .opds = {I, 0} },

    [C__LAST__] = { .str = NULL },

    [D_SFR] = { .str = ".sfr", .opds = {N, E, 0} },

    [D__LAST__] = { .str = NULL },
};


struct cmdinfo_item {
    const char* str;
    struct cmdinfo* cmd;
} cmdinfo_array[256];


struct dict cmdinfo = {
    .array = cmdinfo_array,
    .capacity = lengthof(cmdinfo_array),
    .value_len = sizeof(struct cmdinfo_item),
};


struct line {
    char* label;
    int num;
    struct cmdinfo* cmd;
    union {
        int32_t i;
    } opds[OPDS_LEN];
};


void print_line(struct line* line)
{
    if (line->label != NULL)
        printf("%s: ", line->label);

    printf("%s", line->cmd->str);

    for (int o = 0; o < OPDS_LEN; ++o) {
        if (line->cmd->opds[o] == 0)
            break;
        if (o != 0)
            putchar(',');
        if (line->cmd->opds[o] & (I | U ))
            printf(" 0x%X", line->opds[o].i);
        else if (line->cmd->opds[o] == D)
            printf(" %c", line->opds[o].i ? 'f' : 'w');
        else if (line->cmd->opds[o] & (R | L))
            printf(" %s", line->opds[o].s);
    }

    putchar('\n');
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
        strcpy(line.label, first_text);

        cmd = next_token(b, l);

        if (cmd.type == T_EOL)
            return line;
        else if (cmd.type != T_TEXT)
            fatal(E_COMMON, "%d: Expected opcode or directive", l);

        b->buf[b->pos] = '\0';
        printf("%s\n", cmd.text);

        ci = dict_get(&cmdinfo, cmd.text);
        if (ci == NULL)
            fatal(E_COMMON, "%d: Invalid opcode or directive \"%s\"", l,
                cmd.text);
    } else {
        ci = dict_get(&cmdinfo, first_text);
        if (ci == NULL)
            fatal(E_COMMON, "%d: Invalid opcode or directive \"%s\"", l,
                first_text);
    }
    line.cmd = ci->cmd;

    for (int o = 0; o < OPDS_LEN && line.cmd->opds[o] != 0; ++o) {
        if (tkn.type == T_EOL) {
            if (line.cmd->opds[o] == D && line.cmd->opds[o + 1] == 0) {
                line.opds[o].i = 1;
                break;
            } else {
                fatal(E_COMMON, "%d: Unexpected newline", l);
            }
        }

        if (o > 0) {
            if ( !(tkn.type == T_CHAR && tkn.num == ',') )
                fatal(E_COMMON, "%d: Expected comma", l);

            tkn = next_token(b, l);
        }

        if (line.cmd->opds[o] == I) {
            if (tkn.type != T_NUM)
                fatal(E_COMMON, "%d: Expected number", l);
            line.opds[o].i = tkn.num;
        } else if (line.cmd->opds[o] == D) {
            if ( !(tkn.type == T_TEXT && tkn.num == 1) )
                fatal(E_COMMON, "%d: Expected destination select", l);

            if (tkn.text[0] == 'w' || tkn.text[0] == 'W')
                line.opds[o].i = 0;
            else if (tkn.text[0] == 'f' || tkn.text[0] == 'F')
                line.opds[o].i = 1;
            else
                fatal(E_COMMON, "%d: Expected destination select", l);
        } else {
            fatal(E_RARE, "%d: Unimplemented operand type", l);
        }

        tkn = next_token(b, l);
    }

    if (tkn.type != T_EOL)
        fatal(E_COMMON, "%d: Trailing characters", l);

    return line;
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

    int l = 1;
    char* label = NULL;
    while (true) {
        struct line line = parse_line(&b, l);
        if (line.num == -1) {
            break;
        } else if (line.cmd == NULL) {
            if (line.label != NULL)
                label = line.label;
        } else {
            if (line.label == NULL)
                line.label = label;
            label = NULL;
            print_line(&line);
        }
        ++l;
    }
}
