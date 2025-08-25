#include "thread_comm_module.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <sys/syscall.h>
#include <sys/resource.h>
#include <pthread.h>
#include <stdarg.h>

// 默认配置
const thread_comm_config_t thread_comm_default_config = {
    .name = "default",
    .max_queue_size = 1000,
    .max_msg_size = 64 * 1024,      // 64KB
    .timeout_ms = 5000,              // 5秒
    .enable_priority = true,
    .enable_broadcast = true,
    .max_threads = 100,
    .enable_monitoring = false
};

// 消息队列节点结构
typedef struct msg_queue_node {
    thread_msg_t message;
    struct msg_queue_node *next;
    struct msg_queue_node *prev;
} msg_queue_node_t;

// 优先级消息队列结构
typedef struct {
    msg_queue_node_t *head;
    msg_queue_node_t *tail;
    size_t size;
    size_t max_size;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} priority_msg_queue_t;

// 线程信息结构
typedef struct {
    uint32_t thread_id;
    pthread_t pthread_id;
    char name[64];
    thread_state_t state;
    uint64_t create_time;
    uint64_t cpu_time;
    uint32_t message_count;
    void *user_data;
    priority_msg_queue_t *msg_queue;
    thread_msg_callback_t callback;
    void *callback_user_data;
    bool is_registered;
} internal_thread_info_t;

// 同步原语结构
typedef struct {
    char name[64];
    pthread_mutex_t mutex;
    bool is_initialized;
} named_mutex_t;

typedef struct {
    char name[64];
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool is_initialized;
} named_condition_t;

typedef struct {
    char name[64];
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool is_set;
    bool is_initialized;
} named_event_t;

typedef struct {
    char name[64];
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int value;
    bool is_initialized;
} named_semaphore_t;

// 管道结构
typedef struct {
    char name[64];
    char *buffer;
    size_t buffer_size;
    size_t read_pos;
    size_t write_pos;
    size_t data_size;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    bool is_initialized;
} named_pipe_t;

// 共享缓冲区结构
typedef struct {
    char name[64];
    void *buffer;
    size_t buffer_size;
    pthread_mutex_t mutex;
    bool is_initialized;
} shared_buffer_t;

// 任务结构
typedef struct {
    uint32_t task_id;
    void (*task_func)(void*);
    void *task_data;
    uint64_t schedule_time;
    uint64_t delay_ms;
    bool repeat;
    bool is_completed;
} scheduled_task_t;

// 线程池结构
typedef struct {
    char name[64];
    pthread_t *worker_threads;
    uint32_t min_threads;
    uint32_t max_threads;
    uint32_t current_threads;
    uint32_t active_threads;
    priority_msg_queue_t *task_queue;
    pthread_mutex_t mutex;
    pthread_cond_t worker_cond;
    bool shutdown;
    bool is_initialized;
} thread_pool_t;

// 全局状态
static struct {
    bool initialized;
    thread_comm_config_t config;
    internal_thread_info_t *threads;
    uint32_t max_threads;
    uint32_t next_thread_id;
    uint32_t active_thread_count;
    
    // 同步原语
    named_mutex_t *mutexes;
    uint32_t mutex_count;
    uint32_t max_mutexes;
    
    named_condition_t *conditions;
    uint32_t condition_count;
    uint32_t max_conditions;
    
    named_event_t *events;
    uint32_t event_count;
    uint32_t max_events;
    
    named_semaphore_t *semaphores;
    uint32_t semaphore_count;
    uint32_t max_semaphores;
    
    // 管道和共享缓冲区
    named_pipe_t *pipes;
    uint32_t pipe_count;
    uint32_t max_pipes;
    
    shared_buffer_t *shared_buffers;
    uint32_t shared_buffer_count;
    uint32_t max_shared_buffers;
    
    // 线程池
    thread_pool_t *thread_pools;
    uint32_t thread_pool_count;
    uint32_t max_thread_pools;
    
    // 任务调度
    scheduled_task_t *scheduled_tasks;
    uint32_t scheduled_task_count;
    uint32_t max_scheduled_tasks;
    uint32_t next_task_id;
    
    // 统计和监控
    thread_comm_stats_t stats;
    thread_comm_error_t last_error;
    bool monitoring_enabled;
    bool debug_logging_enabled;
    int log_level;
    
    // 同步
    pthread_mutex_t global_mutex;
    pthread_rwlock_t global_rwlock;
} global_thread_comm_data = {0};

