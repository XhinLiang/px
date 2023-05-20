CC=gcc
CFLAGS=-Wall -Werror -Wvla -O0 -std=c11 -g
LDFLAGS=-lm
BINARIES=pe_exchange pe_trader

all: $(BINARIES)

.PHONY: clean
clean:
	rm -f $(BINARIES)
