#ifndef IPC_UNIFIED_H
#define IPC_UNIFIED_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>
#include <time.h>

// 包含所有IPC模块的头文件
#include "ipc_socket.h"
#include "ipc_pipe.h"
#include "ipc_shmem.h"
#include "ipc_msgqueue.h"
#include "ipc_semaphore.h"
#include "ipc_mutex.h"

#ifdef __cplusplus
extern "C" {
#endif

// IPC通信类型枚举
typedef enum {
    IPC_TYPE_SOCKET = 0,    // Unix域套接字
    IPC_TYPE_PIPE,          // 命名管道
    IPC_TYPE_SHMEM,         // 共享内存
    IPC_TYPE_MSGQUEUE,      // 消息队列
    IPC_TYPE_SEMAPHORE,     // 信号量
    IPC_TYPE_MUTEX,         // 互斥锁
    IPC_TYPE_MAX
} ipc_type_t;

// IPC配置结构
typedef struct {
    char name[64];              // IPC名称
    ipc_type_t type;            // IPC类型
    size_t buffer_size;         // 缓冲区大小
    size_t max_msg_size;        // 最大消息大小
    int timeout_ms;             // 超时时间（毫秒）
    int max_connections;        // 最大连接数
    bool enable_encryption;     // 是否启用加密
    bool enable_compression;    // 是否启用压缩
} ipc_config_t;

// IPC连接结构
typedef struct {
    int id;                     // 连接ID
    ipc_type_t type;            // 连接类型
    bool is_connected;          // 是否已连接
    void *private_data;         // 私有数据
    pid_t remote_pid;           // 远程进程ID
    char remote_name[64];       // 远程名称
} ipc_connection_t;

// IPC统计信息结构
typedef struct {
    uint64_t connections;       // 当前连接数
    uint64_t max_connections;   // 最大连接数
    uint64_t messages_sent;     // 发送消息数
    uint64_t messages_received; // 接收消息数
    uint64_t bytes_sent;        // 发送字节数
    uint64_t bytes_received;    // 接收字节数
    uint64_t errors;            // 错误数
    time_t start_time;          // 启动时间
    uint64_t uptime;            // 运行时间
} ipc_stats_t;

// 统一IPC接口函数声明

// 初始化和清理
int ipc_unified_init(void);
void ipc_unified_cleanup(void);

// 服务器端函数
int ipc_unified_create_server(const ipc_config_t *config);
void ipc_unified_close_server(void);

// 客户端函数
int ipc_unified_connect_to_server(const char *name, const ipc_config_t *config);
int ipc_unified_disconnect(int conn_id);

// 数据传输函数
int ipc_unified_send_message(int conn_id, const void *data, size_t size);
int ipc_unified_receive_message(int conn_id, void **data, size_t *size, int timeout_ms);

// 批量操作函数
int ipc_unified_send_batch(int conn_id, const void **data_array, size_t *size_array, int count);
int ipc_unified_receive_batch(int conn_id, void ***data_array, size_t **size_array, int *count, int timeout_ms);

// 文件传输函数
int ipc_unified_send_file(int conn_id, const char *filepath, const char *remote_path);
int ipc_unified_receive_file(int conn_id, const char *remote_path, const char *filepath);

// 目录传输函数
int ipc_unified_send_directory(int conn_id, const char *dirpath, const char *remote_path);
int ipc_unified_receive_directory(int conn_id, const char *remote_path, const char *dirpath);

// 流式传输函数
int ipc_unified_create_stream(int conn_id, const char *stream_name);
int ipc_unified_stream_data(int conn_id, const char *stream_name, const void *data, size_t size);
int ipc_unified_close_stream(int conn_id, const char *stream_name);

// 同步原语函数
int ipc_unified_create_sync_object(const char *name, ipc_type_t type, int initial_value);
int ipc_unified_wait_sync_object(const char *name, ipc_type_t type, int timeout_ms);
int ipc_unified_signal_sync_object(const char *name, ipc_type_t type);
void ipc_unified_destroy_sync_object(const char *name, ipc_type_t type);

// 统计和监控函数
int ipc_unified_get_statistics(ipc_stats_t *stats);
int ipc_unified_reset_statistics(void);
int ipc_unified_get_connection_count(void);
bool ipc_unified_is_connected(int conn_id);

// 配置管理函数
int ipc_unified_set_encryption_key(const char *key, size_t key_len);
int ipc_unified_set_compression_level(int level);
int ipc_unified_enable_monitoring(bool enable);

// 错误处理函数
int ipc_unified_get_last_error(void);
const char* ipc_unified_error_string(int error);
void ipc_unified_clear_error(void);

// 工具函数
const char* ipc_unified_type_string(ipc_type_t type);
ipc_type_t ipc_unified_parse_type(const char *type_string);
bool ipc_unified_is_type_supported(ipc_type_t type);

#ifdef __cplusplus
}
#endif

#endif // IPC_UNIFIED_H
