#include "pe_common.h"
#include "pe_exchange.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>

pthread_mutex_t mutex; // 定义互斥锁
Exchange *exg;         // 全局变量

int min(int a, int b)
{
    return a < b ? a : b;
}

void destroy_trader(Trader *trader)
{
    if (trader->bin_path)
    {
        free(trader->bin_path);
    }
    free(trader);
}

void destroy_exchange(Exchange *exchange)
{
    for (int i = 0; i < exchange->num_traders; i++)
    {
        destroy_trader(exchange->traders[i]);
    }

    free(exchange->products);
    free(exchange->orders);
    free(exchange);
}

Exchange *create_exchange(const char *product_file)
{

    // 从文件中读取产品信息
    FILE *file = fopen(product_file, "r");
    if (file == NULL)
    {
        perror("Unable to open product file");
        return NULL;
    }

    int num_products = 0;
    // 读取产品数量
    if (fscanf(file, "%d\n", &num_products) != 1)
    {
        perror("Invalid product file format");
        fclose(file);
        return NULL;
    }

    Exchange *exchange = (Exchange *)malloc(sizeof(Exchange));

    // 初始化交易所
    exchange->traders = NULL;
    exchange->num_traders = 0;
    exchange->products = NULL;
    exchange->num_products = num_products;

    // 分配存储产品名称的空间
    exchange->products = (char **)malloc(sizeof(char *) * exchange->num_products);

    // 读取每一个产品的名称
    char line[256];
    for (int i = 0; i < exchange->num_products; i++)
    {
        if (fgets(line, sizeof(line), file) == NULL)
        {
            fprintf(stderr, "[PEX]\tInvalid product file format\n");
            fclose(file);
            destroy_exchange(exchange);
            return NULL;
        }

        // 删除末尾的换行符
        line[strcspn(line, "\n")] = 0;

        // 存储产品名称
        exchange->products[i] = strdup(line);
    }

    fclose(file);
    // [PEX] Starting
    printf("[PEX]\tStarting\n");
    // [PEX] Trading 2 products: GPU Router
    printf("[PEX]\tTrading %d products: ", exchange->num_products);
    for (int i = 0; i < exchange->num_products; i++)
    {
        printf("%s ", exchange->products[i]);
    }
    printf("\n");

    return exchange;
}

Trader *create_trader(int id, const char *bin_path)
{
    // 创建新的Trader并设置相关属性
    Trader *trader = (Trader *)malloc(sizeof(Trader));
    if (!trader)
    {
        fprintf(stderr, "[PEX]\tFailed to allocate memory for Trader\n");
        return NULL;
    }

    trader->id = id;

    trader->bin_path = strdup(bin_path);
    if (!trader->bin_path)
    {
        fprintf(stderr, "[PEX]\tFailed to duplicate bin_path\n");
        return NULL;
    }

    // 创建 Named Pipe
    char pipe_exchange[PIPE_NAME_MAX_SIZE], pipe_trader[PIPE_NAME_MAX_SIZE];

    sprintf(pipe_exchange, "/tmp/pe_exchange_%d", id);
    int exchange_fifo_exists = access(pipe_exchange, F_OK); // 检查文件是否存在
    if (exchange_fifo_exists != 0)
    {
        // TODO check if fifo existed
        if (mkfifo(pipe_exchange, 0666) == -1)
        {
            fprintf(stderr, "[PEX]\tFailed to create pipe to exchange\n");
            return NULL;
        }
        printf("[PEX]\tCreated FIFO %s\n", pipe_exchange);
    }
    else
    {
        printf("[PEX]\tFIFO existed %s\n", pipe_exchange);
    }

    sprintf(pipe_trader, "/tmp/pe_trader_%d", id);
    int trader_fifo_exists = access(pipe_trader, F_OK); // 检查文件是否存在
    if (trader_fifo_exists != 0)
    {
        if (mkfifo(pipe_trader, 0666) == -1)
        {
            fprintf(stderr, "[PEX]\tFailed to create pipe from exchange\n");
            return NULL;
        }
        printf("[PEX]\tCreated FIFO %s\n", pipe_trader);
    }
    else
    {
        printf("[PEX]\tFIFO existed %s\n", pipe_trader);
    }

    // 创建新的进程，然后使用exec()系列函数来运行trader程序
    pid_t pid = fork();
    if (pid == -1)
    {
        fprintf(stderr, "[PEX]\tFailed to fork new process\n");
        return NULL;
    }
    // Child process
    if (pid == 0)
    {
        char trader_id_str[16];
        sprintf(trader_id_str, "%d", id);
        char pid_str[16];
        sprintf(pid_str, "%d", getpid());
        execl(trader->bin_path, trader->bin_path, trader_id_str, pid_str, (char *)NULL);
        fprintf(stderr, "[PEX]\tFailed to exec trader binary\n");
        return NULL;
    }
    else
    { // Parent process
        trader->pid = pid;
    }
    printf("[PEX]\tStarting trader %d (%s)\n", id, bin_path);

    trader->fd_exchange = open(pipe_exchange, O_WRONLY);
    if (trader->fd_exchange == -1)
    {
        fprintf(stderr, "[PEX]\tFailed to open pipe to exchange\n");
        return NULL;
    }
    printf("[PEX]\tConnected to %s\n", pipe_exchange);

    trader->fd_trader = open(pipe_trader, O_RDONLY);
    if (trader->fd_trader == -1)
    {
        fprintf(stderr, "[PEX]\tFailed to open pipe from exchange\n");
        return NULL;
    }
    printf("[PEX]\tConnected to %s\n", pipe_trader);

    return trader;
}

