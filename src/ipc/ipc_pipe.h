#ifndef IPC_PIPE_H
#define IPC_PIPE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Pipe IPC通信函数声明

// 服务器端函数
int ipc_pipe_create_server(const char *name);
void ipc_pipe_cleanup_server(const char *name);

// 客户端函数
int ipc_pipe_connect_client(const char *name);
int ipc_pipe_open_for_reading(const char *name);
int ipc_pipe_open_for_writing(const char *name);

// 数据传输函数
int ipc_pipe_send_message(int fd, const void *data, size_t size);
int ipc_pipe_receive_message(int fd, void **data, size_t *size, int timeout_ms);

// 资源管理函数
void ipc_pipe_close(int fd);

#ifdef __cplusplus
}
#endif

#endif // IPC_PIPE_H
