#include "pe_trader.h"

void process_message(pid_t exchange_pid, const char *message)
{
    printf("[T%d]\tReceived message: %s\n", get_trader_id(), message);

    // Parsing the message
    char order_type[6];
    char item[16];
    int quantity;
    int price;
    sscanf(message, "%s %*s %s %d %d;", order_type, item, &quantity, &price);

    // Checking if it's a SELL order
    if (strcmp(order_type, "MARKET") == 0 && strcmp(item, "SELL") == 0)
    {
        // Placing the opposite BUY order
        place_order(exchange_pid, BUY, item, quantity, price);

        // Checking if quantity is greater than or equal to 1000
        if (quantity >= 1000)
        {
            printf("[T%d]\tExiting\n", get_trader_id());
            exit(0);
        }
    }
}

int main(int argc, char *argv[])
{
    return main0(argc, argv, process_message);
}