// 内部函数声明
static uint32_t generate_thread_id(void);
static uint32_t generate_task_id(void);
static int create_message_queue(internal_thread_info_t *thread_info, size_t queue_size);
static void destroy_message_queue(internal_thread_info_t *thread_info);
static int enqueue_message(priority_msg_queue_t *queue, const thread_msg_t *message);
static int dequeue_message(priority_msg_queue_t *queue, thread_msg_t *message);
static int find_thread_by_id(uint32_t thread_id);
static int find_thread_by_pthread_id(pthread_t pthread_id);
static uint64_t get_current_time_ms(void);
static uint64_t get_thread_cpu_time(pthread_t thread);
static void update_thread_state(uint32_t thread_id, thread_state_t state);
static void worker_thread_function(void *arg);

// 初始化函数
int thread_comm_init(const thread_comm_config_t *config) {
    if (global_thread_comm_data.initialized) {
        global_thread_comm_data.last_error = THREAD_COMM_ERROR_ALREADY_INITIALIZED;
        return -1;
    }
    
    // 初始化全局互斥锁和读写锁
    if (pthread_mutex_init(&global_thread_comm_data.global_mutex, NULL) != 0) {
        global_thread_comm_data.last_error = THREAD_COMM_ERROR_MEMORY_ALLOCATION;
        return -1;
    }
    
    if (pthread_rwlock_init(&global_thread_comm_data.global_rwlock, NULL) != 0) {
        pthread_mutex_destroy(&global_thread_comm_data.global_mutex);
        global_thread_comm_data.last_error = THREAD_COMM_ERROR_MEMORY_ALLOCATION;
        return -1;
    }
    
    // 设置配置
    if (config) {
        memcpy(&global_thread_comm_data.config, config, sizeof(thread_comm_config_t));
    } else {
        memcpy(&global_thread_comm_data.config, &thread_comm_default_config, sizeof(thread_comm_config_t));
    }
    
    // 初始化线程数组
    global_thread_comm_data.max_threads = global_thread_comm_data.config.max_threads;
    global_thread_comm_data.threads = calloc(global_thread_comm_data.max_threads, sizeof(internal_thread_info_t));
    if (!global_thread_comm_data.threads) {
        pthread_rwlock_destroy(&global_thread_comm_data.global_rwlock);
        pthread_mutex_destroy(&global_thread_comm_data.global_mutex);
        global_thread_comm_data.last_error = THREAD_COMM_ERROR_MEMORY_ALLOCATION;
        return -1;
    }
    
    // 初始化同步原语数组
    global_thread_comm_data.max_mutexes = 100;
    global_thread_comm_data.max_conditions = 100;
    global_thread_comm_data.max_events = 100;
    global_thread_comm_data.max_semaphores = 100;
    
    global_thread_comm_data.mutexes = calloc(global_thread_comm_data.max_mutexes, sizeof(named_mutex_t));
    global_thread_comm_data.conditions = calloc(global_thread_comm_data.max_conditions, sizeof(named_condition_t));
    global_thread_comm_data.events = calloc(global_thread_comm_data.max_events, sizeof(named_event_t));
    global_thread_comm_data.semaphores = calloc(global_thread_comm_data.max_semaphores, sizeof(named_semaphore_t));
    
    if (!global_thread_comm_data.mutexes || !global_thread_comm_data.conditions || 
        !global_thread_comm_data.events || !global_thread_comm_data.semaphores) {
        // 清理已分配的内存
        free(global_thread_comm_data.threads);
        if (global_thread_comm_data.mutexes) free(global_thread_comm_data.mutexes);
        if (global_thread_comm_data.conditions) free(global_thread_comm_data.conditions);
        if (global_thread_comm_data.events) free(global_thread_comm_data.events);
        if (global_thread_comm_data.semaphores) free(global_thread_comm_data.semaphores);
        pthread_rwlock_destroy(&global_thread_comm_data.global_rwlock);
        pthread_mutex_destroy(&global_thread_comm_data.global_mutex);
        global_thread_comm_data.last_error = THREAD_COMM_ERROR_MEMORY_ALLOCATION;
        return -1;
    }
    
    // 初始化管道和共享缓冲区数组
    global_thread_comm_data.max_pipes = 50;
    global_thread_comm_data.max_shared_buffers = 50;
    
    global_thread_comm_data.pipes = calloc(global_thread_comm_data.max_pipes, sizeof(named_pipe_t));
    global_thread_comm_data.shared_buffers = calloc(global_thread_comm_data.max_shared_buffers, sizeof(shared_buffer_t));
    
    if (!global_thread_comm_data.pipes || !global_thread_comm_data.shared_buffers) {
        // 清理已分配的内存
        free(global_thread_comm_data.threads);
        free(global_thread_comm_data.mutexes);
        free(global_thread_comm_data.conditions);
        free(global_thread_comm_data.events);
        free(global_thread_comm_data.semaphores);
        if (global_thread_comm_data.pipes) free(global_thread_comm_data.pipes);
        if (global_thread_comm_data.shared_buffers) free(global_thread_comm_data.shared_buffers);
        pthread_rwlock_destroy(&global_thread_comm_data.global_rwlock);
        pthread_mutex_destroy(&global_thread_comm_data.global_mutex);
        global_thread_comm_data.last_error = THREAD_COMM_ERROR_MEMORY_ALLOCATION;
        return -1;
    }
    
    // 初始化线程池和任务调度数组
    global_thread_comm_data.max_thread_pools = 20;
    global_thread_comm_data.max_scheduled_tasks = 1000;
    
    global_thread_comm_data.thread_pools = calloc(global_thread_comm_data.max_thread_pools, sizeof(thread_pool_t));
    global_thread_comm_data.scheduled_tasks = calloc(global_thread_comm_data.max_scheduled_tasks, sizeof(scheduled_task_t));
    
    if (!global_thread_comm_data.thread_pools || !global_thread_comm_data.scheduled_tasks) {
        // 清理已分配的内存
        free(global_thread_comm_data.threads);
        free(global_thread_comm_data.mutexes);
        free(global_thread_comm_data.conditions);
        free(global_thread_comm_data.events);
        free(global_thread_comm_data.semaphores);
        free(global_thread_comm_data.pipes);
        free(global_thread_comm_data.shared_buffers);
        if (global_thread_comm_data.thread_pools) free(global_thread_comm_data.thread_pools);
        if (global_thread_comm_data.scheduled_tasks) free(global_thread_comm_data.scheduled_tasks);
        pthread_rwlock_destroy(&global_thread_comm_data.global_rwlock);
        pthread_mutex_destroy(&global_thread_comm_data.global_mutex);
        global_thread_comm_data.last_error = THREAD_COMM_ERROR_MEMORY_ALLOCATION;
        return -1;
    }
    
    // 初始化统计信息
    memset(&global_thread_comm_data.stats, 0, sizeof(thread_comm_stats_t));
    global_thread_comm_data.stats.max_threads = global_thread_comm_data.max_threads;
    
    global_thread_comm_data.initialized = true;
    global_thread_comm_data.last_error = THREAD_COMM_ERROR_NONE;
    
    return 0;
}

