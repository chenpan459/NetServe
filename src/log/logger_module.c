#include "src/log/logger_module.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// 日志级别字符串
static const char *level_strings[] = {
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR",
    "FATAL"
};

// 日志级别颜色（ANSI转义序列）
static const char *level_colors[] = {
    "\033[36m",  // DEBUG - 青色
    "\033[32m",  // INFO - 绿色
    "\033[33m",  // WARN - 黄色
    "\033[31m",  // ERROR - 红色
    "\033[35m"   // FATAL - 紫色
};

// 默认配置
static logger_config_t default_config = {
    .level = LOG_LEVEL_INFO,
    .log_file = NULL,
    .enable_console = 1,
    .enable_file = 0,
    .enable_timestamp = 1,
    .enable_async = 1,           // 默认启用异步日志
    .max_queue_size = 10000,     // 最大队列大小
    .flush_interval_ms = 100     // 刷新间隔100ms
};

// 全局日志级别
static log_level_t global_log_level = LOG_LEVEL_INFO;

// 全局日志模块实例（用于异步日志）
static logger_private_data_t *global_logger_data = NULL;

// 日志模块接口定义
module_interface_t logger_module = {
    .name = "logger",
    .version = "1.0.0",
    .init = logger_module_init,
    .start = logger_module_start,
    .stop = logger_module_stop,
    .cleanup = logger_module_cleanup,
    .state = MODULE_STATE_UNINITIALIZED,
    .private_data = NULL,
    .dependencies = NULL,
    .dependency_count = 0
};

