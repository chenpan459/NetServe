#include "memory_pool_module.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 默认配置
static memory_pool_config_t default_config = {
    .enable_small_pool = 1,
    .enable_medium_pool = 1,
    .enable_large_pool = 1,
    .enable_huge_pool = 1,
    .small_pool_blocks = 1000,
    .medium_pool_blocks = 500,
    .large_pool_blocks = 200,
    .huge_pool_blocks = 50,
    .enable_statistics = 1,
    .enable_auto_resize = 1
};

// 内存池模块接口定义
module_interface_t memory_pool_module = {
    .name = "memory_pool",
    .version = "1.0.0",
    .init = memory_pool_module_init,
    .start = memory_pool_module_start,
    .stop = memory_pool_module_stop,
    .cleanup = memory_pool_module_cleanup,
    .state = MODULE_STATE_UNINITIALIZED,
    .private_data = NULL,
    .dependencies = NULL,
    .dependency_count = 0
};

// 全局内存池数据
static memory_pool_private_data_t *global_memory_pool_data = NULL;

// 内存块头部信息（用于验证和调试）
typedef struct {
    size_t size;
    int pool_type; // 0=small, 1=medium, 2=large, 3=huge, 4=system
    int magic;     // 魔数用于验证
} block_header_t;

#define BLOCK_MAGIC 0xDEADBEEF
#define BLOCK_HEADER_SIZE sizeof(block_header_t)

// 初始化内存池
static int init_memory_pool(memory_pool_t *pool, size_t block_size, int initial_blocks) {
    pool->block_size = block_size;
    pool->total_blocks = 0;
    pool->free_blocks = 0;
    pool->free_list = NULL;
    
    if (uv_mutex_init(&pool->pool_mutex) != 0) {
        return -1;
    }
    
    // 预分配初始块
    return memory_pool_expand_pool(pool, initial_blocks);
}

// 扩展内存池
int memory_pool_expand_pool(memory_pool_t *pool, int additional_blocks) {
    if (!pool || additional_blocks <= 0) {
        return -1;
    }
    
    uv_mutex_lock(&pool->pool_mutex);
    
    for (int i = 0; i < additional_blocks; i++) {
        // 分配新的内存块
        memory_block_t *block = malloc(sizeof(memory_block_t) + pool->block_size);
        if (!block) {
            uv_mutex_unlock(&pool->pool_mutex);
            return -1;
        }
        
        // 添加到空闲列表头部
        block->next = pool->free_list;
        pool->free_list = block;
        pool->free_blocks++;
        pool->total_blocks++;
    }
    
    uv_mutex_unlock(&pool->pool_mutex);
    return 0;
}

// 从内存池分配内存块
static void* allocate_from_pool(memory_pool_t *pool) {
    if (!pool) return NULL;
    
    uv_mutex_lock(&pool->pool_mutex);
    
    if (pool->free_blocks == 0) {
        // 池为空，尝试扩展
        if (global_memory_pool_data && global_memory_pool_data->config.enable_auto_resize) {
            uv_mutex_unlock(&pool->pool_mutex);
            if (memory_pool_expand_pool(pool, pool->total_blocks / 2 + 1) == 0) {
                uv_mutex_lock(&pool->pool_mutex);
            } else {
                uv_mutex_unlock(&pool->pool_mutex);
                return NULL;
            }
        } else {
            uv_mutex_unlock(&pool->pool_mutex);
            return NULL;
        }
    }
    
    // 从空闲列表获取一个块
    memory_block_t *block = pool->free_list;
    pool->free_list = block->next;
    pool->free_blocks--;
    
    uv_mutex_unlock(&pool->pool_mutex);
    
    return block->data;
}

// 释放内存块到内存池
static void free_to_pool(memory_pool_t *pool, void *ptr) {
    if (!pool || !ptr) return;
    
    memory_block_t *block = (memory_block_t*)((char*)ptr - offsetof(memory_block_t, data));
    
    uv_mutex_lock(&pool->pool_mutex);
    
    // 添加到空闲列表头部
    block->next = pool->free_list;
    pool->free_list = block;
    pool->free_blocks++;
    
    uv_mutex_unlock(&pool->pool_mutex);
}

