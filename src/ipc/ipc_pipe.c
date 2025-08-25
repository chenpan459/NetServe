#include "ipc_pipe.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

// Pipe IPC通信实现
int ipc_pipe_create_server(const char *name) {
    if (!name) {
        return -1;
    }
    
    char pipe_name[256];
    snprintf(pipe_name, sizeof(pipe_name), "/tmp/ipc_pipe_%s", name);
    
    // 删除可能存在的旧管道
    unlink(pipe_name);
    
    // 创建命名管道
    if (mkfifo(pipe_name, 0666) == -1) {
        return -1;
    }
    
    return 0;
}

int ipc_pipe_connect_client(const char *name) {
    if (!name) {
        return -1;
    }
    
    char pipe_name[256];
    snprintf(pipe_name, sizeof(pipe_name), "/tmp/ipc_pipe_%s", name);
    
    // 打开命名管道进行读写
    int fd = open(pipe_name, O_RDWR);
    if (fd == -1) {
        return -1;
    }
    
    return fd;
}

int ipc_pipe_open_for_reading(const char *name) {
    if (!name) {
        return -1;
    }
    
    char pipe_name[256];
    snprintf(pipe_name, sizeof(pipe_name), "/tmp/ipc_pipe_%s", name);
    
    // 以只读方式打开管道
    int fd = open(pipe_name, O_RDONLY);
    if (fd == -1) {
        return -1;
    }
    
    return fd;
}

int ipc_pipe_open_for_writing(const char *name) {
    if (!name) {
        return -1;
    }
    
    char pipe_name[256];
    snprintf(pipe_name, sizeof(pipe_name), "/tmp/ipc_pipe_%s", name);
    
    // 以只写方式打开管道
    int fd = open(pipe_name, O_WRONLY);
    if (fd == -1) {
        return -1;
    }
    
    return fd;
}

int ipc_pipe_send_message(int fd, const void *data, size_t size) {
    if (fd < 0 || !data || size == 0) {
        return -1;
    }
    
    // 发送数据大小
    uint32_t data_size = (uint32_t)size;
    if (write(fd, &data_size, sizeof(data_size)) == -1) {
        return -1;
    }
    
    // 发送实际数据
    size_t total_sent = 0;
    while (total_sent < size) {
        ssize_t sent = write(fd, (char*)data + total_sent, size - total_sent);
        if (sent == -1) {
            return -1;
        }
        total_sent += sent;
    }
    
    return 0;
}

int ipc_pipe_receive_message(int fd, void **data, size_t *size, int timeout_ms) {
    if (fd < 0 || !data || !size) {
        return -1;
    }
    
    // 设置非阻塞模式用于超时
    int flags = fcntl(fd, F_GETFL, 0);
    if (timeout_ms > 0) {
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
    
    // 接收数据大小
    uint32_t data_size;
    ssize_t received = read(fd, &data_size, sizeof(data_size));
    if (received != sizeof(data_size)) {
        if (timeout_ms > 0) {
            fcntl(fd, F_SETFL, flags);
        }
        return -1;
    }
    
    // 分配接收缓冲区
    *data = malloc(data_size);
    if (!*data) {
        if (timeout_ms > 0) {
            fcntl(fd, F_SETFL, flags);
        }
        return -1;
    }
    *size = data_size;
    
    // 接收实际数据
    size_t total_received = 0;
    while (total_received < data_size) {
        received = read(fd, (char*)*data + total_received, data_size - total_received);
        if (received == -1) {
            if (timeout_ms > 0) {
                fcntl(fd, F_SETFL, flags);
            }
            free(*data);
            *data = NULL;
            return -1;
        }
        total_received += received;
    }
    
    // 恢复原始标志
    if (timeout_ms > 0) {
        fcntl(fd, F_SETFL, flags);
    }
    
    return 0;
}

void ipc_pipe_close(int fd) {
    if (fd >= 0) {
        close(fd);
    }
}

void ipc_pipe_cleanup_server(const char *name) {
    if (name) {
        char pipe_path[256];
        snprintf(pipe_path, sizeof(pipe_path), "/tmp/ipc_pipe_%s", name);
        unlink(pipe_path);
    }
}
