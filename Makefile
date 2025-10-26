SRCS := $(wildcard src/*.c)
CFLAGS := -Wall -O0 -g -std=c99 -Wno-switch -Wdeprecated-declarations
ifeq ($(OS),Windows_NT)
	EXE := .EXE
	DYLIB := .dll
else
	EXE :=
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Darwin)
		DYLIB := .dylib
	else
		DYLIB := .so
	endif
endif
EXE_OUT := jcc$(EXE)
LIB_OUT := libjcc$(DYLIB)

default: $(EXE_OUT)

$(EXE_OUT): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^

$(LIB_OUT): $(SRCS)
	$(CC) -fpic -shared -o $@ $(filter-out src/main.c, $^) $(CFLAGS)

test: clean $(EXE_OUT)
	@./run_tests.fish

all: clean $(EXE_OUT) $(LIB_OUT) test docs

docs:
	@headerdoc2html src/jcc.h -o docs/; \
	mv docs/jcc_h/*.html docs/; \
	rm -rf docs/jcc_h

clean:
	@$(RM) -f $(EXE_OUT) $(LIB_OUT)

.PHONY: default test clean docs all
