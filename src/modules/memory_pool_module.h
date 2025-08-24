#ifndef MEMORY_POOL_MODULE_H
#define MEMORY_POOL_MODULE_H

#include "module_manager.h"
#include <uv.h>
#include <stddef.h>

// 内存块大小配置
#define MEMORY_POOL_SMALL_BLOCK_SIZE 64
#define MEMORY_POOL_MEDIUM_BLOCK_SIZE 256
#define MEMORY_POOL_LARGE_BLOCK_SIZE 1024
#define MEMORY_POOL_HUGE_BLOCK_SIZE 4096

// 内存池配置
typedef struct {
    int enable_small_pool;
    int enable_medium_pool;
    int enable_large_pool;
    int enable_huge_pool;
    int small_pool_blocks;
    int medium_pool_blocks;
    int large_pool_blocks;
    int huge_pool_blocks;
    int enable_statistics;
    int enable_auto_resize;
} memory_pool_config_t;

// 内存块结构
typedef struct memory_block {
    struct memory_block *next;
    char data[];
} memory_block_t;

// 内存池结构
typedef struct {
    size_t block_size;
    int total_blocks;
    int free_blocks;
    memory_block_t *free_list;
    uv_mutex_t pool_mutex;
} memory_pool_t;

// 内存池模块私有数据
typedef struct {
    memory_pool_t small_pool;
    memory_pool_t medium_pool;
    memory_pool_t large_pool;
    memory_pool_t huge_pool;
    memory_pool_config_t config;
    uv_timer_t stats_timer;
    size_t total_allocated;
    size_t total_freed;
    int allocation_count;
    int free_count;
} memory_pool_private_data_t;

// 内存池模块接口
extern module_interface_t memory_pool_module;

// 内存池模块函数
int memory_pool_module_init(module_interface_t *self, uv_loop_t *loop);
int memory_pool_module_start(module_interface_t *self);
int memory_pool_module_stop(module_interface_t *self);
int memory_pool_module_cleanup(module_interface_t *self);

// 内存分配和释放函数
void* memory_pool_alloc(size_t size);
void memory_pool_free(void *ptr);
void* memory_pool_realloc(void *ptr, size_t new_size);
void* memory_pool_calloc(size_t count, size_t size);

// 内存池配置函数
int memory_pool_module_set_config(module_interface_t *self, memory_pool_config_t *config);
memory_pool_config_t* memory_pool_module_get_config(module_interface_t *self);

// 内存池统计函数
void memory_pool_print_stats(void);
size_t memory_pool_get_total_allocated(void);
size_t memory_pool_get_total_freed(void);
int memory_pool_get_allocation_count(void);
int memory_pool_get_free_count(void);

// 内存池管理函数
int memory_pool_expand_pool(memory_pool_t *pool, int additional_blocks);
void memory_pool_cleanup_pool(memory_pool_t *pool);
int memory_pool_validate_ptr(void *ptr);

#endif // MEMORY_POOL_MODULE_H
