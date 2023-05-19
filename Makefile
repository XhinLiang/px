CC = gcc
CFLAGS = -Wall -Wextra -std=c11
LDFLAGS = 

COMMON_SRCS = pe_common.c
COMMON_OBJS = $(COMMON_SRCS:.c=.o)

EXCHANGE_SRCS = pe_exchange.c
EXCHANGE_OBJS = $(EXCHANGE_SRCS:.c=.o)
EXCHANGE_EXEC = pe_exchange

TRADER_SRCS = pe_trader.c
TRADER_OBJS = $(TRADER_SRCS:.c=.o)
TRADER_EXEC = pe_trader

all: $(EXCHANGE_EXEC) $(TRADER_EXEC)

$(EXCHANGE_EXEC): $(COMMON_OBJS) $(EXCHANGE_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

$(TRADER_EXEC): $(COMMON_OBJS) $(TRADER_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean

clean:
	rm -f $(COMMON_OBJS) $(EXCHANGE_OBJS) $(TRADER_OBJS) $(EXCHANGE_EXEC) $(TRADER_EXEC)