int thread_comm_cleanup(void) {
    if (!global_thread_comm_data.initialized) {
        return 0;
    }
    
    pthread_mutex_lock(&global_thread_comm_data.global_mutex);
    
    // 清理所有线程
    for (uint32_t i = 0; i < global_thread_comm_data.max_threads; i++) {
        if (global_thread_comm_data.threads[i].is_registered) {
            thread_comm_unregister_thread(global_thread_comm_data.threads[i].thread_id);
        }
    }
    
    // 清理所有同步原语
    for (uint32_t i = 0; i < global_thread_comm_data.mutex_count; i++) {
        if (global_thread_comm_data.mutexes[i].is_initialized) {
            pthread_mutex_destroy(&global_thread_comm_data.mutexes[i].mutex);
        }
    }
    
    for (uint32_t i = 0; i < global_thread_comm_data.condition_count; i++) {
        if (global_thread_comm_data.conditions[i].is_initialized) {
            pthread_mutex_destroy(&global_thread_comm_data.conditions[i].mutex);
            pthread_cond_destroy(&global_thread_comm_data.conditions[i].cond);
        }
    }
    
    for (uint32_t i = 0; i < global_thread_comm_data.event_count; i++) {
        if (global_thread_comm_data.events[i].is_initialized) {
            pthread_mutex_destroy(&global_thread_comm_data.events[i].mutex);
            pthread_cond_destroy(&global_thread_comm_data.events[i].cond);
        }
    }
    
    for (uint32_t i = 0; i < global_thread_comm_data.semaphore_count; i++) {
        if (global_thread_comm_data.semaphores[i].is_initialized) {
            pthread_mutex_destroy(&global_thread_comm_data.semaphores[i].mutex);
            pthread_cond_destroy(&global_thread_comm_data.semaphores[i].cond);
        }
    }
    
    // 清理管道和共享缓冲区
    for (uint32_t i = 0; i < global_thread_comm_data.pipe_count; i++) {
        if (global_thread_comm_data.pipes[i].is_initialized) {
            pthread_mutex_destroy(&global_thread_comm_data.pipes[i].mutex);
            pthread_cond_destroy(&global_thread_comm_data.pipes[i].not_empty);
            pthread_cond_destroy(&global_thread_comm_data.pipes[i].not_full);
            free(global_thread_comm_data.pipes[i].buffer);
        }
    }
    
    for (uint32_t i = 0; i < global_thread_comm_data.shared_buffer_count; i++) {
        if (global_thread_comm_data.shared_buffers[i].is_initialized) {
            pthread_mutex_destroy(&global_thread_comm_data.shared_buffers[i].mutex);
            free(global_thread_comm_data.shared_buffers[i].buffer);
        }
    }
    
    // 清理线程池
    for (uint32_t i = 0; i < global_thread_comm_data.thread_pool_count; i++) {
        if (global_thread_comm_data.thread_pools[i].is_initialized) {
            thread_comm_destroy_thread_pool(global_thread_comm_data.thread_pools[i].name);
        }
    }
    
    pthread_mutex_unlock(&global_thread_comm_data.global_mutex);
    
    // 清理内存
    free(global_thread_comm_data.threads);
    free(global_thread_comm_data.mutexes);
    free(global_thread_comm_data.conditions);
    free(global_thread_comm_data.events);
    free(global_thread_comm_data.semaphores);
    free(global_thread_comm_data.pipes);
    free(global_thread_comm_data.shared_buffers);
    free(global_thread_comm_data.thread_pools);
    free(global_thread_comm_data.scheduled_tasks);
    
    // 销毁同步原语
    pthread_rwlock_destroy(&global_thread_comm_data.global_rwlock);
    pthread_mutex_destroy(&global_thread_comm_data.global_mutex);
    
    global_thread_comm_data.initialized = false;
    return 0;
}