void remove_trader(Exchange *exchange, pid_t pid)
{
    for (int i = 0; i < exchange->num_traders; i++)
    {
        Trader *trader = exchange->traders[i];
        if (trader->pid == pid)
        {
            // 子进程已退出，打印消息
            printf("[PEX]\tTrader %d(pid %d) disconnected\n", trader->id, trader->pid);
            trader->disconnected = true;
            return;
        }
    }
    fprintf(stderr, "[PEX]\tTry to remove trader pid %d, but not found\n", pid);
}

void ack_message(Exchange *exchange, Trader *trader, char *command_type, int order_id, char *product, int qty, int price)
{
    char ack_message[1024];
    sprintf(ack_message, "ACCEPTED %d;", order_id);
    sleep(1);
    send_message_to_trader(trader, ack_message);
    kill(trader->pid, SIGUSR1);

    // Exchange writes to all other traders
    char other_trader_message[1024];
    // MARKET SELL CPU 2 300;
    sprintf(other_trader_message, "MARKET %s %s %d %d;", command_type, product, qty, price);
    for (int i = 0; i < exchange->num_traders; i++)
    {
        Trader *other_trader = exchange->traders[i];
        if (other_trader->id == trader->id || other_trader->disconnected)
        {
            continue;
        }
        send_message_to_trader(other_trader, other_trader_message);
    }
    for (int i = 0; i < exchange->num_traders; i++)
    {
        Trader *other_trader = exchange->traders[i];
        if (other_trader->id == trader->id || other_trader->disconnected)
        {
            continue;
        }
        sleep(1);
        kill(other_trader->pid, SIGUSR1);
    }
}

void process_trader_commands(Exchange *exchange, Trader *trader)
{
    printf("[PEX]\tProcessing commands from trader %d\n", trader->id);
    FILE *file_exchange = fdopen(trader->fd_trader, "r");
    char command[1024];
    if (fgets(command, sizeof(command), file_exchange) == NULL)
    {
        return;
    }
    // [T0] Parsing command: <BUY 0 GPU 30 500>
    char command_type[10];
    int order_id;
    char product[64];
    int qty;
    int price;

    // 解析命令字符串
    if (sscanf(command, "%s %d %s %d %d;", command_type, &order_id, product, &qty, &price) == 5)
    {
        printf("[PEX]\tParsed command: %s %d %s %d %d\n", command_type, order_id, product, qty, price);
        if (strcmp(command_type, "SELL") == 0 || strcmp(command_type, "BUY") == 0)
        {
            OrderType type = (strcmp(command_type, "SELL") == 0) ? SELL : BUY;
            if (add_order(exchange, trader->id, type, order_id, product, qty, price))
            {
                match_orders(exchange);
                ack_message(exchange, trader, command_type, order_id, product, qty, price);
            }
            return;
        }
        if (strcmp(command_type, "AMEND") == 0)
        {
            if (amend_order(exchange, trader->id, order_id, qty, price))
            {
                match_orders(exchange);
                ack_message(exchange, trader, command_type, order_id, product, qty, price);
            }
            return;
        }
        if (strcmp(command_type, "CANCEL") == 0)
        {
            if (cancel_order(exchange, trader->id, order_id))
            {
                ack_message(exchange, trader, command_type, order_id, product, 0, 0);
            }
            return;
        }
        fprintf(stderr, "[PEX]\tUnknown command type: %s", command_type);
        return;
    }
    fprintf(stderr, "[PEX]\tFailed to parse command: %s", command);
}

