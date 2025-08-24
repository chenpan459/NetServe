#ifndef THREADPOOL_MODULE_H
#define THREADPOOL_MODULE_H

#include "module_manager.h"
#include <uv.h>
#include <stddef.h>

// 工作函数类型
typedef void (*work_function_t)(void *data);

// 工作项结构
typedef struct work_item {
    work_function_t func;
    void *data;
    struct work_item *next;
} work_item_t;

// 线程池配置
typedef struct {
    int thread_count;
    int max_queue_size;
    int enable_work_stealing;
    int enable_priority_queue;
} threadpool_config_t;

// 线程池私有数据
typedef struct {
    uv_thread_t *threads;
    int thread_count;
    work_item_t *work_queue;
    work_item_t *priority_queue;
    uv_mutex_t queue_mutex;
    uv_cond_t work_available;
    uv_cond_t queue_not_full;
    int shutdown;
    int active_threads;
    int queued_work;
    int max_queue_size;
    threadpool_config_t config;
} threadpool_private_data_t;

// 线程池模块接口
extern module_interface_t threadpool_module;

// 线程池模块函数
int threadpool_module_init(module_interface_t *self, uv_loop_t *loop);
int threadpool_module_start(module_interface_t *self);
int threadpool_module_stop(module_interface_t *self);
int threadpool_module_cleanup(module_interface_t *self);

// 线程池工作提交函数
int threadpool_submit_work(work_function_t func, void *data);
int threadpool_submit_priority_work(work_function_t func, void *data);
int threadpool_submit_work_async(work_function_t func, void *data, 
                                 void (*callback)(void *result));

// 线程池配置函数
int threadpool_module_set_config(module_interface_t *self, threadpool_config_t *config);
threadpool_config_t* threadpool_module_get_config(module_interface_t *self);

// 线程池统计函数
int threadpool_get_active_thread_count(void);
int threadpool_get_queued_work_count(void);
void threadpool_print_stats(void);

#endif // THREADPOOL_MODULE_H
