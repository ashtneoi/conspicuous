MAKEFLAGS += --no-builtin-rules
.SUFFIXES:

.SECONDEXPANSION:


EXE_SRC := cpic.c
SRC := $(EXE_SRC) bufman.c dict.c fail.c arch_emr.c

OBJ := $(SRC:%.c=%.o)
EXE := $(EXE_SRC:%.c=%)

CC := gcc
CFLAGS := -std=c99 -pedantic -g -Wall -Wextra -Werror -Wno-unused-function


all: $(EXE) $(EXTRA_EXE)

$(OBJ): $$(patsubst %.o,%.c,$$@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(EXE) $(EXTRA_EXE):
	$(CC) -o $@ $^

$(EXE): $$@.o

clean:
	rm -f $(OBJ) $(EXE)


bufman.o: bufman.h common.h
cpic.o: bufman.h common.h
dict.o: common.h dict.h fail.h
fail.o: fail.h common.h
arch_emr.o: arch_emr.h common.h fail.h cpic.h utils.h

cpic: bufman.o dict.o fail.o arch_emr.o


.DEFAULT_GOAL := all
.PHONY: all clean
