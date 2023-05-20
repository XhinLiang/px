#include "pe_trader.h"

int price = 998;
void process_message(pid_t exchange_pid, const char *message)
{
    printf("[T%d]\tReceived message: %s\n", get_trader_id(), message);
    if (price < 1001)
    {
        place_order(exchange_pid, SELL, "GPU", 1, price++);
    }
}

int main(int argc, char *argv[])
{
    return main0(argc, argv, process_message);
}
