#include "fail.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>


void vx_(const char* srcname, int line, const char* format, ...)
{
    fflush(stdout);
#ifdef DEBUG
    fprintf(stdout, "%s:%d: ", srcname, line);
#else
    (void)srcname; (void)line;
#endif
    va_list args;
    va_start(args, format);
    vfprintf(stdout, format, args);
    va_end(args);
    putchar('\n');
}


void warning_(const char* srcname, int line, const char* format, ...)
{
    fflush(stdout);
#ifdef DEBUG
    fprintf(stderr, "%s:%d: ", srcname, line);
#else
    (void)srcname; (void)line;
#endif
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    putc('\n', stderr);
}


void warning_e_(const char* srcname, int line, const char* format, ...)
{
    int e = errno;
    fflush(stdout);
#ifdef DEBUG
    fprintf(stderr, "%s:%d: ", srcname, line);
#else
    (void)srcname; (void)line;
#endif
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, " (%s)\n", strerror(e));
}


void fatal_(int rtn, const char* srcname, int line,
        const char* format, ...)
{
    fflush(stdout);
#ifdef DEBUG
    fprintf(stderr, "%s:%d: ", srcname, line);
#else
    (void)srcname; (void)line;
#endif
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    putc('\n', stderr);
    exit(rtn);
}


void fatal_e_(int rtn, const char* srcname, int line,
        const char* format, ...)
{
    int e = errno;
    fflush(stdout);
#ifdef DEBUG
    fprintf(stderr, "%s:%d: ", srcname, line);
#else
    (void)srcname; (void)line;
#endif
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, " (%s)\n", strerror(e));
    exit(rtn);
}
