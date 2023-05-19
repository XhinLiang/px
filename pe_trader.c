#include "pe_trader.h"
#include "pe_common.h"

Trader *create_trader(int id)
{
    // 创建并初始化交易员
    // ...
}

void destroy_trader(Trader *trader)
{
    // 销毁交易员
    // ...
}

bool place_order(Trader *trader, OrderType type, int product_id,
                 int quantity, int price)
{
    // 下单
    // ...
}

bool cancel_order(Trader *trader, int order_id)
{
    // 取消订单
    // ...
}

bool amend_order(Trader *trader, int order_id, int new_quantity,
                 int new_price)
{
    // 修改订单
    // ...
}