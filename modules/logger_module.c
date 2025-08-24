#include "logger_module.h"
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
    .enable_timestamp = 1
};

// 全局日志级别
static log_level_t global_log_level = LOG_LEVEL_INFO;

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

// 写入日志到文件
static void write_log_to_file(logger_private_data_t *data, const char *message) {
    if (!data->config.enable_file || !data->log_fp) {
        return;
    }
    
    char timestamp[64];
    if (data->config.enable_timestamp) {
        get_timestamp(timestamp, sizeof(timestamp));
        fprintf(data->log_fp, "[%s] %s\n", timestamp, message);
    } else {
        fprintf(data->log_fp, "%s\n", message);
    }
    
    fflush(data->log_fp);
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

// 内部日志记录函数
static void log_internal(log_level_t level, const char *format, va_list args) {
    if (level < global_log_level) {
        return;
    }
    
    // 格式化消息
    char message[1024];
    vsnprintf(message, sizeof(message), format, args);
    
    // 写入控制台
    write_log_to_console(level, message);
    
    // 写入文件（如果有的话）
    // 注意：这里不应该通过模块管理器获取，因为可能造成循环依赖
    // 直接使用全局配置数据
    extern module_interface_t logger_module;
    if (logger_module.private_data) {
        logger_private_data_t *data = (logger_private_data_t*) logger_module.private_data;
        if (data->config.enable_file) {
            char full_message[1024 + 64];
            if (data->config.enable_timestamp) {
                char timestamp[64];
                get_timestamp(timestamp, sizeof(timestamp));
                snprintf(full_message, sizeof(full_message), "[%s] %s %s", 
                        timestamp, level_strings[level], message);
            } else {
                snprintf(full_message, sizeof(full_message), "%s %s", 
                        level_strings[level], message);
            }
            write_log_to_file(data, full_message);
        }
    }
}

// 日志记录函数实现
void log_debug(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_internal(LOG_LEVEL_DEBUG, format, args);
    va_end(args);
}

void log_info(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_internal(LOG_LEVEL_INFO, format, args);
    va_end(args);
}

void log_warn(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_internal(LOG_LEVEL_WARN, format, args);
    va_end(args);
}

void log_error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_internal(LOG_LEVEL_ERROR, format, args);
    va_end(args);
}

void log_fatal(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_internal(LOG_LEVEL_FATAL, format, args);
    va_end(args);
}

// 日志模块初始化
int logger_module_init(module_interface_t *self, uv_loop_t *loop) {
    if (!self) {
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
    
    // 初始化互斥锁
    if (uv_mutex_init(&data->log_mutex) != 0) {
        free(data);
        return -1;
    }
    
    self->private_data = data;
    
    log_info("日志模块初始化成功");
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
            log_error("无法打开日志文件: %s", data->config.log_file);
            return -1;
        }
        log_info("日志文件已打开: %s", data->config.log_file);
    }
    
    log_info("日志模块启动成功");
    return 0;
}

// 日志模块停止
int logger_module_stop(module_interface_t *self) {
    if (!self || !self->private_data) {
        return -1;
    }
    
    log_info("日志模块已停止");
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
    
    // 销毁互斥锁
    uv_mutex_destroy(&data->log_mutex);
    
    // 释放配置
    if (data->config.log_file) {
        free(data->config.log_file);
    }
    
    // 释放私有数据
    free(data);
    self->private_data = NULL;
    
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
    
    // 更新日志文件路径
    if (data->config.log_file != default_config.log_file) {
        free(data->config.log_file);
    }
    data->config.log_file = config->log_file ? strdup(config->log_file) : NULL;
    
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
