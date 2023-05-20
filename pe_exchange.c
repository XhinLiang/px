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

Exchange *ex; // 全局变量

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
            fprintf(stderr, "[PEX] Invalid product file format\n");
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
    printf("[PEX] Starting\n");
    // [PEX] Trading 2 products: GPU Router
    printf("[PEX] Trading %d products: ", exchange->num_products);
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
        fprintf(stderr, "[PEX] Failed to allocate memory for Trader\n");
        return NULL;
    }

    trader->id = id;

    trader->bin_path = strdup(bin_path);
    if (!trader->bin_path)
    {
        fprintf(stderr, "[PEX] Failed to duplicate bin_path\n");
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
            fprintf(stderr, "[PEX] Failed to create pipe to exchange\n");
            return NULL;
        }
        printf("[PEX] Created FIFO %s\n", pipe_exchange);
    }
    else
    {
        printf("[PEX] FIFO existed %s\n", pipe_exchange);
    }

    sprintf(pipe_trader, "/tmp/pe_trader_%d", id);
    int trader_fifo_exists = access(pipe_trader, F_OK); // 检查文件是否存在
    if (trader_fifo_exists != 0)
    {
        if (mkfifo(pipe_trader, 0666) == -1)
        {
            fprintf(stderr, "[PEX] Failed to create pipe from exchange\n");
            return NULL;
        }
        printf("[PEX] Created FIFO %s\n", pipe_trader);
    }
    else
    {
        printf("[PEX] FIFO existed %s\n", pipe_trader);
    }

    // 创建新的进程，然后使用exec()系列函数来运行trader程序
    pid_t pid = fork();
    if (pid == -1)
    {
        fprintf(stderr, "[PEX] Failed to fork new process\n");
        return NULL;
    }
    // Child process
    if (pid == 0)
    {
        char trader_id_str[16];
        sprintf(trader_id_str, "%d", id);
        execl(trader->bin_path, trader->bin_path, trader_id_str, (char *)NULL);
        fprintf(stderr, "[PEX] Failed to exec trader binary\n");
        exit(EXIT_FAILURE);
    }
    else
    { // Parent process
        trader->pid = pid;
    }
    printf("[PEX] Starting trader %d (%s)\n", id, bin_path);

    trader->fd_exchange = open(pipe_exchange, O_WRONLY);
    if (trader->fd_exchange == -1)
    {
        fprintf(stderr, "[PEX] Failed to open pipe to exchange\n");
        return NULL;
    }
    printf("[PEX] Connected to %s\n", pipe_exchange);

    trader->fd_trader = open(pipe_trader, O_RDONLY);
    if (trader->fd_trader == -1)
    {
        fprintf(stderr, "[PEX] Failed to open pipe from exchange\n");
        return NULL;
    }
    printf("[PEX] Connected to %s\n", pipe_trader);

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
            printf("[PEX] Trader %d(pid %d) disconnected\n", trader->id, trader->pid);
            trader->disconnected = true;
        }
    }
    fprintf(stderr, "[PEX] Try to remove trader pid %d, but not found\n", pid);
}

