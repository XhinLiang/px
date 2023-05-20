CC=gcc
CFLAGS=-Wall -Werror -Wvla -O0 -std=c11 -g
LDFLAGS=-lm
BINARIES=pe_exchange pe_trader pe_trader_seller

all: $(BINARIES)

pe_exchange: pe_exchange.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

pe_trader: pe_trader.c pe_trader_common.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

pe_trader_seller: pe_trader_seller.c pe_trader_common.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(BINARIES)