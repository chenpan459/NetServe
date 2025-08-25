#include "ipc_socket.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>

// Socket IPC通信实现
int ipc_socket_create_server(const char *name, int max_connections) {
    if (!name) {
        return -1;
    }
    
    // 创建Unix域套接字
    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd == -1) {
        return -1;
    }
    
    // 设置套接字选项
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        close(server_fd);
        return -1;
    }
    
    // 构造地址结构
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "/tmp/ipc_socket_%s", name);
    
    // 删除可能存在的旧socket文件
    unlink(addr.sun_path);
    
    // 绑定地址
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        close(server_fd);
        return -1;
    }
    
    // 设置文件权限
    chmod(addr.sun_path, 0666);
    
    // 开始监听
    if (listen(server_fd, max_connections) == -1) {
        close(server_fd);
        unlink(addr.sun_path);
        return -1;
    }
    
    return server_fd;
}

int ipc_socket_connect_client(const char *name) {
    if (!name) {
        return -1;
    }
    
    // 创建客户端套接字
    int client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (client_fd == -1) {
        return -1;
    }
    
    // 构造服务器地址
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "/tmp/ipc_socket_%s", name);
    
    // 连接到服务器
    if (connect(client_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        close(client_fd);
        return -1;
    }
    
    return client_fd;
}

int ipc_socket_accept_connection(int server_fd) {
    if (server_fd < 0) {
        return -1;
    }
    
    struct sockaddr_un client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
    if (client_fd == -1) {
        return -1;
    }
    
    return client_fd;
}

int ipc_socket_send_message(int fd, const void *data, size_t size) {
    if (fd < 0 || !data || size == 0) {
        return -1;
    }
    
    // 发送数据大小
    uint32_t data_size = (uint32_t)size;
    if (send(fd, &data_size, sizeof(data_size), 0) == -1) {
        return -1;
    }
    
    // 发送实际数据
    size_t total_sent = 0;
    while (total_sent < size) {
        ssize_t sent = send(fd, (char*)data + total_sent, size - total_sent, 0);
        if (sent == -1) {
            return -1;
        }
        total_sent += sent;
    }
    
    return 0;
}

int ipc_socket_receive_message(int fd, void **data, size_t *size, int timeout_ms) {
    if (fd < 0 || !data || !size) {
        return -1;
    }
    
    // 设置接收超时
    if (timeout_ms > 0) {
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == -1) {
            return -1;
        }
    }
    
    // 接收数据大小
    uint32_t data_size;
    ssize_t received = recv(fd, &data_size, sizeof(data_size), MSG_WAITALL);
    if (received != sizeof(data_size)) {
        return -1;
    }
    
    // 分配接收缓冲区
    *data = malloc(data_size);
    if (!*data) {
        return -1;
    }
    *size = data_size;
    
    // 接收实际数据
    size_t total_received = 0;
    while (total_received < data_size) {
        received = recv(fd, (char*)*data + total_received, data_size - total_received, MSG_WAITALL);
        if (received == -1) {
            free(*data);
            *data = NULL;
            return -1;
        }
        total_received += received;
    }
    
    return 0;
}

void ipc_socket_close(int fd) {
    if (fd >= 0) {
        close(fd);
    }
}

void ipc_socket_cleanup_server(const char *name) {
    if (name) {
        char socket_path[256];
        snprintf(socket_path, sizeof(socket_path), "/tmp/ipc_socket_%s", name);
        unlink(socket_path);
    }
}