// 线程注册和管理函数
uint32_t thread_comm_register_thread(const char *thread_name, void *user_data) {
    if (!global_thread_comm_data.initialized || !thread_name) {
        global_thread_comm_data.last_error = THREAD_COMM_ERROR_INVALID_PARAM;
        return 0;
    }
    
    pthread_mutex_lock(&global_thread_comm_data.global_mutex);
    
    // 查找空闲线程槽
    uint32_t thread_slot = UINT32_MAX;
    for (uint32_t i = 0; i < global_thread_comm_data.max_threads; i++) {
        if (!global_thread_comm_data.threads[i].is_registered) {
            thread_slot = i;
            break;
        }
    }
    
    if (thread_slot == UINT32_MAX) {
        pthread_mutex_unlock(&global_thread_comm_data.global_mutex);
        global_thread_comm_data.last_error = THREAD_COMM_ERROR_MEMORY_ALLOCATION;
        return 0;
    }
    
    // 生成线程ID
    uint32_t thread_id = generate_thread_id();
    
    // 初始化线程信息
    internal_thread_info_t *thread_info = &global_thread_comm_data.threads[thread_slot];
    thread_info->thread_id = thread_id;
    thread_info->pthread_id = pthread_self();
    strncpy(thread_info->name, thread_name, 63);
    thread_info->name[63] = '\0';
    thread_info->state = THREAD_STATE_RUNNING;
    thread_info->create_time = get_current_time_ms();
    thread_info->cpu_time = 0;
    thread_info->message_count = 0;
    thread_info->user_data = user_data;
    thread_info->callback = NULL;
    thread_info->callback_user_data = NULL;
    thread_info->is_registered = true;
    
    // 创建消息队列
    if (create_message_queue(thread_info, global_thread_comm_data.config.max_queue_size) != 0) {
        thread_info->is_registered = false;
        pthread_mutex_unlock(&global_thread_comm_data.global_mutex);
        global_thread_comm_data.last_error = THREAD_COMM_ERROR_MEMORY_ALLOCATION;
        return 0;
    }
    
    global_thread_comm_data.active_thread_count++;
    global_thread_comm_data.stats.active_threads++;
    
    pthread_mutex_unlock(&global_thread_comm_data.global_mutex);
    
    global_thread_comm_data.last_error = THREAD_COMM_ERROR_NONE;
    return thread_id;
}

