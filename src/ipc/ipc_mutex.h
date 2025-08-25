#ifndef IPC_MUTEX_H
#define IPC_MUTEX_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Mutex IPC通信函数声明

// 服务器端函数
int ipc_mutex_create_server(const char *name);
int ipc_mutex_create_named(const char *name);
int ipc_mutex_create_robust(const char *name);
void ipc_mutex_cleanup_server(const char *name);

// 客户端函数
int ipc_mutex_connect_client(const char *name);
int ipc_mutex_open_named(const char *name);

// 互斥锁操作函数
int ipc_mutex_lock(int mutex_id, int timeout_ms);
int ipc_mutex_unlock(int mutex_id);
int ipc_mutex_try_lock(int mutex_id);

// 状态查询函数
int ipc_mutex_is_locked(int mutex_id);
int ipc_mutex_get_owner(int mutex_id);
int ipc_mutex_consistency_check(int mutex_id);

#ifdef __cplusplus
}
#endif

#endif // IPC_MUTEX_H
