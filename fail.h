#pragma once


#include <stdio.h>
#include <stdlib.h>


#define str(x) str_(x)
#define str_(x) #x
#define print(msg) do { fputs(msg, stdout); } while (0)
#define v0(...) do { vx_(progname, __LINE__, __VA_ARGS__); } while (0)
#define v1(...) do { if (verbosity >= 1) vx_(progname, __LINE__, \
                __VA_ARGS__); } while (0)
#define v2(...) do { if (verbosity >= 2) vx_(progname, __LINE__, \
                __VA_ARGS__); } while (0)
#define warning(...) do { warning_(progname, __LINE__, __VA_ARGS__); } \
                     while (0)
#define warning_e(...) do { warning_e_(progname, __LINE__, __VA_ARGS__); } \
                       while (0)
#define fatal(rtn, ...) do { fatal_(rtn, progname, __LINE__, \
                        __VA_ARGS__); } while (0)
#define fatal_e(rtn, ...) do { fatal_e_(rtn, progname, __LINE__, \
                          __VA_ARGS__); } while (0)


void vx_(const char* progname, int line, const char* format, ...);
void warning_(const char* progname, int line, const char* format, ...);
void warning_e_(const char* progname, int line, const char* format, ...);
void fatal_(int rtn, const char* progname, int line, const char* format, ...);
void fatal_e_(int rtn, const char* progname, int line,
    const char* format, ...);