int thread_comm_unregister_thread(uint32_t thread_id) {
    if (!global_thread_comm_data.initialized) {
        global_thread_comm_data.last_error = THREAD_COMM_ERROR_NOT_INITIALIZED;
        return -1;
    }
    
    pthread_mutex_lock(&global_thread_comm_data.global_mutex);
    
    int thread_index = find_thread_by_id(thread_id);
    if (thread_index == -1) {
        pthread_mutex_unlock(&global_thread_comm_data.global_mutex);
        global_thread_comm_data.last_error = THREAD_COMM_ERROR_THREAD_NOT_FOUND;
        return -1;
    }
    
    internal_thread_info_t *thread_info = &global_thread_comm_data.threads[thread_index];
    
    // 销毁消息队列
    destroy_message_queue(thread_info);
    
    // 清理线程信息
    memset(thread_info, 0, sizeof(internal_thread_info_t));
    
    global_thread_comm_data.active_thread_count--;
    global_thread_comm_data.stats.active_threads--;
    
    pthread_mutex_unlock(&global_thread_comm_data.global_mutex);
    
    global_thread_comm_data.last_error = THREAD_COMM_ERROR_NONE;
    return 0;
}

// 消息发送函数
int thread_comm_send_message(uint32_t sender_id, uint32_t receiver_id, 
                           const void *data, size_t data_size, 
                           thread_msg_type_t type, thread_priority_t priority) {
    if (!global_thread_comm_data.initialized || !data || data_size == 0) {
        global_thread_comm_data.last_error = THREAD_COMM_ERROR_INVALID_PARAM;
        return -1;
    }
    
    if (data_size > global_thread_comm_data.config.max_msg_size) {
        global_thread_comm_data.last_error = THREAD_COMM_ERROR_MESSAGE_TOO_LARGE;
        return -1;
    }
    
    pthread_mutex_lock(&global_thread_comm_data.global_mutex);
    
    // 查找接收者线程
    int receiver_index = find_thread_by_id(receiver_id);
    if (receiver_index == -1) {
        pthread_mutex_unlock(&global_thread_comm_data.global_mutex);
        global_thread_comm_data.last_error = THREAD_COMM_ERROR_THREAD_NOT_FOUND;
        return -1;
    }
    
    internal_thread_info_t *receiver_info = &global_thread_comm_data.threads[receiver_index];
    
    // 创建消息
    thread_msg_t message = {0};
    message.msg_id = (uint32_t)get_current_time_ms();
    message.type = type;
    message.priority = priority;
    message.timestamp = get_current_time_ms();
    message.sender_id = sender_id;
    message.receiver_id = receiver_id;
    message.data_size = data_size;
    message.data = malloc(data_size);
    message.flags = 0;
    
    if (!message.data) {
        pthread_mutex_unlock(&global_thread_comm_data.global_mutex);
        global_thread_comm_data.last_error = THREAD_COMM_ERROR_MEMORY_ALLOCATION;
        return -1;
    }
    
    memcpy(message.data, data, data_size);
    
    // 发送消息
    int result = enqueue_message(receiver_info->msg_queue, &message);
    if (result == 0) {
        receiver_info->message_count++;
        global_thread_comm_data.stats.messages_sent++;
        global_thread_comm_data.stats.bytes_sent += data_size;
    } else {
        free(message.data);
        global_thread_comm_data.last_error = THREAD_COMM_ERROR_QUEUE_FULL;
    }
    
    pthread_mutex_unlock(&global_thread_comm_data.global_mutex);
    
    if (result == 0) {
        global_thread_comm_data.last_error = THREAD_COMM_ERROR_NONE;
    }
    
    return result;
}