bool process_trader_commands(Exchange *exchange, Trader *trader)
{
    printf("[PEX]\tProcessing commands from trader %d\n", trader->id);
    FILE *file_exchange = fdopen(trader->fd_trader, "w");
    char command[1024];
    if (fgets(command, sizeof(command), file_exchange) == NULL)
    {
        fprintf(stderr, "[PEX] Failed to read from named pipe, %d", trader->id);
        return false;
    }
    // [T0] Parsing command: <BUY 0 GPU 30 500>
    printf("[T%d] Parsing command: %s", trader->id, command);

    char command_type[10];
    int order_id;
    char product[64];
    int qty;
    int price;

    // 解析命令字符串
    if (sscanf(command, "%s %d %s %d %d;", command_type, &order_id, product, &qty, &price) == 5)
    {
        if (strcmp(command_type, "BUY") == 0)
        {
            return add_order(exchange, trader->id, BUY, product, qty, price);
        }
        if (strcmp(command_type, "SELL") == 0)
        {
            return add_order(exchange, trader->id, SELL, product, qty, price);
        }
        if (strcmp(command_type, "AMEND") == 0)
        {
            return amend_order(exchange, trader->id, order_id, qty, price);
        }
        if (strcmp(command_type, "CANCEL") == 0)
        {
            return remove_order(exchange, trader->id, order_id);
        }
    }
    // 无法解析的命令
    return false;
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

void process_orders(Exchange *exchange)
{
    int max_orders_num = exchange->num_orders;
    Order **sell_orders = (Order **)malloc(max_orders_num * sizeof(Order *));
    Order **buy_orders = (Order **)malloc(max_orders_num * sizeof(Order *));
    int sell_orders_num = 0;
    int buy_orders_num = 0;

    while (true)
    {
        bool deal = false;
        for (int i = 0; i < exchange->num_products; i++)
        {
            if (deal)
            {
                break;
            }
            char *product = exchange->products[i];

            // 初始化 sell_orders & buy_orders
            memset(sell_orders, 0, max_orders_num * sizeof(Order *));
            memset(buy_orders, 0, max_orders_num * sizeof(Order *));
            sell_orders_num = 0;
            buy_orders_num = 0;

            // 遍历 exchange->orders，把合适的放入 sell_orders，并更新 sell_orders_num
            for (int j = 0; j < exchange->num_orders; j++)
            {
                Order *order = exchange->orders[j];
                if (order->canceled)
                {
                    continue;
                }
                if (strcmp(order->product, product) == 0 && order->type == SELL)
                {
                    sell_orders[sell_orders_num++] = order;
                }
            }

            // 遍历 exchange->orders，把合适的放入 buy_orders，并更新 buy_orders_num
            for (int j = 0; j < exchange->num_orders; j++)
            {
                Order *order = exchange->orders[j];
                if (order->canceled)
                {
                    continue;
                }
                if (strcmp(order->product, product) == 0 && order->type == BUY)
                {
                    buy_orders[buy_orders_num++] = order;
                }
            }

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
                        deal_orders(exchange, buy_order, sell_order);
                        deal = true;
                        break;
                    }
                }
            }
        }
        if (!deal)
        {
            break;
        }
    }

    // 清理 buy_orders & sell_orders
    free(sell_orders);
    free(buy_orders);

    monitor_traders(exchange);
}

void monitor_traders(Exchange *exchange)
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
            printf("[PEX] Trading completed\n");
            printf("[PEX] Exchange fees collected: $%d\n", exchange->collected_fees);
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
            // 没有任何子进程退出
            return;
        }
    }
}

void deal_orders(Exchange *exchange, Order *buy_order, Order *sell_order)
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

    // 更新交易员的账户状态
    Trader *buy_trader = exchange->traders[buy_order->trader_id];
    Trader *sell_trader = exchange->traders[sell_order->trader_id];
    update_account(buy_trader, -trade_price - fee);
    update_account(sell_trader, trade_price + fee);
    exchange->collected_fees += fee;

    // 更新订单数量
    buy_order->quantity -= trade_qty;
    sell_order->quantity -= trade_qty;

    // 如果订单数量为0，则取消订单
    if (buy_order->quantity == 0)
    {
        remove_order(exchange, buy_order->trader_id, buy_order->id);
    }
    if (sell_order->quantity == 0)
    {
        remove_order(exchange, sell_order->trader_id, sell_order->id);
    }

    char message[256];
    sprintf(message, "FILL %d %d;", buy_order->id, trade_qty);
    send_message_to_trader(exchange->traders[buy_order->trader_id], message);
    sprintf(message, "FILL %d %d;", sell_order->id, trade_qty);
    send_message_to_trader(exchange->traders[sell_order->trader_id], message);

    kill(buy_trader->pid, SIGUSR1);
    kill(sell_trader->pid, SIGUSR1);

    report(exchange);
}

