#include "pe_trader.h"

void process_message(pid_t exchange_pid, const char *message)
{
    printf("[T%d]\tReceived message: %s\n", get_trader_id(), message);

    // MARKET SELL GPU 1 1000
    char source[MESSAGE_BUFF_SIZE];
    char order_type[MESSAGE_BUFF_SIZE];
    char product[MESSAGE_BUFF_SIZE];
    int quantity;
    int price;
    sscanf(message, "%s %s %s %d %d", source, order_type, product, &quantity, &price);

    // Checking if it's a SELL order
    if (strcmp(source, "MARKET") == 0 && strcmp(order_type, "SELL") == 0)
    {
        // Placing the opposite BUY order
        place_order(exchange_pid, BUY, product, quantity, price);

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
