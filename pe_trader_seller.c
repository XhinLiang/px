#include "pe_trader.h"

int sell_qty = 997;
void process_message(pid_t exchange_pid, const char *message)
{
    printf("[T%d]\tReceived message: %s\n", get_trader_id(), message);
    place_order(exchange_pid, SELL, "GPU", ++sell_qty, 888);
    if (sell_qty >= 1000)
    {
        // 之前的 order 可能会有丢失，丢失就会导致 trader 不自杀，这里 seller 自杀多发几次，保证 trader 收到 1000 qty 以上的，触发 trader 也自杀
        place_order(exchange_pid, SELL, "GPU", ++sell_qty, 888);
        place_order(exchange_pid, SELL, "GPU", ++sell_qty, 888);
        place_order(exchange_pid, SELL, "GPU", ++sell_qty, 888);
        place_order(exchange_pid, SELL, "GPU", ++sell_qty, 888);
        printf("[T%d]\tExiting, for sell_qty = %d\n", get_trader_id(), sell_qty);
        sleep(1);
        fflush(stdout);
        exit(0);
    }
}

int main(int argc, char *argv[])
{
    return main0(argc, argv, process_message);
}