// 选择合适的内存池
static memory_pool_t* select_memory_pool(size_t size) {
    if (!global_memory_pool_data) return NULL;
    
    if (size <= MEMORY_POOL_SMALL_BLOCK_SIZE && global_memory_pool_data->config.enable_small_pool) {
        return &global_memory_pool_data->small_pool;
    } else if (size <= MEMORY_POOL_MEDIUM_BLOCK_SIZE && global_memory_pool_data->config.enable_medium_pool) {
        return &global_memory_pool_data->medium_pool;
    } else if (size <= MEMORY_POOL_LARGE_BLOCK_SIZE && global_memory_pool_data->config.enable_large_pool) {
        return &global_memory_pool_data->large_pool;
    } else if (size <= MEMORY_POOL_HUGE_BLOCK_SIZE && global_memory_pool_data->config.enable_huge_pool) {
        return &global_memory_pool_data->huge_pool;
    }
    
    return NULL; // 使用系统malloc
}

// 内存池分配函数
void* memory_pool_alloc(size_t size) {
    if (size == 0) return NULL;
    
    // 选择合适的内存池
    memory_pool_t *pool = select_memory_pool(size);
    
    if (pool) {
        // 从内存池分配
        void *ptr = allocate_from_pool(pool);
        if (ptr) {
            // 更新统计信息
            if (global_memory_pool_data && global_memory_pool_data->config.enable_statistics) {
                global_memory_pool_data->total_allocated += size;
                global_memory_pool_data->allocation_count++;
            }
            return ptr;
        }
    }
    
    // 回退到系统malloc
    void *ptr = malloc(size);
    if (ptr && global_memory_pool_data && global_memory_pool_data->config.enable_statistics) {
        global_memory_pool_data->total_allocated += size;
        global_memory_pool_data->allocation_count++;
    }
    
    return ptr;
}

// 内存池释放函数
void memory_pool_free(void *ptr) {
    if (!ptr) return;
    
    // 尝试找到对应的内存池
    memory_pool_t *pools[] = {
        &global_memory_pool_data->small_pool,
        &global_memory_pool_data->medium_pool,
        &global_memory_pool_data->large_pool,
        &global_memory_pool_data->huge_pool
    };
    
    for (int i = 0; i < 4; i++) {
        if (global_memory_pool_data && pools[i]) {
            // 检查指针是否属于这个池
            // 这里简化处理，实际应该维护更精确的指针映射
            free_to_pool(pools[i], ptr);
            
            // 更新统计信息
            if (global_memory_pool_data->config.enable_statistics) {
                global_memory_pool_data->total_freed += pools[i]->block_size;
                global_memory_pool_data->free_count++;
            }
            return;
        }
    }
    
    // 使用系统free
    if (global_memory_pool_data && global_memory_pool_data->config.enable_statistics) {
        global_memory_pool_data->free_count++;
    }
    free(ptr);
}

// 内存池重新分配函数
void* memory_pool_realloc(void *ptr, size_t new_size) {
    if (!ptr) return memory_pool_alloc(new_size);
    if (new_size == 0) {
        memory_pool_free(ptr);
        return NULL;
    }
    
    // 分配新内存
    void *new_ptr = memory_pool_alloc(new_size);
    if (!new_ptr) return NULL;
    
    // 复制数据（这里简化处理，实际应该获取原始大小）
    memcpy(new_ptr, ptr, new_size);
    
    // 释放旧内存
    memory_pool_free(ptr);
    
    return new_ptr;
}

// 内存池清零分配函数
void* memory_pool_calloc(size_t count, size_t size) {
    size_t total_size = count * size;
    void *ptr = memory_pool_alloc(total_size);
    if (ptr) {
        memset(ptr, 0, total_size);
    }
    return ptr;
}

// 验证指针是否有效
int memory_pool_validate_ptr(void *ptr) {
    if (!ptr) return 0;
    
    // 这里简化验证，实际应该检查指针是否在内存池范围内
    return 1;
}

// 统计定时器回调
static void on_stats_timer(uv_timer_t *handle) {
    (void)handle; // 避免未使用参数警告
    if (global_memory_pool_data && global_memory_pool_data->config.enable_statistics) {
        memory_pool_print_stats();
    }
}

