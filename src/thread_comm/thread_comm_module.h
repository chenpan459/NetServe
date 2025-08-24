#ifndef THREAD_COMM_MODULE_H
#define THREAD_COMM_MODULE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

// 线程通信消息类型枚举
typedef enum {
    THREAD_MSG_DATA = 0,         // 数据消息
    THREAD_MSG_SIGNAL = 1,       // 信号消息
    THREAD_MSG_COMMAND = 2,      // 命令消息
    THREAD_MSG_EVENT = 3,        // 事件消息
    THREAD_MSG_REQUEST = 4,      // 请求消息
    THREAD_MSG_RESPONSE = 5,     // 响应消息
    THREAD_MSG_HEARTBEAT = 6,    // 心跳消息
    THREAD_MSG_ERROR = 7         // 错误消息
} thread_msg_type_t;

// 线程消息优先级
typedef enum {
    THREAD_PRIORITY_LOW = 0,     // 低优先级
    THREAD_PRIORITY_NORMAL = 1,  // 普通优先级
    THREAD_PRIORITY_HIGH = 2,    // 高优先级
    THREAD_PRIORITY_URGENT = 3   // 紧急优先级
} thread_priority_t;

// 线程消息结构
typedef struct {
    uint32_t msg_id;             // 消息ID
    thread_msg_type_t type;      // 消息类型
    thread_priority_t priority;  // 优先级
    uint64_t timestamp;          // 时间戳
    uint32_t sender_id;          // 发送者线程ID
    uint32_t receiver_id;        // 接收者线程ID
    uint32_t data_size;          // 数据大小
    void *data;                  // 数据指针
    uint32_t flags;              // 标志位
} thread_msg_t;

// 线程通信配置
typedef struct {
    char name[64];               // 通信名称
    size_t max_queue_size;       // 最大队列大小
    size_t max_msg_size;         // 最大消息大小
    int timeout_ms;              // 超时时间(毫秒)
    bool enable_priority;        // 是否启用优先级
    bool enable_broadcast;       // 是否启用广播
    int max_threads;             // 最大线程数
    bool enable_monitoring;      // 是否启用监控
} thread_comm_config_t;

// 线程通信句柄
typedef struct thread_comm_handle thread_comm_handle_t;

// 线程消息回调函数类型
typedef void (*thread_msg_callback_t)(const thread_msg_t *message, void *user_data);

// 线程状态枚举
typedef enum {
    THREAD_STATE_IDLE = 0,       // 空闲状态
    THREAD_STATE_RUNNING = 1,    // 运行状态
    THREAD_STATE_WAITING = 2,    // 等待状态
    THREAD_STATE_BLOCKED = 3,    // 阻塞状态
    THREAD_STATE_TERMINATED = 4  // 终止状态
} thread_state_t;

// 线程信息结构
typedef struct {
    uint32_t thread_id;          // 线程ID
    pthread_t pthread_id;        // pthread句柄
    char name[64];               // 线程名称
    thread_state_t state;        // 线程状态
    uint64_t create_time;        // 创建时间
    uint64_t cpu_time;           // CPU时间
    uint32_t message_count;      // 消息计数
    void *user_data;             // 用户数据
} thread_info_t;

// 线程通信统计信息
typedef struct {
    uint64_t messages_sent;      // 发送消息数
    uint64_t messages_received;  // 接收消息数
    uint64_t bytes_sent;         // 发送字节数
    uint64_t bytes_received;     // 接收字节数
    uint64_t errors;             // 错误数
    uint64_t timeouts;           // 超时数
    uint64_t active_threads;     // 活跃线程数
    uint64_t max_threads;        // 最大线程数
} thread_comm_stats_t;

// 默认配置
extern const thread_comm_config_t thread_comm_default_config;

// 错误代码枚举
typedef enum {
    THREAD_COMM_ERROR_NONE = 0,
    THREAD_COMM_ERROR_INVALID_PARAM,
    THREAD_COMM_ERROR_MEMORY_ALLOCATION,
    THREAD_COMM_ERROR_QUEUE_FULL,
    THREAD_COMM_ERROR_QUEUE_EMPTY,
    THREAD_COMM_ERROR_TIMEOUT,
    THREAD_COMM_ERROR_THREAD_NOT_FOUND,
    THREAD_COMM_ERROR_MESSAGE_TOO_LARGE,
    THREAD_COMM_ERROR_INVALID_MESSAGE,
    THREAD_COMM_ERROR_ALREADY_INITIALIZED,
    THREAD_COMM_ERROR_NOT_INITIALIZED,
    THREAD_COMM_ERROR_UNKNOWN
} thread_comm_error_t;

// 初始化函数
int thread_comm_init(const thread_comm_config_t *config);
int thread_comm_cleanup(void);

// 线程注册和管理函数
uint32_t thread_comm_register_thread(const char *thread_name, void *user_data);
int thread_comm_unregister_thread(uint32_t thread_id);
int thread_comm_get_thread_info(uint32_t thread_id, thread_info_t *info);
int thread_comm_get_all_threads(thread_info_t **threads, uint32_t *count);

// 消息发送函数
int thread_comm_send_message(uint32_t sender_id, uint32_t receiver_id, 
                           const void *data, size_t data_size, 
                           thread_msg_type_t type, thread_priority_t priority);
