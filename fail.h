#pragma once


#include "common.h"

#include <stdio.h>
#include <stdlib.h>


#define v0(...) do { vx_(__FILE__, __LINE__, __VA_ARGS__); } while (0)
#define v1(...) do { if (verbosity >= 1) vx_(__FILE__, __LINE__, \
                __VA_ARGS__); } while (0)
#define v2(...) do { if (verbosity >= 2) vx_(__FILE__, __LINE__, \
                __VA_ARGS__); } while (0)

#define warning(...) do { warning_(__FILE__, __LINE__, __VA_ARGS__); } \
                     while (0)
#define warning_e(...) do { warning_e_(__FILE__, __LINE__, __VA_ARGS__); } \
                       while (0)
#define fatal(rtn, ...) do { fatal_(rtn, __FILE__, __LINE__, \
                        __VA_ARGS__); } while (0)
#define fatal_e(rtn, ...) do { fatal_e_(rtn, __FILE__, __LINE__, \
                          __VA_ARGS__); } while (0)


void vx_(const char* srcname, int line, const char* format, ...);
void warning_(const char* srcname, int line, const char* format, ...);
void warning_e_(const char* srcname, int line, const char* format, ...);
void fatal_(int rtn, const char* srcname, int line, const char* format, ...);
void fatal_e_(int rtn, const char* srcname, int line,
    const char* format, ...);
