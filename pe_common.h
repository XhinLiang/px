#ifndef PE_COMMON_H
#define PE_COMMON_H

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#define MAX_TRADERS 10
#define MAX_PRODUCTS 50
#define MAX_ORDERS 500
#define PIPE_NAME_MAX_SIZE 256
#define MESSAGE_BUFF_SIZE 256
#define MAX_PRICE 999999

typedef enum
{
    BUY,
    SELL
} OrderType;

typedef struct
{
    int id;         // Order ID
    int trader_id;  // Trader ID
    OrderType type; // Order type
    char *product;  // Product Name
    int quantity;   // Quantity
    int price;      // Price
    int timestamp;  // timestamp second
    bool canceled;
} Order;

// 交易员的结构体
typedef struct
{
    int id;
    pid_t pid;       // Trader进程的PID
    char *bin_path;  // Trader binary的路径
    int fd_exchange; // 到交易所的管道文件描述符
    int fd_trader;   // 从交易所的管道文件描述符
    int balance;
    bool disconnected;
} Trader;

// 交易所的结构体
typedef struct
{
    Trader **traders; // An array of pointers to Traders
    int num_traders;  // Number of traders

    char **products;  // An array of product names
    int num_products; // Number of products

    Order **orders; // An array of pointers to Orders
    int num_orders; // Number of orders

    int collected_fees;
} Exchange;

#endif // PE_COMMON_H