// 获取当前时间字符串
static void get_timestamp(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

// 创建日志消息
static log_message_t* create_log_message(log_level_t level, const char *message) {
    log_message_t *msg = malloc(sizeof(log_message_t));
    if (!msg) return NULL;
    
    msg->level = level;
    msg->message = strdup(message);
    msg->timestamp = NULL;
    msg->next = NULL;
    
    // 添加时间戳
    if (global_logger_data && global_logger_data->config.enable_timestamp) {
        char timestamp[64];
        get_timestamp(timestamp, sizeof(timestamp));
        msg->timestamp = strdup(timestamp);
    }
    
    return msg;
}

// 释放日志消息
static void free_log_message(log_message_t *msg) {
    if (msg) {
        if (msg->message) free(msg->message);
        if (msg->timestamp) free(msg->timestamp);
        free(msg);
    }
}

// 队列操作函数实现
int log_queue_push(log_message_t *message) {
    if (!global_logger_data || !message) return -1;
    
    log_queue_t *queue = &global_logger_data->message_queue;
    
    uv_mutex_lock(&queue->queue_mutex);
    
    // 检查队列是否已满
    if (queue->size >= queue->max_size) {
        uv_mutex_unlock(&queue->queue_mutex);
        return -1; // 队列已满
    }
    
    // 添加到队列尾部
    if (queue->tail) {
        queue->tail->next = message;
        queue->tail = message;
    } else {
        queue->head = message;
        queue->tail = message;
    }
    queue->size++;
    
    // 通知工作线程
    uv_cond_signal(&queue->queue_cond);
    
    uv_mutex_unlock(&queue->queue_mutex);
    return 0;
}

log_message_t* log_queue_pop(void) {
    if (!global_logger_data) return NULL;
    
    log_queue_t *queue = &global_logger_data->message_queue;
    log_message_t *message = NULL;
    
    uv_mutex_lock(&queue->queue_mutex);
    
    // 等待消息到达
    while (queue->size == 0 && global_logger_data->worker_running) {
        uv_cond_wait(&queue->queue_cond, &queue->queue_mutex);
    }
    
    // 获取消息
    if (queue->size > 0) {
        message = queue->head;
        queue->head = message->next;
        queue->size--;
        
        if (queue->size == 0) {
            queue->tail = NULL;
        }
    }
    
    uv_mutex_unlock(&queue->queue_mutex);
    return message;
}

void log_queue_clear(void) {
    if (!global_logger_data) return;
    
    log_queue_t *queue = &global_logger_data->message_queue;
    
    uv_mutex_lock(&queue->queue_mutex);
    
    log_message_t *current = queue->head;
    while (current) {
        log_message_t *next = current->next;
        free_log_message(current);
        current = next;
    }
    
    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
    
    uv_mutex_unlock(&queue->queue_mutex);
}

size_t log_queue_size(void) {
    if (!global_logger_data) return 0;
    
    log_queue_t *queue = &global_logger_data->message_queue;
    size_t size;
    
    uv_mutex_lock(&queue->queue_mutex);
    size = queue->size;
    uv_mutex_unlock(&queue->queue_mutex);
    
    return size;
}

// 写入日志到文件
static void write_log_to_file(logger_private_data_t *data, const char *message) {
    if (!data->config.enable_file || !data->log_fp) {
        return;
    }
    
    uv_mutex_lock(&data->log_mutex);
    
    if (data->config.enable_timestamp) {
        char timestamp[64];
        get_timestamp(timestamp, sizeof(timestamp));
        fprintf(data->log_fp, "[%s] %s\n", timestamp, message);
    } else {
        fprintf(data->log_fp, "%s\n", message);
    }
    
    fflush(data->log_fp);
    
    uv_mutex_unlock(&data->log_mutex);
}

// 写入日志到控制台
static void write_log_to_console(log_level_t level, const char *message) {
    if (level < global_log_level) {
        return;
    }
    
    char timestamp[64];
    get_timestamp(timestamp, sizeof(timestamp));
    
    if (level < LOG_LEVEL_FATAL) {
        printf("%s[%s] %s%s %s\n", 
               level_colors[level], 
               timestamp, 
               level_strings[level], 
               "\033[0m", 
               message);
    } else {
        printf("%s[%s] %s%s %s\n", 
               level_colors[level], 
               timestamp, 
               level_strings[level], 
               "\033[0m", 
               message);
    }
    
    fflush(stdout);
}

// 工作线程函数
static void logger_worker_thread(void *arg) {
    logger_private_data_t *data = (logger_private_data_t*) arg;
    
    while (data->worker_running) {
        log_message_t *msg = log_queue_pop();
        if (!msg) continue;
        
        // 格式化完整消息
        char full_message[1024];
        if (msg->timestamp) {
            snprintf(full_message, sizeof(full_message), "[%s] %s %s", 
                    msg->timestamp, level_strings[msg->level], msg->message);
        } else {
            snprintf(full_message, sizeof(full_message), "%s %s", 
                    level_strings[msg->level], msg->message);
        }
        
        // 写入控制台
        if (data->config.enable_console) {
            write_log_to_console(msg->level, msg->message);
        }
        
        // 写入文件
        if (data->config.enable_file) {
            write_log_to_file(data, full_message);
        }
        
        // 释放消息
        free_log_message(msg);
    }
}

// 刷新定时器回调
static void on_flush_timer(uv_timer_t *handle) {
    logger_private_data_t *data = (logger_private_data_t*) handle->data;
    
    // 强制刷新文件缓冲区
    if (data->config.enable_file && data->log_fp) {
        uv_mutex_lock(&data->log_mutex);
        fflush(data->log_fp);
        uv_mutex_unlock(&data->log_mutex);
    }
}

// 内部日志记录函数（同步）
static void log_internal_sync(log_level_t level, const char *format, va_list args) {
    if (level < global_log_level) {
        return;
    }
    
    // 格式化消息
    char message[1024];
    vsnprintf(message, sizeof(message), format, args);
    
    // 直接写入控制台
    write_log_to_console(level, message);
    
    // 直接写入文件
    if (global_logger_data && global_logger_data->config.enable_file) {
        char full_message[2048]; // 增加缓冲区大小以避免截断
        if (global_logger_data->config.enable_timestamp) {
            char timestamp[64];
            get_timestamp(timestamp, sizeof(timestamp));
            snprintf(full_message, sizeof(full_message), "[%s] %s %s",
                    timestamp, level_strings[level], message);
        } else {
            snprintf(full_message, sizeof(full_message), "%s %s", 
                    level_strings[level], message);
        }
        write_log_to_file(global_logger_data, full_message);
    }
}

// 内部日志记录函数（异步）
static void log_internal_async(log_level_t level, const char *format, va_list args) {
    if (level < global_log_level || !global_logger_data) {
        return;
    }
    
    // 格式化消息
    char message[1024];
    vsnprintf(message, sizeof(message), format, args);
    
    // 创建日志消息并加入队列
    log_message_t *msg = create_log_message(level, message);
    if (msg) {
        if (log_queue_push(msg) != 0) {
            // 队列已满，直接写入（同步方式）
            free_log_message(msg);
            log_internal_sync(level, format, args);
        }
    }
}

// 异步日志记录函数实现
void log_debug(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_internal_async(LOG_LEVEL_DEBUG, format, args);
    va_end(args);
}

void log_info(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_internal_async(LOG_LEVEL_INFO, format, args);
    va_end(args);
}

void log_warn(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_internal_async(LOG_LEVEL_WARN, format, args);
    va_end(args);
}

void log_error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_internal_async(LOG_LEVEL_ERROR, format, args);
    va_end(args);
}

