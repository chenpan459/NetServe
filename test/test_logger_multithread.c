#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "log/logger_module.h"

#define THREAD_COUNT 5
#define LOGS_PER_THREAD 100

// 线程参数结构
typedef struct {
    int thread_id;
    int log_count;
} thread_params_t;

// 线程函数
void* logger_thread(void *arg) {
    thread_params_t *params = (thread_params_t*) arg;
    int thread_id = params->thread_id;
    int log_count = params->log_count;
    
    printf("线程 %d 开始，将记录 %d 条日志\n", thread_id, log_count);
    
    for (int i = 0; i < log_count; i++) {
        // 使用异步日志（默认）
        log_info("线程 %d: 异步日志消息 %d", thread_id, i);
        
        // 偶尔使用同步日志
        if (i % 10 == 0) {
            log_info_sync("线程 %d: 同步日志消息 %d", thread_id, i);
        }
        
        // 模拟一些工作
        usleep(1000 + (rand() % 5000)); // 1-6ms
    }
    
    printf("线程 %d 完成\n", thread_id);
    return NULL;
}

// 测试队列功能
void test_queue_functions() {
    printf("\n=== 测试队列功能 ===\n");
    
    printf("当前队列大小: %zu\n", log_queue_size());
    
    // 测试队列操作
    log_message_t *msg1 = malloc(sizeof(log_message_t));
    msg1->level = LOG_LEVEL_INFO;
    msg1->message = strdup("测试消息1");
    msg1->timestamp = strdup("2024-01-01 12:00:00");
    msg1->next = NULL;
    
    log_message_t *msg2 = malloc(sizeof(log_message_t));
    msg2->level = LOG_LEVEL_WARN;
    msg2->message = strdup("测试消息2");
    msg2->timestamp = strdup("2024-01-01 12:00:01");
    msg2->next = NULL;
    
    printf("添加消息到队列...\n");
    log_queue_push(msg1);
    log_queue_push(msg2);
    
    printf("队列大小: %zu\n", log_queue_size());
    
    // 清空队列
    printf("清空队列...\n");
    log_queue_clear();
    printf("队列大小: %zu\n", log_queue_size());
}

int main() {
    printf("=== 多线程日志测试程序 ===\n\n");
    
    // 初始化日志模块
    module_interface_t logger_mod = logger_module;
    if (logger_module_init(&logger_mod, NULL) != 0) {
        printf("日志模块初始化失败\n");
        return 1;
    }
    
    // 配置日志模块
    logger_config_t config = {
        .level = LOG_LEVEL_DEBUG,
        .log_file = "test_multithread.log",
        .enable_console = 1,
        .enable_file = 1,
        .enable_timestamp = 1,
        .enable_async = 1,
        .max_queue_size = 1000,
        .flush_interval_ms = 50
    };
    
    logger_module_set_config(&logger_mod, &config);
    
    // 启动日志模块
    if (logger_module_start(&logger_mod) != 0) {
        printf("日志模块启动失败\n");
        return 1;
    }
    
    printf("日志模块启动成功\n");
    printf("异步日志: %s\n", config.enable_async ? "启用" : "禁用");
    printf("日志文件: %s\n", config.log_file);
    printf("队列大小: %zu\n", config.max_queue_size);
    printf("刷新间隔: %d ms\n", config.flush_interval_ms);
    
    // 测试队列功能
    test_queue_functions();
    
    // 创建多个线程
    pthread_t threads[THREAD_COUNT];
    thread_params_t params[THREAD_COUNT];
    
    printf("\n=== 启动多线程日志测试 ===\n");
    printf("线程数: %d\n", THREAD_COUNT);
    printf("每线程日志数: %d\n", LOGS_PER_THREAD);
    printf("总日志数: %d\n", THREAD_COUNT * LOGS_PER_THREAD);
    
    // 创建线程
    for (int i = 0; i < THREAD_COUNT; i++) {
        params[i].thread_id = i + 1;
        params[i].log_count = LOGS_PER_THREAD;
        
        if (pthread_create(&threads[i], NULL, logger_thread, &params[i]) != 0) {
            printf("创建线程 %d 失败\n", i + 1);
            return 1;
        }
    }
    
    // 等待所有线程完成
    for (int i = 0; i < THREAD_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("\n所有线程已完成\n");
    
    // 等待队列清空
    printf("等待日志队列清空...\n");
    while (log_queue_size() > 0) {
        printf("队列中还有 %zu 条日志，等待...\n", log_queue_size());
        usleep(100000); // 等待100ms
    }
    printf("日志队列已清空\n");
    
    // 强制刷新
    printf("强制刷新日志...\n");
    logger_flush();
    
    // 显示最终统计
    printf("\n=== 测试完成 ===\n");
    printf("所有日志已写入文件: %s\n", config.log_file);
    printf("队列大小: %zu\n", log_queue_size());
    
    // 停止日志模块
    logger_module_stop(&logger_mod);
    logger_module_cleanup(&logger_mod);
    
    printf("测试完成！\n");
    return 0;
}