// 消息接收函数
int thread_comm_receive_message(uint32_t receiver_id, thread_msg_t *message, int timeout_ms) {
    if (!global_thread_comm_data.initialized || !message) {
        global_thread_comm_data.last_error = THREAD_COMM_ERROR_INVALID_PARAM;
        return -1;
    }
    
    pthread_mutex_lock(&global_thread_comm_data.global_mutex);
    
    // 查找接收者线程
    int receiver_index = find_thread_by_id(receiver_id);
    if (receiver_index == -1) {
        pthread_mutex_unlock(&global_thread_comm_data.global_mutex);
        global_thread_comm_data.last_error = THREAD_COMM_ERROR_THREAD_NOT_FOUND;
        return -1;
    }
    
    internal_thread_info_t *receiver_info = &global_thread_comm_data.threads[receiver_index];
    
    // 等待消息
    int result = -1;
    uint64_t start_time = get_current_time_ms();
    
    while (result != 0) {
        result = dequeue_message(receiver_info->msg_queue, message);
        if (result == 0) {
            break;
        }
        
        // 检查超时
        if (timeout_ms > 0) {
            uint64_t current_time = get_current_time_ms();
            if (current_time - start_time >= (uint64_t)timeout_ms) {
                global_thread_comm_data.last_error = THREAD_COMM_ERROR_TIMEOUT;
                pthread_mutex_unlock(&global_thread_comm_data.global_mutex);
                return -1;
            }
        }
        
        // 等待消息
        pthread_cond_wait(&receiver_info->msg_queue->not_empty, &global_thread_comm_data.global_mutex);
    }
    
    if (result == 0) {
        global_thread_comm_data.stats.messages_received++;
        global_thread_comm_data.stats.bytes_received += message->data_size;
        global_thread_comm_data.last_error = THREAD_COMM_ERROR_NONE;
    }
    
    pthread_mutex_unlock(&global_thread_comm_data.global_mutex);
    
    return result;
}

// 同步原语函数
int thread_comm_create_mutex(const char *mutex_name) {
    if (!global_thread_comm_data.initialized || !mutex_name) {
        global_thread_comm_data.last_error = THREAD_COMM_ERROR_INVALID_PARAM;
        return -1;
    }
    
    pthread_mutex_lock(&global_thread_comm_data.global_mutex);
    
    // 查找空闲互斥锁槽
    uint32_t mutex_slot = UINT32_MAX;
    for (uint32_t i = 0; i < global_thread_comm_data.max_mutexes; i++) {
        if (!global_thread_comm_data.mutexes[i].is_initialized) {
            mutex_slot = i;
            break;
        }
    }
    
    if (mutex_slot == UINT32_MAX) {
        pthread_mutex_unlock(&global_thread_comm_data.global_mutex);
        global_thread_comm_data.last_error = THREAD_COMM_ERROR_MEMORY_ALLOCATION;
        return -1;
    }
    
    // 初始化互斥锁
    named_mutex_t *mutex = &global_thread_comm_data.mutexes[mutex_slot];
    strncpy(mutex->name, mutex_name, 63);
    mutex->name[63] = '\0';
    
    if (pthread_mutex_init(&mutex->mutex, NULL) != 0) {
        pthread_mutex_unlock(&global_thread_comm_data.global_mutex);
        global_thread_comm_data.last_error = THREAD_COMM_ERROR_MEMORY_ALLOCATION;
        return -1;
    }
    
    mutex->is_initialized = true;
    global_thread_comm_data.mutex_count++;
    
    pthread_mutex_unlock(&global_thread_comm_data.global_mutex);
    
    global_thread_comm_data.last_error = THREAD_COMM_ERROR_NONE;
    return 0;
}

int thread_comm_lock_mutex(const char *mutex_name, int timeout_ms) {
    if (!global_thread_comm_data.initialized || !mutex_name) {
        global_thread_comm_data.last_error = THREAD_COMM_ERROR_INVALID_PARAM;
        return -1;
    }
    
    pthread_mutex_lock(&global_thread_comm_data.global_mutex);
    
    // 查找互斥锁
    named_mutex_t *mutex = NULL;
    for (uint32_t i = 0; i < global_thread_comm_data.mutex_count; i++) {
        if (global_thread_comm_data.mutexes[i].is_initialized && 
            strcmp(global_thread_comm_data.mutexes[i].name, mutex_name) == 0) {
            mutex = &global_thread_comm_data.mutexes[i];
            break;
        }
    }
    
    if (!mutex) {
        pthread_mutex_unlock(&global_thread_comm_data.global_mutex);
        global_thread_comm_data.last_error = THREAD_COMM_ERROR_THREAD_NOT_FOUND;
        return -1;
    }
    
    pthread_mutex_unlock(&global_thread_comm_data.global_mutex);
    
    // 尝试锁定互斥锁
    if (timeout_ms > 0) {
        // 带超时的锁定
        uint64_t start_time = get_current_time_ms();
        while (pthread_mutex_trylock(&mutex->mutex) != 0) {
            uint64_t current_time = get_current_time_ms();
            if (current_time - start_time >= (uint64_t)timeout_ms) {
                global_thread_comm_data.last_error = THREAD_COMM_ERROR_TIMEOUT;
                return -1;
            }
            usleep(1000); // 1ms
        }
    } else {
        // 阻塞锁定
        if (pthread_mutex_lock(&mutex->mutex) != 0) {
            global_thread_comm_data.last_error = THREAD_COMM_ERROR_UNKNOWN;
            return -1;
        }
    }
    
    global_thread_comm_data.last_error = THREAD_COMM_ERROR_NONE;
    return 0;
}

