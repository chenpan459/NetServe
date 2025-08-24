#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVER_HOST "127.0.0.1"
#define SERVER_PORT 8080
#define BUFFER_SIZE 4096

// 发送HTTP请求
int send_http_request(const char *method, const char *path, const char *body, const char *headers) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("创建socket失败");
        return -1;
    }
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_HOST);
    
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("连接服务器失败");
        close(sockfd);
        return -1;
    }
    
    // 构建HTTP请求
    char request[BUFFER_SIZE];
    if (body && strlen(body) > 0) {
        snprintf(request, sizeof(request),
                "%s %s HTTP/1.1\r\n"
                "Host: %s:%d\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: %zu\r\n"
                "%s"
                "\r\n"
                "%s",
                method, path, SERVER_HOST, SERVER_PORT, strlen(body), 
                headers ? headers : "", body);
    } else {
        snprintf(request, sizeof(request),
                "%s %s HTTP/1.1\r\n"
                "Host: %s:%d\r\n"
                "%s"
                "\r\n",
                method, path, SERVER_HOST, SERVER_PORT, 
                headers ? headers : "");
    }
    
    // 发送请求
    if (send(sockfd, request, strlen(request), 0) < 0) {
        perror("发送请求失败");
        close(sockfd);
        return -1;
    }
    
    printf("发送请求:\n%s\n", request);
    
    // 接收响应
    char response[BUFFER_SIZE];
    int bytes_received = recv(sockfd, response, sizeof(response) - 1, 0);
    if (bytes_received > 0) {
        response[bytes_received] = '\0';
        printf("收到响应:\n%s\n", response);
    }
    
    close(sockfd);
    return 0;
}

// 测试GET请求
void test_get_users() {
    printf("\n=== 测试获取所有用户 ===\n");
    send_http_request("GET", "/api/users", NULL, NULL);
}

void test_get_user(int id) {
    printf("\n=== 测试获取用户 %d ===\n", id);
    char path[64];
    snprintf(path, sizeof(path), "/api/users/%d", id);
    send_http_request("GET", path, NULL, NULL);
}

// 测试POST请求
void test_create_user() {
    printf("\n=== 测试创建用户 ===\n");
    const char *user_data = "{\"name\":\"新用户\",\"email\":\"newuser@example.com\",\"age\":35}";
    send_http_request("POST", "/api/users", user_data, NULL);
}

// 测试PUT请求
void test_update_user(int id) {
    printf("\n=== 测试更新用户 %d ===\n", id);
    char path[64];
    snprintf(path, sizeof(path), "/api/users/%d", id);
    const char *user_data = "{\"name\":\"更新后的用户\",\"email\":\"updated@example.com\",\"age\":40}";
    send_http_request("PUT", path, user_data, NULL);
}

// 测试DELETE请求
void test_delete_user(int id) {
    printf("\n=== 测试删除用户 %d ===\n", id);
    char path[64];
    snprintf(path, sizeof(path), "/api/users/%d", id);
    send_http_request("DELETE", path, NULL, NULL);
}

// 测试健康检查
void test_health_check() {
    printf("\n=== 测试健康检查 ===\n");
    send_http_request("GET", "/api/health", NULL, NULL);
}

// 测试不存在的路径
void test_not_found() {
    printf("\n=== 测试404错误 ===\n");
    send_http_request("GET", "/api/nonexistent", NULL, NULL);
}

// 测试无效的JSON
void test_invalid_json() {
    printf("\n=== 测试无效JSON ===\n");
    const char *invalid_json = "{\"name\":\"测试\",\"email\":\"test@example.com\",\"age\":\"invalid\"}";
    send_http_request("POST", "/api/users", invalid_json, NULL);
}

int main() {
    printf("HTTP客户端测试程序\n");
    printf("服务器地址: %s:%d\n\n", SERVER_HOST, SERVER_PORT);
    
    // 等待服务器启动
    printf("等待服务器启动...\n");
    sleep(2);
    
    // 测试健康检查
    test_health_check();
    
    // 测试获取所有用户
    test_get_users();
    
    // 测试获取指定用户
    test_get_user(1);
    test_get_user(2);
    test_get_user(3);
    
    // 测试创建用户
    test_create_user();
    
    // 测试更新用户
    test_update_user(1);
    
    // 测试删除用户
    test_delete_user(3);
    
    // 测试错误情况
    test_not_found();
    test_invalid_json();
    
    printf("\n所有测试完成\n");
    return 0;
}
