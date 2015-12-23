#pragma once


#define str(x) str_(x)
#define str_(x) #x
#define print(msg) do { fputs(msg, stdout); } while (0)
#define lengthof(x) (sizeof(x) / sizeof((x)[0]))
#define min(x, y) ((x) < (y) ? (x) : (y))
#define max(x, y) ((x) > (y) ? (x) : (y))
