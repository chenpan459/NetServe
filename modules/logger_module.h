#ifndef LOGGER_MODULE_H
#define LOGGER_MODULE_H

#include "module_manager.h"
#include <stdarg.h>

// 日志级别枚举
typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL
} log_level_t;

// 日志模块配置
typedef struct {
    log_level_t level;
    char *log_file;
    int enable_console;
    int enable_file;
    int enable_timestamp;
} logger_config_t;

// 日志模块私有数据
typedef struct {
    FILE *log_fp;
    logger_config_t config;
    uv_mutex_t log_mutex;
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

// 日志模块配置
int logger_module_set_config(module_interface_t *self, logger_config_t *config);
logger_config_t* logger_module_get_config(module_interface_t *self);

// 日志级别设置
void logger_set_level(log_level_t level);
log_level_t logger_get_level(void);

#endif // LOGGER_MODULE_H
