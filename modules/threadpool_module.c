#include "threadpool_module.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 默认配置
static threadpool_config_t default_config = {
    .thread_count = 4,
    .max_queue_size = 1000,
    .enable_work_stealing = 1,
    .enable_priority_queue = 1
};

// 线程池模块接口定义
module_interface_t threadpool_module = {
    .name = "threadpool",
    .version = "1.0.0",
    .init = threadpool_module_init,
    .start = threadpool_module_start,
    .stop = threadpool_module_stop,
    .cleanup = threadpool_module_cleanup,
    .state = MODULE_STATE_UNINITIALIZED,
    .private_data = NULL,
    .dependencies = NULL,
    .dependency_count = 0
};

// 全局线程池数据
static threadpool_private_data_t *global_threadpool_data = NULL;

// 工作线程函数
static void worker_thread(void *arg) {
    threadpool_private_data_t *pool = (threadpool_private_data_t*) arg;
    
    while (1) {
        uv_mutex_lock(&pool->queue_mutex);
        
        // 等待工作或关闭信号
        while (pool->work_queue == NULL && pool->priority_queue == NULL && !pool->shutdown) {
            uv_cond_wait(&pool->work_available, &pool->queue_mutex);
        }
        
        // 检查是否应该关闭
        if (pool->shutdown) {
            uv_mutex_unlock(&pool->queue_mutex);
            break;
        }
        
        // 获取工作项（优先处理优先级队列）
        work_item_t *work = NULL;
        if (pool->priority_queue != NULL) {
            work = pool->priority_queue;
            pool->priority_queue = work->next;
        } else if (pool->work_queue != NULL) {
            work = pool->work_queue;
            pool->work_queue = work->next;
        }
        
        if (work != NULL) {
            pool->queued_work--;
            pool->active_threads++;
            uv_mutex_unlock(&pool->queue_mutex);
            
            // 执行工作
            work->func(work->data);
            
            // 释放工作项
            free(work);
            
            uv_mutex_lock(&pool->queue_mutex);
            pool->active_threads--;
            uv_cond_signal(&pool->queue_not_full);
            uv_mutex_unlock(&pool->queue_mutex);
        } else {
            uv_mutex_unlock(&pool->queue_mutex);
        }
    }
}

// 创建新的工作项
static work_item_t* create_work_item(work_function_t func, void *data) {
    work_item_t *item = malloc(sizeof(work_item_t));
    if (!item) {
        return NULL;
    }
    
    item->func = func;
    item->data = data;
    item->next = NULL;
    
    return item;
}

// 线程池模块初始化
int threadpool_module_init(module_interface_t *self, uv_loop_t *loop) {
    (void)loop; // 避免未使用参数警告
    
    if (!self) {
        return -1;
    }
    
    // 分配私有数据
    threadpool_private_data_t *data = malloc(sizeof(threadpool_private_data_t));
    if (!data) {
        return -1;
    }
    
    // 初始化私有数据
    memset(data, 0, sizeof(threadpool_private_data_t));
    data->config = default_config;
    data->threads = malloc(sizeof(uv_thread_t) * data->config.thread_count);
    if (!data->threads) {
        free(data);
        return -1;
    }
    
    // 初始化同步原语
    if (uv_mutex_init(&data->queue_mutex) != 0 ||
        uv_cond_init(&data->work_available) != 0 ||
        uv_cond_init(&data->queue_not_full) != 0) {
        free(data->threads);
        free(data);
        return -1;
    }
    
    data->work_queue = NULL;
    data->priority_queue = NULL;
    data->shutdown = 0;
    data->active_threads = 0;
    data->queued_work = 0;
    data->max_queue_size = data->config.max_queue_size;
    
    self->private_data = data;
    global_threadpool_data = data;
    
    printf("线程池模块初始化成功\n");
    return 0;
}

// 线程池模块启动
int threadpool_module_start(module_interface_t *self) {
    if (!self || !self->private_data) {
        return -1;
    }
    
    threadpool_private_data_t *data = (threadpool_private_data_t*) self->private_data;
    
    // 创建工作线程
    for (int i = 0; i < data->config.thread_count; i++) {
        if (uv_thread_create(&data->threads[i], worker_thread, data) != 0) {
            fprintf(stderr, "创建工作线程 %d 失败\n", i);
            return -1;
        }
    }
    
    printf("线程池模块启动成功，创建了 %d 个工作线程\n", data->config.thread_count);
    return 0;
}

// 线程池模块停止
int threadpool_module_stop(module_interface_t *self) {
    if (!self || !self->private_data) {
        return -1;
    }
    
    threadpool_private_data_t *data = (threadpool_private_data_t*) self->private_data;
    
    // 设置关闭标志
    uv_mutex_lock(&data->queue_mutex);
    data->shutdown = 1;
    uv_cond_broadcast(&data->work_available);
    uv_mutex_unlock(&data->queue_mutex);
    
    // 等待所有线程结束
    for (int i = 0; i < data->config.thread_count; i++) {
        uv_thread_join(&data->threads[i]);
    }
    
    printf("线程池模块已停止\n");
    return 0;
}