int compare_sell_orders(const void *a, const void *b)
{
    Order *order_a = *(Order **)a;
    Order *order_b = *(Order **)b;
    if (order_a->price != order_b->price)
    {
        return order_a->price - order_b->price;
    }
    else
    {
        return order_a->timestamp - order_b->timestamp;
    }
}

int compare_buy_orders(const void *a, const void *b)
{
    Order *order_a = *(Order **)a;
    Order *order_b = *(Order **)b;
    if (order_a->price != order_b->price)
    {
        return order_b->price - order_a->price;
    }
    else
    {
        return order_a->timestamp - order_b->timestamp;
    }
}

void match_orders(Exchange *exchange)
{
    int max_orders_num = exchange->num_orders;
    Order **sell_orders = (Order **)malloc(max_orders_num * sizeof(Order *));
    Order **buy_orders = (Order **)malloc(max_orders_num * sizeof(Order *));
    int sell_orders_num = 0;
    int buy_orders_num = 0;

    while (true)
    {
        printf("[PEX]\tMatching orders, orders num: %d\n", exchange->num_orders);
        bool deal = false;
        for (int i = 0; i < exchange->num_products; i++)
        {
            if (deal)
            {
                break;
            }
            char *product = exchange->products[i];
            printf("[PEX]\tMatching orders for product %s\n", product);

            // 初始化 sell_orders & buy_orders
            memset(sell_orders, 0, max_orders_num * sizeof(Order *));
            memset(buy_orders, 0, max_orders_num * sizeof(Order *));
            sell_orders_num = 0;
            buy_orders_num = 0;

            // 遍历 exchange->orders，把合适的放入 sell_orders，并更新 sell_orders_num
            for (int j = 0; j < exchange->num_orders; j++)
            {
                Order *order = exchange->orders[j];
                if (order->canceled || order->quantity <= 0)
                {
                    continue;
                }
                if (strcmp(order->product, product) == 0 && order->type == SELL)
                {
                    sell_orders[sell_orders_num++] = order;
                }
            }
            if (sell_orders_num == 0)
            {
                printf("[PEX]\tNo sell orders for product %s\n", product);
                continue;
            }
            printf("[PEX]\t%s Sell orders: %d\n", product, sell_orders_num);

            // 遍历 exchange->orders，把合适的放入 buy_orders，并更新 buy_orders_num
            for (int j = 0; j < exchange->num_orders; j++)
            {
                Order *order = exchange->orders[j];
                if (order->canceled || order->quantity <= 0)
                {
                    continue;
                }
                if (strcmp(order->product, product) == 0 && order->type == BUY)
                {
                    buy_orders[buy_orders_num++] = order;
                }
            }
            if (buy_orders_num == 0)
            {
                printf("[PEX]\tNo buy orders for product %s\n", product);
                continue;
            }
            printf("[PEX]\t%s Buy orders: %d\n", product, buy_orders_num);

            // 按价格排序
            qsort(sell_orders, sell_orders_num, sizeof(Order *), compare_sell_orders);
            qsort(buy_orders, buy_orders_num, sizeof(Order *), compare_buy_orders);

            // 按顺序遍历 sell_orders，看是否对 sell_order 能匹配到 buy_order
            for (int m = 0; m < sell_orders_num; m++)
            {
                Order *sell_order = sell_orders[m];
                if (deal)
                {
                    break;
                }
                for (int n = 0; n < buy_orders_num; n++)
                {
                    Order *buy_order = buy_orders[n];
                    if (buy_order->price >= sell_order->price)
                    {
                        deal_orders(exchange, product, buy_order, sell_order);
                        deal = true;
                        break;
                    }
                }
            }
        }
        printf("[PEX]\tMatching orders completed, dealed: %d\n", deal);
        if (!deal)
        {
            break;
        }
    }

    // 释放 sell_orders
    for (int i = 0; i < max_orders_num; i++)
    {
        if (sell_orders[i] != NULL)
        {
            free(sell_orders[i]);
        }
    }
    free(sell_orders);

    // 释放 buy_orders
    for (int i = 0; i < max_orders_num; i++)
    {
        if (buy_orders[i] != NULL)
        {
            free(buy_orders[i]);
        }
    }
    free(buy_orders);
}

