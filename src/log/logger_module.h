#ifndef LOGGER_MODULE_H
#define LOGGER_MODULE_H

#include "src/modules/module_manager.h"
#include <stdarg.h>
#include <uv.h>

// 日志级别枚举
typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL
} log_level_t;

// 日志消息结构
typedef struct log_message {
    log_level_t level;
    char *message;
    char *timestamp;
    struct log_message *next;
} log_message_t;

// 日志队列结构
typedef struct {
    log_message_t *head;
    log_message_t *tail;
    size_t size;
    size_t max_size;
    uv_mutex_t queue_mutex;
    uv_cond_t queue_cond;
} log_queue_t;

// 日志模块配置
typedef struct {
    log_level_t level;
    char *log_file;
    int enable_console;
    int enable_file;
    int enable_timestamp;
    int enable_async;           // 启用异步日志
    size_t max_queue_size;      // 最大队列大小
    int flush_interval_ms;      // 刷新间隔（毫秒）
} logger_config_t;

// 日志模块私有数据
typedef struct {
    FILE *log_fp;
    logger_config_t config;
    uv_mutex_t log_mutex;
    
    // 异步日志支持
    log_queue_t message_queue;
    uv_thread_t worker_thread;
    int worker_running;
    uv_timer_t flush_timer;
    uv_loop_t *loop;
} logger_private_data_t;

// 日志模块接口
extern module_interface_t logger_module;

// 日志模块函数
int logger_module_init(module_interface_t *self, uv_loop_t *loop);
int logger_module_start(module_interface_t *self);
int logger_module_stop(module_interface_t *self);
int logger_module_cleanup(module_interface_t *self);

// 日志记录函数
void log_debug(const char *format, ...);
void log_info(const char *format, ...);
void log_warn(const char *format, ...);
void log_error(const char *format, ...);
void log_fatal(const char *format, ...);

// 同步日志记录函数（直接写入，不经过队列）
void log_debug_sync(const char *format, ...);
void log_info_sync(const char *format, ...);
void log_warn_sync(const char *format, ...);
void log_error_sync(const char *format, ...);
void log_fatal_sync(const char *format, ...);

// 日志模块配置
int logger_module_set_config(module_interface_t *self, logger_config_t *config);
logger_config_t* logger_module_get_config(module_interface_t *self);

// 日志级别设置
void logger_set_level(log_level_t level);
log_level_t logger_get_level(void);

// 队列操作函数
int log_queue_push(log_message_t *message);
log_message_t* log_queue_pop(void);
void log_queue_clear(void);
size_t log_queue_size(void);

// 异步日志控制
int logger_enable_async(int enable);
int logger_flush(void);

#endif // LOGGER_MODULE_H
