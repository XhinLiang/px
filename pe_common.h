#ifndef PE_COMMON_H
#define PE_COMMON_H

#include <stdlib.h>
#include <stdbool.h>

#define MAX_TRADERS 10
#define MAX_PRODUCTS 50
#define MAX_ORDERS 500

typedef enum
{
    BUY,
    SELL
} OrderType;

typedef struct
{
    int trader_id;
    OrderType type;
    int product_id;
    int quantity;
    int price;
} Order;

// 交易员的结构体
typedef struct
{
    int id;
    int pid;

    int num_orders;
    Order *orders[MAX_ORDERS];

    int balance;
} Trader;

// 交易所的结构体
typedef struct
{
    int num_traders;
    Trader *traders[MAX_TRADERS];

    int num_products;
    char *products[MAX_PRODUCTS];
} Exchange;

#endif // PE_COMMON_H