void deal_orders(Exchange *exchange, char *product, Order *buy_order, Order *sell_order)
{
    // 交易
    int trade_qty = min(buy_order->quantity, sell_order->quantity);
    int each_price = buy_order->price;
    if (sell_order->timestamp > buy_order->timestamp)
    {
        each_price = sell_order->price;
    }
    int trade_price = each_price * trade_qty;
    int fee = (double)((double)trade_price * 0.01 + 0.5);

    printf("[PEX]\tDealing %s orders: buy %d@%d, sell %d@%d, each %d, qty %d, fee %d\n",
           product, buy_order->id, buy_order->trader_id, sell_order->id, sell_order->trader_id, each_price, trade_qty, fee);

    // 更新交易员的账户状态
    Trader *buy_trader = exchange->traders[buy_order->trader_id];
    Trader *sell_trader = exchange->traders[sell_order->trader_id];
    update_account(buy_trader, -trade_price - fee);
    update_account(sell_trader, trade_price + fee);
    exchange->collected_fees += fee;

    // 更新订单数量
    buy_order->quantity -= trade_qty;
    sell_order->quantity -= trade_qty;

    char message[256];
    sprintf(message, "FILL %d %d;", buy_order->id, trade_qty);
    send_message_to_trader(exchange->traders[buy_order->trader_id], message);
    sprintf(message, "FILL %d %d;", sell_order->id, trade_qty);
    send_message_to_trader(exchange->traders[sell_order->trader_id], message);

    sleep(1);
    kill(buy_trader->pid, SIGUSR1);
    sleep(1);
    kill(sell_trader->pid, SIGUSR1);

    report(exchange);
}

void report(Exchange *exchange)
{
    printf("[PEX]\n");
    printf("[PEX]\t--ORDERBOOK--\n");

    // Iterate over products
    for (int i = 0; i < exchange->num_products; i++)
    {
        char *product = exchange->products[i];
        int num_buy_levels = 0;
        int num_sell_levels = 0;

        printf("[PEX]\tProduct: %s; ", product);

        int min_price = MAX_PRICE; // TODO
        int max_price = 0;
        int min_sell_price = MAX_PRICE;

        // Count the number of buy and sell levels
        for (int j = 0; j < exchange->num_orders; j++)
        {
            Order *order = exchange->orders[j];
            if (order->canceled || order->quantity == 0)
            {
                continue;
            }
            if (strcmp(order->product, product) == 0)
            {
                if (order->price < min_price)
                {
                    min_price = order->price;
                }
                if (order->price > max_price)
                {
                    max_price = order->price;
                }
                if (order->type == BUY)
                {
                    num_buy_levels++;
                }
                else if (order->type == SELL)
                {
                    if (order->price < min_sell_price)
                    {
                        min_sell_price = order->price;
                    }
                    num_sell_levels++;
                }
            }
        }

        printf("Buy levels: %d; Sell levels: %d\n", num_buy_levels, num_sell_levels);

        // Print the orders for the current product
        for (int level = max_price; level >= min_price; level--)
        {
            int num_orders_at_level = 0;

            // Count the number of orders at the current level
            for (int j = 0; j < exchange->num_orders; j++)
            {
                Order *order = exchange->orders[j];
                if (order->canceled || order->quantity == 0)
                {
                    continue;
                }
                if (strcmp(order->product, product) == 0 && order->price == level)
                {
                    num_orders_at_level++;
                }
            }

            if (num_orders_at_level > 0)
            {
                printf("[PEX]\t\t%s %d @ $%d (%d order%s)\n",
                       (level >= min_sell_price) ? "SELL" : "BUY",
                       num_orders_at_level,
                       level,
                       num_orders_at_level,
                       (num_orders_at_level > 1) ? "s" : "");
            }
        }
    }

    printf("[PEX]\t--POSITIONS--\n");

    // Iterate over traders
    for (int i = 0; i < exchange->num_traders; i++)
    {
        Trader *trader = exchange->traders[i];
        printf("[PEX]\tTrader %d: ", trader->id);

        // Iterate over trader's positions
        for (int j = 0; j < exchange->num_products; j++)
        {
            char *product = exchange->products[j];
            int position = 0;

            int total_value = 0;
            // Calculate trader's position for the current product
            for (int k = 0; k < exchange->num_orders; k++)
            {
                Order *order = exchange->orders[k];
                if (order->canceled || order->quantity == 0)
                {
                    continue;
                }
                if (order->trader_id == trader->id && strcmp(order->product, product) == 0)
                {
                    if (order->type == BUY)
                    {
                        position -= order->quantity;
                        total_value -= order->quantity * order->price;
                    }
                    else if (order->type == SELL)
                    {
                        position += order->quantity;
                        total_value += order->quantity * order->price;
                    }
                }
            }

            printf("%s %d ($%d), ", product, position, total_value);
        }

        printf("\n");
    }
    printf("[PEX]\n");
}

