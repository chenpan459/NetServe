#ifndef IPC_MSGQUEUE_H
#define IPC_MSGQUEUE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// 消息队列最大大小
#define IPC_MSGQUEUE_MAX_SIZE 8192

// Message Queue IPC通信函数声明

// 服务器端函数
int ipc_msgqueue_create_server(const char *name);
void ipc_msgqueue_cleanup_server(const char *name);

// 客户端函数
int ipc_msgqueue_connect_client(const char *name);

// 数据传输函数
int ipc_msgqueue_send_message(int msgq_id, const void *data, size_t size, long msg_type);
int ipc_msgqueue_receive_message(int msgq_id, void **data, size_t *size, long msg_type, int timeout_ms);

// 类型化消息函数
int ipc_msgqueue_send_typed_message(int msgq_id, const void *data, size_t size, long msg_type);
int ipc_msgqueue_receive_typed_message(int msgq_id, void **data, size_t *size, long msg_type, int timeout_ms);

// 管理函数
int ipc_msgqueue_get_message_count(int msgq_id);
int ipc_msgqueue_get_max_size(int msgq_id);
int ipc_msgqueue_set_max_size(int msgq_id, int max_size);

#ifdef __cplusplus
}
#endif

#endif // IPC_MSGQUEUE_H
