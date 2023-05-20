#include "pe_trader.h"
#include "pe_common.h"
#include <stdio.h>
#include <fcntl.h>
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

int trader_id;
int current_order_id = 1;

// 为此交易者创建Named Pipes
char pipe_exchange[PIPE_NAME_MAX_SIZE];
char pipe_trader[PIPE_NAME_MAX_SIZE];

int fd_exchange;
int fd_trader;

void place_order(pid_t exchange_pid, OrderType order_type, const char *item, int quantity, int price)
{
    char message[MESSAGE_BUFF_SIZE];
    sprintf(message, "%d %s %s %d %d\n", current_order_id++, order_type == BUY ? "BUY" : "SELL", item, quantity, price);
    printf("[T%d]\tSending message: %s\n", trader_id, message);
    size_t message_len = strlen(message);
    ssize_t bytes_written = write(fd_trader, message, message_len);
    if (bytes_written == message_len)
    {
        printf("[T%d]\tMessage sent successfully\n", trader_id);
        printf("[T%d]\tTrying to signal exchange process: %d\n", trader_id, exchange_pid);
        kill(exchange_pid, SIGUSR1);
    }
    else
    {
        printf("[T%d]\tFailed to send message, written %ld bytes out of %ld\n", trader_id, bytes_written, message_len);
    }
}

void process_message(pid_t exchange_pid, const char *message)
{
    printf("[T%d]\tReceived message: %s\n", trader_id, message);

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
            printf("[T%d]\tExiting\n", trader_id);
            exit(0);
        }
    }
}

void handle_sigusr1(int sig, siginfo_t *info, void *context)
{
    printf("[T%d]\tReceived SIGUSR1\n", trader_id);
    char buffer[MESSAGE_BUFF_SIZE];
    int num_bytes = read(fd_exchange, buffer, MESSAGE_BUFF_SIZE - 1);
    if (num_bytes > 0)
    {
        buffer[num_bytes] = '\0'; // 添加字符串终止符
        process_message(info->si_pid, buffer);
    }
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s [Trader ID]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    trader_id = atoi(argv[1]); // 获取交易者ID

    // 为此交易者创建Named Pipes
    sprintf(pipe_exchange, "/tmp/pe_exchange_%d", trader_id);
    printf("[T%d]\tCreating named pipe: %s\n", trader_id, pipe_exchange);
    // 打开FIFO进行读写
    fd_exchange = open(pipe_exchange, O_RDONLY);
    if (fd_exchange < 0)
    {
        fprintf(stderr, "[T%d]\tFailed to open pipe to exchange\n", trader_id);
        return 1;
    }
    printf("[T%d]\tOpened named pipe: %s\n", trader_id, pipe_exchange);

    sprintf(pipe_trader, "/tmp/pe_trader_%d", trader_id);
    printf("[T%d]\tCreating named pipe: %s\n", trader_id, pipe_trader);
    fd_trader = open(pipe_trader, O_WRONLY);
    if (fd_trader < 0)
    {
        fprintf(stderr, "[T%d]\tFailed to open pipe from exchange\n", trader_id);
        return 1;
    }
    printf("[T%d]\tOpened named pipe: %s\n", trader_id, pipe_trader);

    struct sigaction act;
    act.sa_sigaction = handle_sigusr1; // 设置信号处理函数
    act.sa_flags = SA_SIGINFO;         // 使用 sa_sigaction 而不是 sa_handler

    if (sigaction(SIGUSR1, &act, NULL) == -1)
    { // 将 SIGUSR1 信号与处理函数关联
        fprintf(stderr, "[T%d]\tsigaction\n", trader_id);
        return 1;
    }
    else
    {
        printf("[T%d]\tRegistered SIGUSR1 handler\n", trader_id);
    }

    while (1)
    {
        pause(); // 暂停，等待信号
    }

    // 关闭FIFO
    close(fd_exchange);
    close(fd_trader);

    return 0;
}