// 内存池模块初始化
int memory_pool_module_init(module_interface_t *self, uv_loop_t *loop) {
    if (!self || !loop) {
        return -1;
    }
    
    // 分配私有数据
    memory_pool_private_data_t *data = malloc(sizeof(memory_pool_private_data_t));
    if (!data) {
        return -1;
    }
    
    // 初始化私有数据
    memset(data, 0, sizeof(memory_pool_private_data_t));
    data->config = default_config;
    data->total_allocated = 0;
    data->total_freed = 0;
    data->allocation_count = 0;
    data->free_count = 0;
    
    // 初始化统计定时器
    uv_timer_init(loop, &data->stats_timer);
    data->stats_timer.data = data;
    
    self->private_data = data;
    global_memory_pool_data = data;
    
    printf("内存池模块初始化成功\n");
    return 0;
}

// 内存池模块启动
int memory_pool_module_start(module_interface_t *self) {
    if (!self || !self->private_data) {
        return -1;
    }
    
    memory_pool_private_data_t *data = (memory_pool_private_data_t*) self->private_data;
    
    // 初始化各个内存池
    if (data->config.enable_small_pool) {
        if (init_memory_pool(&data->small_pool, MEMORY_POOL_SMALL_BLOCK_SIZE, 
                            data->config.small_pool_blocks) != 0) {
            fprintf(stderr, "初始化小内存池失败\n");
            return -1;
        }
        printf("小内存池初始化成功，块大小: %d, 初始块数: %d\n", 
               MEMORY_POOL_SMALL_BLOCK_SIZE, data->config.small_pool_blocks);
    }
    
    if (data->config.enable_medium_pool) {
        if (init_memory_pool(&data->medium_pool, MEMORY_POOL_MEDIUM_BLOCK_SIZE, 
                            data->config.medium_pool_blocks) != 0) {
            fprintf(stderr, "初始化中等内存池失败\n");
            return -1;
        }
        printf("中等内存池初始化成功，块大小: %d, 初始块数: %d\n", 
               MEMORY_POOL_MEDIUM_BLOCK_SIZE, data->config.medium_pool_blocks);
    }
    
    if (data->config.enable_large_pool) {
        if (init_memory_pool(&data->large_pool, MEMORY_POOL_LARGE_BLOCK_SIZE, 
                            data->config.large_pool_blocks) != 0) {
            fprintf(stderr, "初始化大内存池失败\n");
            return -1;
        }
        printf("大内存池初始化成功，块大小: %d, 初始块数: %d\n", 
               MEMORY_POOL_LARGE_BLOCK_SIZE, data->config.large_pool_blocks);
    }
    
    if (data->config.enable_huge_pool) {
        if (init_memory_pool(&data->huge_pool, MEMORY_POOL_HUGE_BLOCK_SIZE, 
                            data->config.huge_pool_blocks) != 0) {
            fprintf(stderr, "初始化超大内存池失败\n");
            return -1;
        }
        printf("超大内存池初始化成功，块大小: %d, 初始块数: %d\n", 
               MEMORY_POOL_HUGE_BLOCK_SIZE, data->config.huge_pool_blocks);
    }
    
    // 启动统计定时器（每10秒打印一次统计信息）
    if (data->config.enable_statistics) {
        uv_timer_start(&data->stats_timer, on_stats_timer, 10000, 10000);
    }
    
    printf("内存池模块启动成功\n");
    return 0;
}

// 内存池模块停止
int memory_pool_module_stop(module_interface_t *self) {
    if (!self || !self->private_data) {
        return -1;
    }
    
    memory_pool_private_data_t *data = (memory_pool_private_data_t*) self->private_data;
    
    // 停止统计定时器
    if (data->config.enable_statistics) {
        uv_timer_stop(&data->stats_timer);
    }
    
    printf("内存池模块已停止\n");
    return 0;
}

// 清理内存池
void memory_pool_cleanup_pool(memory_pool_t *pool) {
    if (!pool) return;
    
    uv_mutex_lock(&pool->pool_mutex);
    
    // 释放所有内存块
    memory_block_t *block = pool->free_list;
    while (block) {
        memory_block_t *next = block->next;
        free(block);
        block = next;
    }
    
    pool->free_list = NULL;
    pool->total_blocks = 0;
    pool->free_blocks = 0;
    
    uv_mutex_unlock(&pool->pool_mutex);
    
    // 销毁互斥锁
    uv_mutex_destroy(&pool->pool_mutex);
}