void log_fatal(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_internal_async(LOG_LEVEL_FATAL, format, args);
    va_end(args);
}

// 同步日志记录函数实现
void log_debug_sync(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_internal_sync(LOG_LEVEL_DEBUG, format, args);
    va_end(args);
}

void log_info_sync(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_internal_sync(LOG_LEVEL_INFO, format, args);
    va_end(args);
}

void log_warn_sync(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_internal_sync(LOG_LEVEL_WARN, format, args);
    va_end(args);
}

void log_error_sync(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_internal_sync(LOG_LEVEL_ERROR, format, args);
    va_end(args);
}

void log_fatal_sync(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_internal_sync(LOG_LEVEL_FATAL, format, args);
    va_end(args);
}

// 日志模块初始化
int logger_module_init(module_interface_t *self, uv_loop_t *loop) {
    if (!self || !loop) {
        return -1;
    }
    
    // 分配私有数据
    logger_private_data_t *data = malloc(sizeof(logger_private_data_t));
    if (!data) {
        return -1;
    }
    
    // 初始化私有数据
    memset(data, 0, sizeof(logger_private_data_t));
    data->config = default_config;
    data->log_fp = NULL;
    data->loop = loop;
    data->worker_running = 0;
    
    // 初始化日志互斥锁
    if (uv_mutex_init(&data->log_mutex) != 0) {
        free(data);
        return -1;
    }
    
    // 初始化队列
    memset(&data->message_queue, 0, sizeof(log_queue_t));
    if (uv_mutex_init(&data->message_queue.queue_mutex) != 0) {
        uv_mutex_destroy(&data->log_mutex);
        free(data);
        return -1;
    }
    
    if (uv_cond_init(&data->message_queue.queue_cond) != 0) {
        uv_mutex_destroy(&data->message_queue.queue_mutex);
        uv_mutex_destroy(&data->log_mutex);
        free(data);
        return -1;
    }
    
    data->message_queue.max_size = default_config.max_queue_size;
    
    // 初始化刷新定时器
    uv_timer_init(loop, &data->flush_timer);
    data->flush_timer.data = data;
    
    self->private_data = data;
    global_logger_data = data;
    
    log_info_sync("日志模块初始化成功");
    return 0;
}

// 日志模块启动
int logger_module_start(module_interface_t *self) {
    if (!self || !self->private_data) {
        return -1;
    }
    
    logger_private_data_t *data = (logger_private_data_t*) self->private_data;
    
    // 如果启用了文件日志，打开日志文件
    if (data->config.enable_file && data->config.log_file) {
        data->log_fp = fopen(data->config.log_file, "a");
        if (!data->log_fp) {
            log_error_sync("无法打开日志文件: %s", data->config.log_file);
            return -1;
        }
        log_info_sync("日志文件已打开: %s", data->config.log_file);
    }
    
    // 启动工作线程
    if (data->config.enable_async) {
        data->worker_running = 1;
        if (uv_thread_create(&data->worker_thread, logger_worker_thread, data) != 0) {
            log_error_sync("无法创建工作线程");
            return -1;
        }
        log_info_sync("日志工作线程已启动");
    }
    
    // 启动刷新定时器
    uv_timer_start(&data->flush_timer, on_flush_timer, 
                   data->config.flush_interval_ms, data->config.flush_interval_ms);
    
    log_info_sync("日志模块启动成功");
    return 0;
}

