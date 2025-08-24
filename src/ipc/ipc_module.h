#ifndef IPC_MODULE_H
#define IPC_MODULE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

// IPC通信类型枚举
typedef enum {
    IPC_TYPE_PIPE = 0,           // 命名管道
    IPC_TYPE_SHMEM = 1,          // 共享内存
    IPC_TYPE_SOCKET = 2,         // Unix域套接字
    IPC_TYPE_MSGQUEUE = 3,       // 消息队列
    IPC_TYPE_SEMAPHORE = 4,      // 信号量
    IPC_TYPE_MUTEX = 5           // 互斥锁
} ipc_type_t;

// IPC消息类型枚举
typedef enum {
    IPC_MSG_DATA = 0,            // 数据消息
    IPC_MSG_NOTIFY = 1,          // 通知消息
    IPC_MSG_CONTROL = 2,         // 控制消息
    IPC_MSG_HEARTBEAT = 3,       // 心跳消息
    IPC_MSG_ERROR = 4            // 错误消息
} ipc_msg_type_t;

// IPC消息优先级
typedef enum {
    IPC_PRIORITY_LOW = 0,        // 低优先级
    IPC_PRIORITY_NORMAL = 1,     // 普通优先级
    IPC_PRIORITY_HIGH = 2,       // 高优先级
    IPC_PRIORITY_URGENT = 3      // 紧急优先级
} ipc_priority_t;

// IPC消息头结构
typedef struct {
    uint32_t magic;              // 魔数标识
    uint32_t version;            // 协议版本
    uint32_t msg_id;             // 消息ID
    uint32_t msg_type;           // 消息类型
    uint32_t priority;           // 优先级
    uint32_t flags;              // 标志位
    uint64_t timestamp;          // 时间戳
    uint64_t data_size;          // 数据大小
    uint32_t checksum;           // 校验和
    uint32_t reserved[4];        // 保留字段
} ipc_msg_header_t;

// IPC消息结构
typedef struct {
    ipc_msg_header_t header;     // 消息头
    void *data;                  // 数据指针
    size_t data_len;             // 数据长度
} ipc_msg_t;

// IPC连接结构
typedef struct {
    int id;                      // 连接ID
    ipc_type_t type;             // 连接类型
    pid_t remote_pid;            // 远程进程ID
    char remote_name[64];        // 远程进程名称
    bool is_connected;           // 连接状态
    void *private_data;          // 私有数据
} ipc_connection_t;

// IPC配置结构
typedef struct {
    char name[64];               // 连接名称
    ipc_type_t type;             // 连接类型
    size_t buffer_size;          // 缓冲区大小
    size_t max_msg_size;         // 最大消息大小
    int timeout_ms;              // 超时时间(毫秒)
    bool enable_encryption;      // 是否启用加密
    bool enable_compression;     // 是否启用压缩
    int max_connections;         // 最大连接数
    int heartbeat_interval;      // 心跳间隔(秒)
} ipc_config_t;

// IPC事件类型枚举
typedef enum {
    IPC_EVENT_CONNECT = 0,       // 连接建立
    IPC_EVENT_DISCONNECT = 1,    // 连接断开
    IPC_EVENT_DATA_RECEIVED = 2, // 数据接收
    IPC_EVENT_ERROR = 3,         // 错误事件
    IPC_EVENT_TIMEOUT = 4,       // 超时事件
    IPC_EVENT_HEARTBEAT = 5      // 心跳事件
} ipc_event_type_t;

// IPC事件结构
typedef struct {
    ipc_event_type_t type;       // 事件类型
    ipc_connection_t *conn;      // 相关连接
    void *data;                  // 事件数据
    size_t data_len;             // 数据长度
    uint64_t timestamp;          // 时间戳
} ipc_event_t;

// IPC事件回调函数类型
typedef void (*ipc_event_callback_t)(const ipc_event_t *event, void *user_data);

// IPC统计信息结构
typedef struct {
    uint64_t messages_sent;      // 发送消息数
    uint64_t messages_received;  // 接收消息数
    uint64_t bytes_sent;         // 发送字节数
    uint64_t bytes_received;     // 接收字节数
    uint64_t errors;             // 错误数
    uint64_t timeouts;           // 超时数
    uint64_t connections;        // 当前连接数
    uint64_t max_connections;    // 最大连接数
} ipc_stats_t;

// 默认配置
extern const ipc_config_t ipc_default_config;

