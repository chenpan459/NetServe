#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

#define SERVER_HOST "127.0.0.1"
#define SERVER_PORT 8080
#define MAX_CLIENTS 10
#define MESSAGES_PER_CLIENT 5

typedef struct {
    int client_id;
    SOCKET sock_fd;
    int message_count;
} client_context_t;

// 客户端线程函数
DWORD WINAPI client_thread(LPVOID arg) {
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
        return 1;
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
        Sleep(rand() % 100 + 50); // 50-150ms
    }
    
    printf("客户端 %d 完成，断开连接\n", ctx->client_id);
    closesocket(ctx->sock_fd);
    
    return 0;
}

int main() {
    printf("=== Windows多线程并发客户端测试 ===\n");
    printf("服务器地址: %s:%d\n", SERVER_HOST, SERVER_PORT);
    printf("客户端数量: %d\n", MAX_CLIENTS);
    printf("每个客户端消息数: %d\n", MESSAGES_PER_CLIENT);
    printf("==================================\n\n");
    
    // 初始化Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup失败\n");
        return 1;
    }
    
    HANDLE threads[MAX_CLIENTS];
    client_context_t contexts[MAX_CLIENTS];
    
    // 初始化随机数种子
    srand(GetTickCount());
    
    // 创建客户端线程
    for (int i = 0; i < MAX_CLIENTS; i++) {
        contexts[i].client_id = i + 1;
        contexts[i].message_count = MESSAGES_PER_CLIENT;
        
        // 创建socket
        contexts[i].sock_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (contexts[i].sock_fd == INVALID_SOCKET) {
            fprintf(stderr, "创建socket失败\n");
            WSACleanup();
            return 1;
        }
        
        // 创建线程
        threads[i] = CreateThread(NULL, 0, client_thread, &contexts[i], 0, NULL);
        if (threads[i] == NULL) {
            fprintf(stderr, "创建客户端线程 %d 失败\n", i + 1);
            closesocket(contexts[i].sock_fd);
            WSACleanup();
            return 1;
        }
        
        printf("客户端线程 %d 已创建\n", i + 1);
        
        // 稍微延迟，避免同时连接
        Sleep(10); // 10ms
    }
    
    // 等待所有线程完成
    WaitForMultipleObjects(MAX_CLIENTS, threads, TRUE, INFINITE);
    
    // 清理线程句柄
    for (int i = 0; i < MAX_CLIENTS; i++) {
        CloseHandle(threads[i]);
    }
    
    printf("\n所有客户端测试完成！\n");
    
    // 清理Winsock
    WSACleanup();
    return 0;
}
