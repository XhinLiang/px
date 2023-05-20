#ifndef PE_TRADER_COMMON_H
#define PE_TRADER_COMMON_H

#include "pe_common.h"

void (*message_handler)(pid_t exchange_pid, const char *message);

void place_order(pid_t exchange_pid, OrderType order_type, const char *item, int quantity, int price);
void process_message(pid_t exchange_pid, const char *message);
void handle_sigusr1(int sig, siginfo_t *info, void *context);
int main0(int argc, char *argv[], void (*handler)(pid_t exchange_pid, const char *message));
int get_trader_id();
int get_exchange_pid();

#endif // PE_TRADER_COMMON_H