void report(Exchange *exchange)
{
    printf("[PEX]\t--ORDERBOOK--\n");

    // Iterate over products
    for (int i = 0; i < exchange->num_products; i++)
    {
        char *product = exchange->products[i];
        int num_buy_levels = 0;
        int num_sell_levels = 0;

        printf("[PEX]\tProduct: %s; ", product);

        int min_price = 999999;
        int max_price = 0;
        int min_sell_price = 999999;

        // Count the number of buy and sell levels
        for (int j = 0; j < exchange->num_orders; j++)
        {
            Order *order = exchange->orders[j];
            if (order->canceled)
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
                if (order->canceled)
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
                if (order->canceled)
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
}

void update_account(Trader *trader, int amount)
{
    trader->balance += amount;
}

bool add_order(Exchange *exchange, int trader_id, OrderType type, char *product, int quantity, int price)
{
    // 创建新的Order并设置相关属性
    Order *order = (Order *)malloc(sizeof(Order));
    order->id = exchange->num_orders; // Assign a unique ID to the order
    order->trader_id = trader_id;
    order->type = type;
    order->product = product;
    order->quantity = quantity;
    order->price = price;

    // 将新的Order添加到Exchange的订单列表中
    exchange->orders = (Order **)realloc(exchange->orders, sizeof(Order *) * (exchange->num_orders + 1));
    exchange->orders[exchange->num_orders] = order;
    exchange->num_orders++;

    process_orders(exchange);
    return true;
}

bool remove_order(Exchange *exchange, int trader_id, int order_id)
{
    // 从Exchange的订单列表中找到并删除指定的Order
    for (int i = 0; i < exchange->num_orders; i++)
    {
        if (exchange->orders[i]->id == order_id && exchange->orders[i]->trader_id == trader_id)
        {
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
            exchange->orders[i]->quantity = new_quantity;
            exchange->orders[i]->price = new_price;
            process_orders(exchange);
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
    // TODO 统一用 \t 替换空格
    pid_t trader_pid = info->si_pid;
    printf("[PEX]\tReceived SIGUSR1 from trader: %d\n", trader_pid);
    // 获取发送信号的进程 ID
    Trader *trader = NULL;
    printf("[PEX]\tChecking traders, len %d\n", ex->num_traders);
    for (int i = 0; i < ex->num_traders; i++)
    {
        printf("[PEX]\tChecking trader %d with PID: %d\n", ex->traders[i]->id, ex->traders[i]->pid);
        if (ex->traders[i]->pid == trader_pid)
        {
            trader = ex->traders[i];
            break;
        }
    }
    if (trader == NULL)
    {
        printf("[PEX]\tERROR: Could not find trader with PID %d\n", trader_pid);
        return;
    }
    printf("[PEX]\tFound trader %d with PID: %d sent signal\n", trader->id, trader_pid);
    if (process_trader_commands(ex, trader))
    {
        process_orders(ex);
    }
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
    act.sa_sigaction = handle_sigusr1; // 设置信号处理函数
    act.sa_flags = SA_SIGINFO;         // 使用 sa_sigaction 而不是 sa_handler

    if (sigaction(SIGUSR1, &act, NULL) == -1)
    { // 将 SIGUSR1 信号与处理函数关联
        fprintf(stderr, "[PEX] sigaction");
        return 1;
    }

    // 读取产品文件
    char *product_file = argv[1];
    Exchange *exchange = create_exchange(product_file);
    if (exchange == NULL)
    {
        fprintf(stderr, "[PEX] Failed to load product file: %s\n", product_file);
        return 1;
    }

    // 创建交易员的数组，并初始化
    int num_traders = argc - 2;
    Trader **traders = malloc(sizeof(Trader *) * num_traders);
    for (int i = 0; i < num_traders; i++)
    {
        traders[i] = create_trader(i, argv[i + 2]);
        if (!traders[i])
        {
            fprintf(stderr, "[PEX] Failed to create trader: %s, %d\n", argv[i + 2], i);
            return 1;
        }
        else
        {
            printf("[PEX] Created trader: %s, %d\n", argv[i + 2], i);
        }
    }
    exchange->num_traders = num_traders;
    exchange->traders = traders;

    // 向所有的交易员发送"MARKET OPEN"消息，并发送SIGUSR1信号
    for (int i = 0; i < num_traders; i++)
    {
        if (!send_message_to_trader(traders[i], "MARKET OPEN;"))
        {
            fprintf(stderr, "[PEX] Failed to send message to trader: %d\n", i);
            return 1;
        }
        else
        {
            printf("[PEX] sent 'MARKET OPEN;' to trader %d\n", i);
        }
    }
    for (int i = 0; i < num_traders; i++)
    {
        if (kill(traders[i]->pid, SIGUSR1) == -1)
        {
            fprintf(stderr, "[PEX] Failed to send signal to trader: %d\n", i);
            return 1;
        }
        else
        {
            printf("[PEX] sent SIGUSR1 to trader %d\n", i);
        }
    }

    while (1)
    {
        pause(); // 暂停，等待信号
    }
    return 0;
}