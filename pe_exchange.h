#ifndef PE_EXCHANGE_H
#define PE_EXCHANGE_H

void mark_trader_disconnected(Exchange *exchange, pid_t pid);
int compare_sell_orders(const void *a, const void *b);
int compare_buy_orders(const void *a, const void *b);
void match_orders(Exchange *exchange);
void check_trader_status(Exchange *exchange);
void deal_orders(Exchange *exchange, char *product, Order *buy_order, Order *sell_order);
void report(Exchange *exchange);
void update_account(Trader *trader, int amount);
void handle_sigusr1(int sig, siginfo_t *info, void *context);
void process_trader_commands(Exchange *exchange, Trader *trader);
bool add_order(Exchange *exchange, int trader_id, OrderType type, int order_id, char *product, int quantity, int price);
bool cancel_order(Exchange *exchange, int trader_id, int order_id);
bool amend_order(Exchange *exchange, int trader_id, int order_id, int new_quantity, int new_price);
bool send_message_to_trader(Trader *trader, const char *message);

#endif // PE_EXCHANGE_H