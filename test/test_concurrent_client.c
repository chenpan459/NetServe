#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVER_HOST "127.0.0.1"
#define SERVER_PORT 8080
#define MAX_CLIENTS 10
#define MESSAGES_PER_CLIENT 5

typedef struct {
    int client_id;
    int sock_fd;
    int message_count;
} client_context_t;

// 客户端线程函数
void* client_thread(void *arg) {
    client_context_t *ctx = (client_context_t*) arg;
    char message[256];
    char response[1024];
    
    printf("客户端 %d 开始连接...\n", ctx->client_id);
    
    // 连接到服务器
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_HOST);
    
    if (connect(ctx->sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        fprintf(stderr, "客户端 %d 连接失败\n", ctx->client_id);
        return NULL;
    }
    
    printf("客户端 %d 连接成功\n", ctx->client_id);
    
    // 发送消息
    for (int i = 0; i < ctx->message_count; i++) {
        snprintf(message, sizeof(message), "客户端%d消息%d", ctx->client_id, i + 1);
        
        // 发送消息
        if (send(ctx->sock_fd, message, strlen(message), 0) < 0) {
            fprintf(stderr, "客户端 %d 发送消息失败\n", ctx->client_id);
            break;
        }
        
        printf("客户端 %d 发送消息: %s\n", ctx->client_id, message);
        
        // 接收响应
        int bytes_received = recv(ctx->sock_fd, response, sizeof(response) - 1, 0);
        if (bytes_received > 0) {
            response[bytes_received] = '\0';
            printf("客户端 %d 收到响应: %s\n", ctx->client_id, response);
        }
        
        // 随机延迟
        usleep((rand() % 100 + 50) * 1000); // 50-150ms
    }
    
    printf("客户端 %d 完成，断开连接\n", ctx->client_id);
    close(ctx->sock_fd);
    
    return NULL;
}

int main() {
    printf("=== 多线程并发客户端测试 ===\n");
    printf("服务器地址: %s:%d\n", SERVER_HOST, SERVER_PORT);
    printf("客户端数量: %d\n", MAX_CLIENTS);
    printf("每个客户端消息数: %d\n", MESSAGES_PER_CLIENT);
    printf("==========================\n\n");
    
    pthread_t threads[MAX_CLIENTS];
    client_context_t contexts[MAX_CLIENTS];
    
    // 初始化随机数种子
    srand(time(NULL));
    
    // 创建客户端线程
    for (int i = 0; i < MAX_CLIENTS; i++) {
        contexts[i].client_id = i + 1;
        contexts[i].message_count = MESSAGES_PER_CLIENT;
        
        // 创建socket
        contexts[i].sock_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (contexts[i].sock_fd < 0) {
            fprintf(stderr, "创建socket失败\n");
            return 1;
        }
        
        // 创建线程
        if (pthread_create(&threads[i], NULL, client_thread, &contexts[i]) != 0) {
            fprintf(stderr, "创建客户端线程 %d 失败\n", i + 1);
            close(contexts[i].sock_fd);
            return 1;
        }
        
        printf("客户端线程 %d 已创建\n", i + 1);
        
        // 稍微延迟，避免同时连接
        usleep(10000); // 10ms
    }
    
    // 等待所有线程完成
    for (int i = 0; i < MAX_CLIENTS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("\n所有客户端测试完成！\n");
    return 0;
}