void update_account(Trader *trader, int amount)
{
    printf("[PEX]\tUpdating trader %d balance, before: %d, after: %d\n", trader->id, trader->balance, trader->balance + amount);
    trader->balance += amount;
}

bool add_order(Exchange *exchange, int trader_id, OrderType type, int order_id, char *product, int quantity, int price)
{
    // 创建新的Order并设置相关属性
    Order *order = (Order *)malloc(sizeof(Order));
    order->id = order_id;
    order->trader_id = trader_id;
    order->type = type;
    order->product = product;
    order->quantity = quantity;
    order->price = price;

    // 将新的Order添加到Exchange的订单列表中
    exchange->orders = (Order **)realloc(exchange->orders, sizeof(Order *) * (exchange->num_orders + 1));
    exchange->orders[exchange->num_orders] = order;
    exchange->num_orders++;

    printf("[PEX]\tNew order id: %d, trader: %d, total orders: %d\n", order_id, trader_id, exchange->num_orders);
    return true;
}

bool cancel_order(Exchange *exchange, int trader_id, int order_id)
{
    // 从Exchange的订单列表中找到并删除指定的Order
    for (int i = 0; i < exchange->num_orders; i++)
    {
        if (exchange->orders[i]->id == order_id && exchange->orders[i]->trader_id == trader_id)
        {
            printf("[PEX]\tCancelling order id: %d, trader: %d\n", order_id, trader_id);
            exchange->orders[i]->canceled = true;
            return true;
        }
    }
    // 如果找不到指定的Order，返回false
    return false;
}

bool amend_order(Exchange *exchange, int trader_id, int order_id, int new_quantity, int new_price)
{
    // 从Exchange的订单列表中找到并修改指定的Order
    for (int i = 0; i < exchange->num_orders; i++)
    {
        if (exchange->orders[i]->id == order_id && exchange->orders[i]->trader_id == trader_id)
        {
            printf("[PEX]\tAmending order id: %d, trader: %d, qty %d -> %d, price %d -> %d\n",
                   order_id, trader_id, exchange->orders[i]->quantity, new_quantity, exchange->orders[i]->price, new_price);
            exchange->orders[i]->quantity = new_quantity;
            exchange->orders[i]->price = new_price;
            return true;
        }
    }
    // 如果找不到指定的Order，返回false
    return false;
}

bool send_message_to_trader(Trader *trader, const char *message)
{
    size_t message_len = strlen(message);
    ssize_t bytes_written = write(trader->fd_exchange, message, message_len);
    return bytes_written == message_len;
}

void handle_sigusr1(int sig, siginfo_t *info, void *context)
{
    pthread_mutex_lock(&mutex); // 加锁
    pid_t trader_pid = info->si_pid;
    if (trader_pid <= 0)
    {
        printf("[PEX]\t Received invalid PID: %d, iterate all of the traders\n", trader_pid);
        for (int i = 0; i < exg->num_traders; i++)
        {
            Trader *trader = exg->traders[i];
            process_trader_commands(exg, trader);
        }
        pthread_mutex_unlock(&mutex); // 解锁
        return;
    }
    printf("[PEX]\tReceived SIGUSR1 from pid: %d\n", trader_pid);
    // 获取发送信号的进程 ID
    Trader *trader = NULL;
    for (int i = 0; i < exg->num_traders; i++)
    {
        printf("[PEX]\tChecking trader %d with PID: %d\n", exg->traders[i]->id, exg->traders[i]->pid);
        if (exg->traders[i]->pid == trader_pid)
        {
            trader = exg->traders[i];
            break;
        }
    }
    if (trader == NULL)
    {
        printf("[PEX]\tERROR: Could not find trader with PID %d\n", trader_pid);
        pthread_mutex_unlock(&mutex); // 解锁
        return;
    }
    printf("[PEX]\tFound trader %d with PID: %d sent signal\n", trader->id, trader_pid);
    process_trader_commands(exg, trader);
    pthread_mutex_unlock(&mutex); // 解锁
    return;
}