// 内存池模块清理
int memory_pool_module_cleanup(module_interface_t *self) {
    if (!self || !self->private_data) {
        return -1;
    }
    
    memory_pool_private_data_t *data = (memory_pool_private_data_t*) self->private_data;
    
    // 清理各个内存池
    if (data->config.enable_small_pool) {
        memory_pool_cleanup_pool(&data->small_pool);
    }
    if (data->config.enable_medium_pool) {
        memory_pool_cleanup_pool(&data->medium_pool);
    }
    if (data->config.enable_large_pool) {
        memory_pool_cleanup_pool(&data->large_pool);
    }
    if (data->config.enable_huge_pool) {
        memory_pool_cleanup_pool(&data->huge_pool);
    }
    
    // 释放私有数据
    free(data);
    self->private_data = NULL;
    global_memory_pool_data = NULL;
    
    printf("内存池模块清理完成\n");
    return 0;
}

// 设置内存池配置
int memory_pool_module_set_config(module_interface_t *self, memory_pool_config_t *config) {
    if (!self || !self->private_data || !config) {
        return -1;
    }
    
    memory_pool_private_data_t *data = (memory_pool_private_data_t*) self->private_data;
    
    // 更新配置
    data->config = *config;
    
    printf("内存池模块配置已更新\n");
    return 0;
}

// 获取内存池配置
memory_pool_config_t* memory_pool_module_get_config(module_interface_t *self) {
    if (!self || !self->private_data) {
        return NULL;
    }
    
    memory_pool_private_data_t *data = (memory_pool_private_data_t*) self->private_data;
    return &data->config;
}

// 获取总分配内存
size_t memory_pool_get_total_allocated(void) {
    if (!global_memory_pool_data) return 0;
    return global_memory_pool_data->total_allocated;
}

// 获取总释放内存
size_t memory_pool_get_total_freed(void) {
    if (!global_memory_pool_data) return 0;
    return global_memory_pool_data->total_freed;
}

// 获取分配次数
int memory_pool_get_allocation_count(void) {
    if (!global_memory_pool_data) return 0;
    return global_memory_pool_data->allocation_count;
}

// 获取释放次数
int memory_pool_get_free_count(void) {
    if (!global_memory_pool_data) return 0;
    return global_memory_pool_data->free_count;
}

// 打印内存池统计信息
void memory_pool_print_stats(void) {
    if (!global_memory_pool_data) {
        printf("内存池未初始化\n");
        return;
    }
    
    memory_pool_private_data_t *data = global_memory_pool_data;
    
    printf("\n=== 内存池统计 ===\n");
    printf("总分配内存: %zu 字节\n", data->total_allocated);
    printf("总释放内存: %zu 字节\n", data->total_freed);
    printf("分配次数: %d\n", data->allocation_count);
    printf("释放次数: %d\n", data->free_count);
    
    if (data->config.enable_small_pool) {
        printf("小内存池: %d/%d 块 (块大小: %d)\n", 
               data->small_pool.free_blocks, data->small_pool.total_blocks, 
               MEMORY_POOL_SMALL_BLOCK_SIZE);
    }
    
    if (data->config.enable_medium_pool) {
        printf("中等内存池: %d/%d 块 (块大小: %d)\n", 
               data->medium_pool.free_blocks, data->medium_pool.total_blocks, 
               MEMORY_POOL_MEDIUM_BLOCK_SIZE);
    }
    
    if (data->config.enable_large_pool) {
        printf("大内存池: %d/%d 块 (块大小: %d)\n", 
               data->large_pool.free_blocks, data->large_pool.total_blocks, 
               MEMORY_POOL_LARGE_BLOCK_SIZE);
    }
    
    if (data->config.enable_huge_pool) {
        printf("超大内存池: %d/%d 块 (块大小: %d)\n", 
               data->huge_pool.free_blocks, data->huge_pool.total_blocks, 
               MEMORY_POOL_HUGE_BLOCK_SIZE);
    }
    
    printf("==================\n\n");
}