// 线程池模块清理
int threadpool_module_cleanup(module_interface_t *self) {
    if (!self || !self->private_data) {
        return -1;
    }
    
    threadpool_private_data_t *data = (threadpool_private_data_t*) self->private_data;
    
    // 清理剩余的工作项
    work_item_t *item = data->work_queue;
    while (item) {
        work_item_t *next = item->next;
        free(item);
        item = next;
    }
    
    item = data->priority_queue;
    while (item) {
        work_item_t *next = item->next;
        free(item);
        item = next;
    }
    
    // 销毁同步原语
    uv_mutex_destroy(&data->queue_mutex);
    uv_cond_destroy(&data->work_available);
    uv_cond_destroy(&data->queue_not_full);
    
    // 释放线程数组
    if (data->threads) {
        free(data->threads);
    }
    
    // 释放私有数据
    free(data);
    self->private_data = NULL;
    global_threadpool_data = NULL;
    
    printf("线程池模块清理完成\n");
    return 0;
}

// 提交工作到线程池
int threadpool_submit_work(work_function_t func, void *data) {
    if (!global_threadpool_data || !func) {
        return -1;
    }
    
    threadpool_private_data_t *pool = global_threadpool_data;
    
    uv_mutex_lock(&pool->queue_mutex);
    
    // 检查队列是否已满
    while (pool->queued_work >= pool->max_queue_size && !pool->shutdown) {
        uv_cond_wait(&pool->queue_not_full, &pool->queue_mutex);
    }
    
    if (pool->shutdown) {
        uv_mutex_unlock(&pool->queue_mutex);
        return -1;
    }
    
    // 创建工作项
    work_item_t *work = create_work_item(func, data);
    if (!work) {
        uv_mutex_unlock(&pool->queue_mutex);
        return -1;
    }
    
    // 添加到工作队列末尾
    if (pool->work_queue == NULL) {
        pool->work_queue = work;
    } else {
        work_item_t *current = pool->work_queue;
        while (current->next) {
            current = current->next;
        }
        current->next = work;
    }
    
    pool->queued_work++;
    
    // 通知工作线程
    uv_cond_signal(&pool->work_available);
    uv_mutex_unlock(&pool->queue_mutex);
    
    return 0;
}

// 提交优先级工作
int threadpool_submit_priority_work(work_function_t func, void *data) {
    if (!global_threadpool_data || !func) {
        return -1;
    }
    
    threadpool_private_data_t *pool = global_threadpool_data;
    
    uv_mutex_lock(&pool->queue_mutex);
    
    // 检查队列是否已满
    while (pool->queued_work >= pool->max_queue_size && !pool->shutdown) {
        uv_cond_wait(&pool->queue_not_full, &pool->queue_mutex);
    }
    
    if (pool->shutdown) {
        uv_mutex_unlock(&pool->queue_mutex);
        return -1;
    }
    
    // 创建工作项
    work_item_t *work = create_work_item(func, data);
    if (!work) {
        uv_mutex_unlock(&pool->queue_mutex);
        return -1;
    }
    
    // 添加到优先级队列头部
    work->next = pool->priority_queue;
    pool->priority_queue = work;
    
    pool->queued_work++;
    
    // 通知工作线程
    uv_cond_signal(&pool->work_available);
    uv_mutex_unlock(&pool->queue_mutex);
    
    return 0;
}

// 异步提交工作（带回调）
int threadpool_submit_work_async(work_function_t func, void *data, 
                                 void (*callback)(void *result)) {
    (void)callback; // 抑制未使用参数警告
    // 这里可以实现异步提交逻辑
    // 为了简化，暂时直接提交到普通队列
    return threadpool_submit_work(func, data);
}

// 设置线程池配置
int threadpool_module_set_config(module_interface_t *self, threadpool_config_t *config) {
    if (!self || !self->private_data || !config) {
        return -1;
    }
    
    threadpool_private_data_t *data = (threadpool_private_data_t*) self->private_data;
    
    // 更新配置
    data->config = *config;
    data->max_queue_size = config->max_queue_size;
    
    printf("线程池模块配置已更新\n");
    return 0;
}

// 获取线程池配置
threadpool_config_t* threadpool_module_get_config(module_interface_t *self) {
    if (!self || !self->private_data) {
        return NULL;
    }
    
    threadpool_private_data_t *data = (threadpool_private_data_t*) self->private_data;
    return &data->config;
}

// 获取活跃线程数
int threadpool_get_active_thread_count(void) {
    if (!global_threadpool_data) {
        return 0;
    }
    
    uv_mutex_lock(&global_threadpool_data->queue_mutex);
    int count = global_threadpool_data->active_threads;
    uv_mutex_unlock(&global_threadpool_data->queue_mutex);
    
    return count;
}

// 获取队列中的工作数
int threadpool_get_queued_work_count(void) {
    if (!global_threadpool_data) {
        return 0;
    }
    
    uv_mutex_lock(&global_threadpool_data->queue_mutex);
    int count = global_threadpool_data->queued_work;
    uv_mutex_unlock(&global_threadpool_data->queue_mutex);
    
    return count;
}

// 打印线程池统计信息
void threadpool_print_stats(void) {
    if (!global_threadpool_data) {
        printf("线程池未初始化\n");
        return;
    }
    
    threadpool_private_data_t *pool = global_threadpool_data;
    
    uv_mutex_lock(&pool->queue_mutex);
    printf("\n=== 线程池统计 ===\n");
    printf("总线程数: %d\n", pool->config.thread_count);
    printf("活跃线程数: %d\n", pool->active_threads);
    printf("队列中工作数: %d\n", pool->queued_work);
    printf("最大队列大小: %d\n", pool->max_queue_size);
    printf("工作窃取: %s\n", pool->config.enable_work_stealing ? "启用" : "禁用");
    printf("优先级队列: %s\n", pool->config.enable_priority_queue ? "启用" : "禁用");
    printf("==================\n\n");
    uv_mutex_unlock(&pool->queue_mutex);
}