void check_trader_status(Exchange *exchange)
{
    int status;
    while (true)
    {
        bool all_disconnected = true;
        for (int i = 0; i < exchange->num_traders; i++)
        {
            Trader *trader = exchange->traders[i];
            if (!trader->disconnected)
            {
                all_disconnected = false;
                break;
            }
        }
        if (all_disconnected)
        {
            printf("[PEX]\n");
            printf("[PEX]\tTrading completed\n");
            printf("[PEX]\tExchange fees collected: $%d\n", exchange->collected_fees);
            sleep(1);
            fflush(stdout);
            exit(0);
            return;
        }

        pid_t pid = waitpid(-1, &status, WNOHANG);
        if (pid > 0)
        {
            remove_trader(exchange, pid);
        }
        else
        {
            return;
        }
    }
}

void *check_trader_status_intervally(void *arg)
{
    while (1)
    {
        check_trader_status(exg);
        // Sleep for 10 seconds
        sleep(10);
    }
    // This will never be reached, as the thread runs indefinitely
    return NULL;
}

int main(int argc, char *argv[])
{
    // 检查参数数量
    if (argc < 3)
    {
        fprintf(stderr, "Usage: %s [product file] [trader 0] [trader 1] ... [trader n]\n", argv[0]);
        return 1;
    }

    struct sigaction act;
    act.sa_sigaction = handle_sigusr1;
    act.sa_flags = SA_SIGINFO;

    if (sigaction(SIGUSR1, &act, NULL) == -1)
    { // 将 SIGUSR1 信号与处理函数关联
        fprintf(stderr, "[PEX]\tSigaction error");
        return 1;
    }

    // 读取产品文件
    char *product_file = argv[1];
    exg = create_exchange(product_file);
    if (exg == NULL)
    {
        fprintf(stderr, "[PEX]\tFailed to load product file: %s\n", product_file);
        return 1;
    }

    // 创建交易员的数组，并初始化
    exg->num_traders = argc - 2;
    exg->traders = malloc(sizeof(Trader *) * exg->num_traders);
    for (int i = 0; i < exg->num_traders; i++)
    {
        exg->traders[i] = create_trader(i, argv[i + 2]);
        if (!exg->traders[i])
        {
            fprintf(stderr, "[PEX]\tFailed to create trader: %s, %d\n", argv[i + 2], i);
            return 1;
        }
        else
        {
            printf("[PEX]\tCreated trader: %s, %d\n", argv[i + 2], i);
        }
    }
    printf("[PEX]\tCreated %d traders\n", exg->num_traders);

    // 向所有的交易员发送"MARKET OPEN"消息，并发送SIGUSR1信号
    for (int i = 0; i < exg->num_traders; i++)
    {
        if (!send_message_to_trader(exg->traders[i], "MARKET OPEN;"))
        {
            fprintf(stderr, "[PEX]\tFailed to send message to trader: %d\n", i);
            return 1;
        }
        else
        {
            printf("[PEX]\tsent 'MARKET OPEN;' to trader %d\n", i);
        }
    }
    for (int i = 0; i < exg->num_traders; i++)
    {
        sleep(1);
        if (kill(exg->traders[i]->pid, SIGUSR1) == -1)
        {
            fprintf(stderr, "[PEX]\tFailed to send signal to trader: %d\n", i);
            return 1;
        }
        else
        {
            printf("[PEX]\tsent SIGUSR1 to trader %d\n", i);
        }
    }

    pthread_t thread;

    // Create a thread to run the check_trader_status function
    if (pthread_create(&thread, NULL, check_trader_status_intervally, NULL) != 0)
    {
        fprintf(stderr, "Failed to create thread\n");
        return 1;
    }

    while (1)
    {
        pause(); // 暂停，等待信号
    }
    // Wait for the thread to finish (which will never happen in this case)
    pthread_join(thread, NULL);

    printf("[PEX]\tMarket closed unintentionally\n");
    fflush(stdout);
    return 1;
}