int thread_comm_send_message_async(uint32_t sender_id, uint32_t receiver_id,
                                 const void *data, size_t data_size,
                                 thread_msg_type_t type, thread_priority_t priority);
int thread_comm_broadcast_message(uint32_t sender_id, const void *data, 
                                size_t data_size, thread_msg_type_t type, 
                                thread_priority_t priority);

// 消息接收函数
int thread_comm_receive_message(uint32_t receiver_id, thread_msg_t *message, int timeout_ms);
int thread_comm_receive_message_async(uint32_t receiver_id, thread_msg_t *message);
int thread_comm_poll_messages(uint32_t receiver_id, int timeout_ms);

// 消息队列管理函数
int thread_comm_create_message_queue(uint32_t thread_id, size_t queue_size);
int thread_comm_destroy_message_queue(uint32_t thread_id);
int thread_comm_get_queue_size(uint32_t thread_id, size_t *size);
int thread_comm_clear_message_queue(uint32_t thread_id);

// 消息回调函数
int thread_comm_set_message_callback(uint32_t thread_id, thread_msg_callback_t callback, void *user_data);
int thread_comm_remove_message_callback(uint32_t thread_id);

// 同步原语
int thread_comm_create_mutex(const char *mutex_name);
int thread_comm_lock_mutex(const char *mutex_name, int timeout_ms);
int thread_comm_unlock_mutex(const char *mutex_name);
int thread_comm_destroy_mutex(const char *mutex_name);

int thread_comm_create_semaphore(const char *sem_name, int initial_value);
int thread_comm_wait_semaphore(const char *sem_name, int timeout_ms);
int thread_comm_signal_semaphore(const char *sem_name);
int thread_comm_destroy_semaphore(const char *sem_name);

int thread_comm_create_condition(const char *cond_name);
int thread_comm_wait_condition(const char *cond_name, const char *mutex_name, int timeout_ms);
int thread_comm_signal_condition(const char *cond_name);
int thread_comm_broadcast_condition(const char *cond_name);
int thread_comm_destroy_condition(const char *cond_name);

// 事件通知系统
int thread_comm_create_event(const char *event_name);
int thread_comm_set_event(const char *event_name);
int thread_comm_reset_event(const char *event_name);
int thread_comm_wait_event(const char *event_name, int timeout_ms);
int thread_comm_destroy_event(const char *event_name);

// 管道通信
int thread_comm_create_pipe(const char *pipe_name, size_t buffer_size);
int thread_comm_write_pipe(const char *pipe_name, const void *data, size_t size, int timeout_ms);
int thread_comm_read_pipe(const char *pipe_name, void *buffer, size_t buffer_size, size_t *bytes_read, int timeout_ms);
int thread_comm_destroy_pipe(const char *pipe_name);

// 共享内存（线程间）
int thread_comm_create_shared_buffer(const char *buffer_name, size_t size);
int thread_comm_map_shared_buffer(const char *buffer_name, void **ptr, size_t *size);
int thread_comm_unmap_shared_buffer(void *ptr);
int thread_comm_destroy_shared_buffer(const char *buffer_name);

// 线程池管理
int thread_comm_create_thread_pool(const char *pool_name, uint32_t min_threads, uint32_t max_threads);
int thread_comm_submit_task(const char *pool_name, void (*task_func)(void*), void *task_data);
int thread_comm_wait_task_completion(const char *pool_name, uint32_t task_id, int timeout_ms);
int thread_comm_destroy_thread_pool(const char *pool_name);

// 任务调度
int thread_comm_schedule_task(uint32_t thread_id, void (*task_func)(void*), void *task_data, 
                            uint64_t delay_ms, bool repeat);
int thread_comm_cancel_scheduled_task(uint32_t thread_id, uint32_t task_id);

// 监控和统计函数
int thread_comm_get_statistics(thread_comm_stats_t *stats);
int thread_comm_reset_statistics(void);
int thread_comm_enable_monitoring(bool enable);
int thread_comm_get_performance_metrics(uint32_t thread_id, void *metrics);

// 错误处理函数
thread_comm_error_t thread_comm_get_last_error(void);
const char* thread_comm_error_string(thread_comm_error_t error);
void thread_comm_clear_error(void);

// 工具函数
bool thread_comm_is_thread_registered(uint32_t thread_id);
uint32_t thread_comm_get_current_thread_id(void);
int thread_comm_get_thread_count(void);
bool thread_comm_is_initialized(void);

// 调试和日志函数
int thread_comm_enable_debug_logging(bool enable);
int thread_comm_set_log_level(int level);
int thread_comm_log_message(const char *format, ...);

// 内存管理
int thread_comm_set_memory_pool_size(size_t pool_size);
int thread_comm_get_memory_usage(size_t *used, size_t *total);

// 性能优化
int thread_comm_set_thread_affinity(uint32_t thread_id, int cpu_core);
int thread_comm_set_thread_priority(uint32_t thread_id, int priority);
int thread_comm_enable_lock_free_queues(bool enable);

#ifdef __cplusplus
}
#endif

#endif // THREAD_COMM_MODULE_H
