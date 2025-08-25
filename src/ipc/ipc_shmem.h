#ifndef IPC_SHMEM_H
#define IPC_SHMEM_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Shared Memory IPC通信函数声明

// 服务器端函数
int ipc_shmem_create_server(const char *name, size_t size);
int ipc_shmem_create_file_mapping(const char *name, size_t size);
void ipc_shmem_cleanup_server(const char *name);

// 客户端函数
int ipc_shmem_connect_client(const char *name, size_t *size);

// 内存管理函数
void* ipc_shmem_attach(int shm_id);
int ipc_shmem_detach(void *ptr);
void* ipc_shmem_map_file(const char *name, size_t size, int *fd);
int ipc_shmem_unmap_file(void *ptr, size_t size);

// 数据传输函数
int ipc_shmem_send_message(void *ptr, const void *data, size_t size, size_t offset);
int ipc_shmem_receive_message(void *ptr, void **data, size_t size, size_t offset);

#ifdef __cplusplus
}
#endif

#endif // IPC_SHMEM_H
