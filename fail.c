#include "fail.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>


void vx_(const char* progname, int line, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    fprintf(stdout, "%s:%d: ", progname, line);
    vfprintf(stdout, format, args);
    putchar('\n');
}


void warning_(const char* progname, int line, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    fprintf(stderr, "%s:%d: ", progname, line);
    vfprintf(stderr, format, args);
    putc('\n', stderr);
}


void warning_e_(const char* progname, int line, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    fprintf(stderr, "%s:%d: ", progname, line);
    vfprintf(stderr, format, args);
    fprintf(stderr, " (%s)\n", strerror(errno));
}


void fatal_(int rtn, const char* progname, int line,
        const char* format, ...)
{
    va_list args;
    va_start(args, format);
    fprintf(stderr, "%s:%d: ", progname, line);
    vfprintf(stderr, format, args);
    putc('\n', stderr);
    exit(rtn);
}


void fatal_e_(int rtn, const char* progname, int line,
        const char* format, ...)
{
    va_list args;
    va_start(args, format);
    fprintf(stderr, "%s:%d: ", progname, line);
    vfprintf(stderr, format, args);
    fprintf(stderr, " (%s)\n", strerror(errno));
    exit(rtn);
}
