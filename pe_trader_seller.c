#include "pe_trader.h"

int sell_qty = 998;
void process_message(pid_t exchange_pid, const char *message)
{
    printf("[T%d]\tReceived message: %s\n", get_trader_id(), message);
    if (sell_qty <= 1008)
    {
        place_order(exchange_pid, SELL, "GPU", sell_qty++, 888);
    } else {
        printf("[T%d]\tExiting, for sell_qty = %d\n", get_trader_id(), sell_qty);
        sleep(5);
        exit(0);
    }
}

int main(int argc, char *argv[])
{
    return main0(argc, argv, process_message);
}
