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
#include <unistd.h>
#include <pthread.h>
#include <ctype.h>

pthread_mutex_t mutex; // 定义互斥锁

int current_order_id = 1;

// 为此交易者创建Named Pipes
char pipe_exchange[PIPE_NAME_MAX_SIZE];
char pipe_trader[PIPE_NAME_MAX_SIZE];

int fd_exchange;
int fd_trader;
int trader_id;
int exchange_pid;

int get_exchange_pid()
{
    return exchange_pid;
}

int get_trader_id()
{
    return trader_id;
}

void place_order(pid_t exchange_pid, OrderType order_type, const char *item, int quantity, int price)
{
    char message[MESSAGE_BUFF_SIZE];
    sprintf(message, "%s %d %s %d %d;", order_type == BUY ? "BUY" : "SELL", current_order_id++, item, quantity, price);
    printf("[T%d]\tSending message: %s\n", trader_id, message);
    size_t message_len = strlen(message);
    sleep(1);
    ssize_t bytes_written = write(fd_trader, message, message_len);
    if (bytes_written == message_len)
    {
        printf("[T%d]\tMessage sent successfully\n", trader_id);
        sleep(1);
        kill(exchange_pid, SIGUSR1);
        printf("[T%d]\tSignaled exchange process: %d\n", trader_id, exchange_pid);
    }
    else
    {
        printf("[T%d]\tFailed to send message, written %ld bytes out of %ld\n", trader_id, bytes_written, message_len);
    }
}

void handle_sigusr1(int sig, siginfo_t *info, void *context)
{
    pthread_mutex_lock(&mutex); // 加锁
    pid_t pid = info->si_pid;
    if (pid <= 0)
    {
        printf("[T%d]\tReceived SIGUSR1 from invalid pid: %d\n", trader_id, pid);
        pid = exchange_pid;
    }
    printf("[T%d]\tReceived SIGUSR1 from pid: %d\n", trader_id, pid);

    char buffer[MESSAGE_BUFF_SIZE];
    int num_bytes = read(fd_exchange, buffer, MESSAGE_BUFF_SIZE - 1);
    if (num_bytes > 0)
    {
        buffer[num_bytes] = '\0'; // add string terminator
        char* message = strtok(buffer, ";");
        while(message != NULL) 
        {
            // Trim leading and trailing spaces
            while(isspace((unsigned char)*message)) message++;
            char *end = message + strlen(message) - 1;
            while(end > message && isspace((unsigned char)*end)) end--;
            *(end+1) = 0;

            // Call message_handler
            message_handler(pid, message);

            // Get next token
            message = strtok(NULL, ";");
        }
    }
    pthread_mutex_unlock(&mutex); // 解锁
}

int main0(int argc, char *argv[], void (*handler)(pid_t exchange_pid, const char *message))
{
    pthread_mutex_init(&mutex, NULL);
    message_handler = handler;
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s [Trader ID] [Exchange PID]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    trader_id = atoi(argv[1]); // 获取交易者ID
    exchange_pid = atoi(argv[2]); // 获取交易所进程ID

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

    pthread_mutex_destroy(&mutex);
    // 关闭FIFO
    close(fd_exchange);
    close(fd_trader);

    return 0;
}