int thread_comm_unlock_mutex(const char *mutex_name) {
    if (!global_thread_comm_data.initialized || !mutex_name) {
        global_thread_comm_data.last_error = THREAD_COMM_ERROR_INVALID_PARAM;
        return -1;
    }
    
    pthread_mutex_lock(&global_thread_comm_data.global_mutex);
    
    // 查找互斥锁
    named_mutex_t *mutex = NULL;
    for (uint32_t i = 0; i < global_thread_comm_data.mutex_count; i++) {
        if (global_thread_comm_data.mutexes[i].is_initialized && 
            strcmp(global_thread_comm_data.mutexes[i].name, mutex_name) == 0) {
            mutex = &global_thread_comm_data.mutexes[i];
            break;
        }
    }
    
    if (!mutex) {
        pthread_mutex_unlock(&global_thread_comm_data.global_mutex);
        global_thread_comm_data.last_error = THREAD_COMM_ERROR_THREAD_NOT_FOUND;
        return -1;
    }
    
    pthread_mutex_unlock(&global_thread_comm_data.global_mutex);
    
    // 解锁互斥锁
    if (pthread_mutex_unlock(&mutex->mutex) != 0) {
        global_thread_comm_data.last_error = THREAD_COMM_ERROR_UNKNOWN;
        return -1;
    }
    
    global_thread_comm_data.last_error = THREAD_COMM_ERROR_NONE;
    return 0;
}

// 统计函数
int thread_comm_get_statistics(thread_comm_stats_t *stats) {
    if (!global_thread_comm_data.initialized || !stats) {
        global_thread_comm_data.last_error = THREAD_COMM_ERROR_INVALID_PARAM;
        return -1;
    }
    
    pthread_mutex_lock(&global_thread_comm_data.global_mutex);
    memcpy(stats, &global_thread_comm_data.stats, sizeof(thread_comm_stats_t));
    pthread_mutex_unlock(&global_thread_comm_data.global_mutex);
    
    global_thread_comm_data.last_error = THREAD_COMM_ERROR_NONE;
    return 0;
}

// 工具函数
bool thread_comm_is_thread_registered(uint32_t thread_id) {
    if (!global_thread_comm_data.initialized) {
        return false;
    }
    
    pthread_mutex_lock(&global_thread_comm_data.global_mutex);
    int thread_index = find_thread_by_id(thread_id);
    bool registered = (thread_index != -1);
    pthread_mutex_unlock(&global_thread_comm_data.global_mutex);
    
    return registered;
}

uint32_t thread_comm_get_current_thread_id(void) {
    if (!global_thread_comm_data.initialized) {
        return 0;
    }
    
    pthread_mutex_lock(&global_thread_comm_data.global_mutex);
    pthread_t current_pthread = pthread_self();
    int thread_index = find_thread_by_pthread_id(current_pthread);
    uint32_t thread_id = (thread_index != -1) ? global_thread_comm_data.threads[thread_index].thread_id : 0;
    pthread_mutex_unlock(&global_thread_comm_data.global_mutex);
    
    return thread_id;
}

bool thread_comm_is_initialized(void) {
    return global_thread_comm_data.initialized;
}

// 内部函数实现
static uint32_t generate_thread_id(void) {
    static uint32_t counter = 0;
    return ++counter;
}

static uint32_t generate_task_id(void) {
    static uint32_t counter = 0;
    return ++counter;
}

static int create_message_queue(internal_thread_info_t *thread_info, size_t queue_size) {
    priority_msg_queue_t *queue = malloc(sizeof(priority_msg_queue_t));
    if (!queue) {
        return -1;
    }
    
    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
    queue->max_size = queue_size;
    
    if (pthread_mutex_init(&queue->mutex, NULL) != 0) {
        free(queue);
        return -1;
    }
    
    if (pthread_cond_init(&queue->not_empty, NULL) != 0) {
        pthread_mutex_destroy(&queue->mutex);
        free(queue);
        return -1;
    }
    
    if (pthread_cond_init(&queue->not_full, NULL) != 0) {
        pthread_cond_destroy(&queue->not_empty);
        pthread_mutex_destroy(&queue->mutex);
        free(queue);
        return -1;
    }
    
    thread_info->msg_queue = queue;
    return 0;
}

