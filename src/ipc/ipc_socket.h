#ifndef IPC_SOCKET_H
#define IPC_SOCKET_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Socket IPC通信函数声明

// 服务器端函数
int ipc_socket_create_server(const char *name, int max_connections);
int ipc_socket_accept_connection(int server_fd);
void ipc_socket_cleanup_server(const char *name);

// 客户端函数
int ipc_socket_connect_client(const char *name);

// 数据传输函数
int ipc_socket_send_message(int fd, const void *data, size_t size);
int ipc_socket_receive_message(int fd, void **data, size_t *size, int timeout_ms);

// 资源管理函数
void ipc_socket_close(int fd);

#ifdef __cplusplus
}
#endif

#endif // IPC_SOCKET_H
