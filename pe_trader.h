#ifndef PE_TRADER_H
#define PE_TRADER_H

#include <stdlib.h>
#include <stdbool.h>

#include "pe_common.h"

void place_order(pid_t exchange_pid, OrderType order_type, const char *item, int quantity, int price);
void process_message(pid_t exchange_pid, const char *message);
void handle_sigusr1(int sig, siginfo_t *info, void *context);

#endif // PE_TRADER_H