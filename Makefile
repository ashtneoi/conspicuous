MAKEFLAGS += --no-builtin-rules

.SUFFIXES:
.SECONDEXPANSION:


EXE_SRC := cpic.c
SRC := $(EXE_SRC) bufman.c fail.c P16F1454.c

OBJ := $(SRC:%.c=%.o)
EXE := $(EXE_SRC:%.c=%)

CC := gcc
CFLAGS := -std=c99 -g -Wall -Wextra -Werror


all: $(EXE) $(EXTRA_EXE)

$(OBJ): $$(patsubst %.o,%.c,$$@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(EXE) $(EXTRA_EXE):
	$(CC) -o $@ $^

$(EXE): $$@.o

clean:
	rm -f $(OBJ) $(EXE)


bufman.o: bufman.h
cpic.o: bufman.h
fail.o: fail.h
P16F1454.o: P16F1454.h fail.h cpic.h utils.h

cpic: bufman.o fail.o P16F1454.o


.DEFAULT_GOAL := all
.PHONY: all clean
