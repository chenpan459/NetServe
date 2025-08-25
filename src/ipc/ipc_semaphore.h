#ifndef IPC_SEMAPHORE_H
#define IPC_SEMAPHORE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Semaphore IPC通信函数声明

// 服务器端函数
int ipc_semaphore_create_server(const char *name, int initial_value);
int ipc_semaphore_create_array(const char *name, int num_sems, int *initial_values);
void ipc_semaphore_cleanup_server(const char *name);

// 客户端函数
int ipc_semaphore_connect_client(const char *name);

// 信号量操作函数
int ipc_semaphore_wait(int sem_id, int timeout_ms);
int ipc_semaphore_signal(int sem_id);
int ipc_semaphore_try_wait(int sem_id);
int ipc_semaphore_wait_array(int sem_id, int num_sems, int *operations);

// 管理函数
int ipc_semaphore_get_value(int sem_id);
int ipc_semaphore_set_value(int sem_id, int value);

#ifdef __cplusplus
}
#endif

#endif // IPC_SEMAPHORE_H