// 错误代码枚举
typedef enum {
    IPC_ERROR_NONE = 0,
    IPC_ERROR_INVALID_PARAM,
    IPC_ERROR_MEMORY_ALLOCATION,
    IPC_ERROR_CONNECTION_FAILED,
    IPC_ERROR_TIMEOUT,
    IPC_ERROR_DATA_TOO_LARGE,
    IPC_ERROR_INVALID_MESSAGE,
    IPC_ERROR_CHECKSUM_MISMATCH,
    IPC_ERROR_ENCRYPTION_FAILED,
    IPC_ERROR_COMPRESSION_FAILED,
    IPC_ERROR_IO_ERROR,
    IPC_ERROR_UNKNOWN
} ipc_error_t;

// 初始化函数
int ipc_module_init(void);
int ipc_module_cleanup(void);

// 连接管理函数
int ipc_create_server(const ipc_config_t *config);
int ipc_connect_to_server(const char *server_name, const ipc_config_t *config);
int ipc_disconnect(int conn_id);
int ipc_close_server(void);

// 消息发送函数
int ipc_send_message(int conn_id, const ipc_msg_t *message);
int ipc_send_data(int conn_id, const void *data, size_t size, ipc_msg_type_t type, ipc_priority_t priority);
int ipc_send_notification(int conn_id, const char *notification, ipc_priority_t priority);
int ipc_broadcast_message(const ipc_msg_t *message);

// 消息接收函数
int ipc_receive_message(int conn_id, ipc_msg_t *message, int timeout_ms);
int ipc_receive_data(int conn_id, void **data, size_t *size, int timeout_ms);
int ipc_poll_messages(int timeout_ms);

// 事件处理函数
int ipc_set_event_callback(ipc_event_callback_t callback, void *user_data);
int ipc_process_events(int timeout_ms);
int ipc_wait_for_event(ipc_event_type_t event_type, int timeout_ms);

// 数据传输函数（大数据量）
int ipc_send_large_data(int conn_id, const void *data, size_t size, const char *filename);
int ipc_receive_large_data(int conn_id, void **data, size_t *size, const char *filename);
int ipc_stream_data(int conn_id, const char *source_file, const char *dest_file);
int ipc_create_data_stream(int conn_id, const char *stream_name);
int ipc_close_data_stream(int conn_id, const char *stream_name);

// 共享内存操作
int ipc_create_shared_memory(const char *name, size_t size);
int ipc_attach_shared_memory(const char *name, void **ptr, size_t *size);
int ipc_detach_shared_memory(void *ptr);
int ipc_destroy_shared_memory(const char *name);

// 同步原语
int ipc_create_semaphore(const char *name, int initial_value);
int ipc_wait_semaphore(const char *name, int timeout_ms);
int ipc_signal_semaphore(const char *name);
int ipc_destroy_semaphore(const char *name);

int ipc_create_mutex(const char *name);
int ipc_lock_mutex(const char *name, int timeout_ms);
int ipc_unlock_mutex(const char *name);
int ipc_destroy_mutex(const char *name);

// 进程管理函数
int ipc_get_process_list(pid_t **pids, int *count);
int ipc_kill_process(pid_t pid);
int ipc_signal_process(pid_t pid, int signal);
int ipc_get_process_info(pid_t pid, char *name, size_t name_size);

// 监控和统计函数
int ipc_get_connection_info(int conn_id, ipc_connection_t *info);
int ipc_get_statistics(ipc_stats_t *stats);
int ipc_reset_statistics(void);
int ipc_enable_monitoring(bool enable);

// 工具函数
int ipc_get_last_error(void);
const char* ipc_error_string(ipc_error_t error);
void ipc_clear_error(void);
bool ipc_is_connected(int conn_id);
int ipc_get_connection_count(void);

// 加密和压缩函数
int ipc_set_encryption_key(const char *key, size_t key_len);
int ipc_set_compression_level(int level);
int ipc_encrypt_data(const void *input, size_t input_len, void **output, size_t *output_len);
int ipc_decrypt_data(const void *input, size_t input_len, void **output, size_t *output_len);
int ipc_compress_data(const void *input, size_t input_len, void **output, size_t *output_len);
int ipc_decompress_data(const void *input, size_t input_len, void **output, size_t *output_len);

// 文件传输函数
int ipc_send_file(int conn_id, const char *filepath, const char *remote_path);
int ipc_receive_file(int conn_id, const char *remote_path, const char *filepath);
int ipc_send_directory(int conn_id, const char *dirpath, const char *remote_path);
int ipc_receive_directory(int conn_id, const char *remote_path, const char *dirpath);

// 批量操作函数
int ipc_send_batch(int conn_id, const ipc_msg_t *messages, int count);
int ipc_receive_batch(int conn_id, ipc_msg_t *messages, int max_count, int timeout_ms);
int ipc_create_batch_sender(int conn_id, const char *batch_name);
int ipc_close_batch_sender(int conn_id, const char *batch_name);

#ifdef __cplusplus
}
#endif

#endif // IPC_MODULE_H
