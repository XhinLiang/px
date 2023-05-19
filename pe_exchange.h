#ifndef PE_EXCHANGE_H
#define PE_EXCHANGE_H

#include <stdlib.h>
#include <stdbool.h>
#include "pe_common.h"

// 创建一个新的交易所
Exchange *create_exchange();

// 销毁一个交易所
void destroy_exchange(Exchange *exchange);

// 添加一个新的交易员
bool add_trader(Exchange *exchange, int id);

// 删除一个交易员
bool remove_trader(Exchange *exchange, int id);

// 添加一个新的订单
bool add_order(Exchange *exchange, int trader_id, OrderType type,
               int product_id, int quantity, int price);

// 删除一个订单
bool remove_order(Exchange *exchange, int trader_id, int order_id);

// 修改一个订单
bool amend_order(Exchange *exchange, int trader_id, int order_id,
                 int new_quantity, int new_price);

// 处理所有的订单
void process_orders(Exchange *exchange);

#endif // PE_EXCHANGE_H