static void destroy_message_queue(internal_thread_info_t *thread_info) {
    if (!thread_info->msg_queue) {
        return;
    }
    
    priority_msg_queue_t *queue = thread_info->msg_queue;
    
    // 清理所有消息
    msg_queue_node_t *node = queue->head;
    while (node) {
        msg_queue_node_t *next = node->next;
        if (node->message.data) {
            free(node->message.data);
        }
        free(node);
        node = next;
    }
    
    pthread_cond_destroy(&queue->not_full);
    pthread_cond_destroy(&queue->not_empty);
    pthread_mutex_destroy(&queue->mutex);
    free(queue);
    
    thread_info->msg_queue = NULL;
}

static int enqueue_message(priority_msg_queue_t *queue, const thread_msg_t *message) {
    pthread_mutex_lock(&queue->mutex);
    
    // 检查队列是否已满
    if (queue->size >= queue->max_size) {
        pthread_mutex_unlock(&queue->mutex);
        return -1;
    }
    
    // 创建新节点
    msg_queue_node_t *new_node = malloc(sizeof(msg_queue_node_t));
    if (!new_node) {
        pthread_mutex_unlock(&queue->mutex);
        return -1;
    }
    
    // 复制消息
    memcpy(&new_node->message, message, sizeof(thread_msg_t));
    new_node->message.data = malloc(message->data_size);
    if (!new_node->message.data) {
        free(new_node);
        pthread_mutex_unlock(&queue->mutex);
        return -1;
    }
    memcpy(new_node->message.data, message->data, message->data_size);
    
    // 根据优先级插入到合适位置
    msg_queue_node_t *current = queue->head;
    msg_queue_node_t *prev = NULL;
    
    while (current && current->message.priority >= message->priority) {
        prev = current;
        current = current->next;
    }
    
    new_node->next = current;
    new_node->prev = prev;
    
    if (prev) {
        prev->next = new_node;
    } else {
        queue->head = new_node;
    }
    
    if (current) {
        current->prev = new_node;
    } else {
        queue->tail = new_node;
    }
    
    queue->size++;
    
    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->mutex);
    
    return 0;
}

static int dequeue_message(priority_msg_queue_t *queue, thread_msg_t *message) {
    pthread_mutex_lock(&queue->mutex);
    
    if (queue->size == 0) {
        pthread_mutex_unlock(&queue->mutex);
        return -1;
    }
    
    // 从头部取出消息
    msg_queue_node_t *node = queue->head;
    queue->head = node->next;
    
    if (queue->head) {
        queue->head->prev = NULL;
    } else {
        queue->tail = NULL;
    }
    
    queue->size--;
    
    // 复制消息
    memcpy(message, &node->message, sizeof(thread_msg_t));
    message->data = malloc(node->message.data_size);
    if (message->data) {
        memcpy(message->data, node->message.data, node->message.data_size);
    }
    
    // 清理节点
    if (node->message.data) {
        free(node->message.data);
    }
    free(node);
    
    pthread_cond_signal(&queue->not_full);
    pthread_mutex_unlock(&queue->mutex);
    
    return 0;
}

static int find_thread_by_id(uint32_t thread_id) {
    for (uint32_t i = 0; i < global_thread_comm_data.max_threads; i++) {
        if (global_thread_comm_data.threads[i].is_registered && 
            global_thread_comm_data.threads[i].thread_id == thread_id) {
            return i;
        }
    }
    return -1;
}

static int find_thread_by_pthread_id(pthread_t pthread_id) {
    for (uint32_t i = 0; i < global_thread_comm_data.max_threads; i++) {
        if (global_thread_comm_data.threads[i].is_registered && 
            pthread_equal(global_thread_comm_data.threads[i].pthread_id, pthread_id)) {
            return i;
        }
    }
    return -1;
}

static uint64_t get_current_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec / 1000;
}

static uint64_t get_thread_cpu_time(pthread_t thread) {
    // 简化实现，实际应该使用更精确的CPU时间获取方法
    return get_current_time_ms();
}

static void update_thread_state(uint32_t thread_id, thread_state_t state) {
    int thread_index = find_thread_by_id(thread_id);
    if (thread_index != -1) {
        global_thread_comm_data.threads[thread_index].state = state;
    }
}

static void worker_thread_function(void *arg) {
    // 线程池工作线程函数，简化实现
    (void)arg;
    // 实际实现应该包含任务处理逻辑
}
