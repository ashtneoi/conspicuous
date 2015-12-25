#include "fail.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>


void vx_(const char* srcname, int line, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    fflush(stdout);
    fprintf(stdout, "%s:%d: ", srcname, line);
    vfprintf(stdout, format, args);
    putchar('\n');
}


void warning_(const char* srcname, int line, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    fflush(stdout);
    fprintf(stderr, "%s:%d: ", srcname, line);
    vfprintf(stderr, format, args);
    putc('\n', stderr);
}


void warning_e_(const char* srcname, int line, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    fflush(stdout);
    fprintf(stderr, "%s:%d: ", srcname, line);
    vfprintf(stderr, format, args);
    fprintf(stderr, " (%s)\n", strerror(errno));
}


void fatal_(int rtn, const char* srcname, int line,
        const char* format, ...)
{
    va_list args;
    va_start(args, format);
    fflush(stdout);
    fprintf(stderr, "%s:%d: ", srcname, line);
    vfprintf(stderr, format, args);
    putc('\n', stderr);
    exit(rtn);
}


void fatal_e_(int rtn, const char* srcname, int line,
        const char* format, ...)
{
    va_list args;
    va_start(args, format);
    fflush(stdout);
    fprintf(stderr, "%s:%d: ", srcname, line);
    vfprintf(stderr, format, args);
    fprintf(stderr, " (%s)\n", strerror(errno));
    exit(rtn);
}
