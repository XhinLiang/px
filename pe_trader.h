#ifndef PE_TRADER_H
#define PE_TRADER_H

#include <stdlib.h>
#include <stdbool.h>

#include "pe_common.h"

typedef struct
{
    int id;
    Order orders[MAX_ORDERS];
} Trader;

Trader *create_trader(int id);

void destroy_trader(Trader *trader);

bool place_order(Trader *trader, OrderType type, int product_id,
                 int quantity, int price);

bool cancel_order(Trader *trader, int order_id);

bool amend_order(Trader *trader, int order_id, int new_quantity,
                 int new_price);

#endif // PE_TRADER_H