// 日志模块停止
int logger_module_stop(module_interface_t *self) {
    if (!self || !self->private_data) {
        return -1;
    }
    
    logger_private_data_t *data = (logger_private_data_t*) self->private_data;
    
    // 停止刷新定时器
    uv_timer_stop(&data->flush_timer);
    
    // 停止工作线程
    if (data->worker_running) {
        data->worker_running = 0;
        uv_cond_signal(&data->message_queue.queue_cond);
        uv_thread_join(&data->worker_thread);
        log_info_sync("日志工作线程已停止");
    }
    
    // 清空队列
    log_queue_clear();
    
    log_info_sync("日志模块已停止");
    return 0;
}

// 日志模块清理
int logger_module_cleanup(module_interface_t *self) {
    if (!self || !self->private_data) {
        return -1;
    }
    
    logger_private_data_t *data = (logger_private_data_t*) self->private_data;
    
    // 关闭日志文件
    if (data->log_fp) {
        fclose(data->log_fp);
        data->log_fp = NULL;
    }
    
    // 清空队列
    log_queue_clear();
    
    // 销毁条件变量
    uv_cond_destroy(&data->message_queue.queue_cond);
    
    // 销毁互斥锁
    uv_mutex_destroy(&data->message_queue.queue_mutex);
    uv_mutex_destroy(&data->log_mutex);
    
    // 释放配置
    if (data->config.log_file) {
        free(data->config.log_file);
    }
    
    // 释放私有数据
    free(data);
    self->private_data = NULL;
    global_logger_data = NULL;
    
    printf("日志模块清理完成\n");
    return 0;
}

// 设置日志模块配置
int logger_module_set_config(module_interface_t *self, logger_config_t *config) {
    if (!self || !self->private_data || !config) {
        return -1;
    }
    
    logger_private_data_t *data = (logger_private_data_t*) self->private_data;
    
    // 更新配置
    data->config.level = config->level;
    data->config.enable_console = config->enable_console;
    data->config.enable_file = config->enable_file;
    data->config.enable_timestamp = config->enable_timestamp;
    data->config.enable_async = config->enable_async;
    data->config.max_queue_size = config->max_queue_size;
    data->config.flush_interval_ms = config->flush_interval_ms;
    
    // 更新日志文件路径
    if (data->config.log_file != default_config.log_file) {
        free(data->config.log_file);
    }
    data->config.log_file = config->log_file ? strdup(config->log_file) : NULL;
    
    // 更新队列大小
    data->message_queue.max_size = config->max_queue_size;
    
    // 更新全局日志级别
    global_log_level = config->level;
    
    log_info("日志模块配置已更新");
    return 0;
}

// 获取日志模块配置
logger_config_t* logger_module_get_config(module_interface_t *self) {
    if (!self || !self->private_data) {
        return NULL;
    }
    
    logger_private_data_t *data = (logger_private_data_t*) self->private_data;
    return &data->config;
}

// 设置全局日志级别
void logger_set_level(log_level_t level) {
    global_log_level = level;
}

// 获取全局日志级别
log_level_t logger_get_level(void) {
    return global_log_level;
}

// 启用/禁用异步日志
int logger_enable_async(int enable) {
    if (!global_logger_data) return -1;
    
    global_logger_data->config.enable_async = enable;
    return 0;
}

// 强制刷新日志
int logger_flush(void) {
    if (!global_logger_data) return -1;
    
    logger_private_data_t *data = global_logger_data;
    
    // 刷新文件缓冲区
    if (data->config.enable_file && data->log_fp) {
        uv_mutex_lock(&data->log_mutex);
        fflush(data->log_fp);
        uv_mutex_unlock(&data->log_mutex);
    }
    
    // 等待队列清空
    while (log_queue_size() > 0) {
        uv_sleep(1); // 等待1ms
    }
    
    return 0;
}
