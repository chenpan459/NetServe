#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "modules/memory_pool_module.h"

#define TEST_ITERATIONS 10000
#define MAX_ALLOCATIONS 1000

typedef struct {
    void *ptr;
    size_t size;
    int is_allocated;
} allocation_record_t;

// 测试内存池基本功能
void test_basic_functionality() {
    printf("=== 测试内存池基本功能 ===\n");
    
    // 测试不同大小的分配
    void *ptr1 = memory_pool_alloc(32);   // 小内存池
    void *ptr2 = memory_pool_alloc(128);  // 中等内存池
    void *ptr3 = memory_pool_alloc(512);  // 大内存池
    void *ptr4 = memory_pool_alloc(2048); // 超大内存池
    void *ptr5 = memory_pool_alloc(8192); // 系统malloc
    
    if (ptr1 && ptr2 && ptr3 && ptr4 && ptr5) {
        printf("✓ 基本分配测试通过\n");
        
        // 写入测试数据
        strcpy(ptr1, "小内存块测试");
        strcpy(ptr2, "中等内存块测试");
        strcpy(ptr3, "大内存块测试");
        strcpy(ptr4, "超大内存块测试");
        strcpy(ptr5, "系统内存块测试");
        
        printf("✓ 数据写入测试通过\n");
        
        // 验证数据
        if (strcmp(ptr1, "小内存块测试") == 0 &&
            strcmp(ptr2, "中等内存块测试") == 0 &&
            strcmp(ptr3, "大内存块测试") == 0 &&
            strcmp(ptr4, "超大内存块测试") == 0 &&
            strcmp(ptr5, "系统内存块测试") == 0) {
            printf("✓ 数据验证测试通过\n");
        }
        
        // 释放内存
        memory_pool_free(ptr1);
        memory_pool_free(ptr2);
        memory_pool_free(ptr3);
        memory_pool_free(ptr4);
        memory_pool_free(ptr5);
        
        printf("✓ 内存释放测试通过\n");
    } else {
        printf("✗ 基本分配测试失败\n");
    }
    
    printf("\n");
}

// 测试内存池性能
void test_performance() {
    printf("=== 测试内存池性能 ===\n");
    
    allocation_record_t allocations[MAX_ALLOCATIONS];
    memset(allocations, 0, sizeof(allocations));
    
    clock_t start_time = clock();
    
    // 随机分配和释放测试
    srand(time(NULL));
    for (int i = 0; i < TEST_ITERATIONS; i++) {
        int action = rand() % 2; // 0=分配, 1=释放
        
        if (action == 0) {
            // 分配内存
            for (int j = 0; j < MAX_ALLOCATIONS; j++) {
                if (!allocations[j].is_allocated) {
                    size_t size = (rand() % 4 + 1) * 256; // 256, 512, 768, 1024
                    allocations[j].ptr = memory_pool_alloc(size);
                    if (allocations[j].ptr) {
                        allocations[j].size = size;
                        allocations[j].is_allocated = 1;
                        break;
                    }
                }
            }
        } else {
            // 释放内存
            for (int j = 0; j < MAX_ALLOCATIONS; j++) {
                if (allocations[j].is_allocated) {
                    memory_pool_free(allocations[j].ptr);
                    allocations[j].is_allocated = 0;
                    break;
                }
            }
        }
    }
    
    // 清理剩余分配
    for (int j = 0; j < MAX_ALLOCATIONS; j++) {
        if (allocations[j].is_allocated) {
            memory_pool_free(allocations[j].ptr);
        }
    }
    
    clock_t end_time = clock();
    double elapsed_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    
    printf("✓ 性能测试完成\n");
    printf("  迭代次数: %d\n", TEST_ITERATIONS);
    printf("  执行时间: %.3f 秒\n", elapsed_time);
    printf("  平均每次操作: %.6f 秒\n", elapsed_time / TEST_ITERATIONS);
    
    printf("\n");
}

// 测试内存池边界情况
void test_edge_cases() {
    printf("=== 测试内存池边界情况 ===\n");
    
    // 测试零大小分配
    void *ptr1 = memory_pool_alloc(0);
    if (ptr1 == NULL) {
        printf("✓ 零大小分配测试通过\n");
    } else {
        printf("✗ 零大小分配测试失败\n");
        memory_pool_free(ptr1);
    }
    
    // 测试NULL指针释放
    memory_pool_free(NULL);
    printf("✓ NULL指针释放测试通过\n");
    
    // 测试calloc
    void *ptr2 = memory_pool_calloc(10, 64);
    if (ptr2) {
        // 验证是否清零
        char *check = (char*)ptr2;
        int is_zero = 1;
        for (int i = 0; i < 640; i++) {
            if (check[i] != 0) {
                is_zero = 0;
                break;
            }
        }
        if (is_zero) {
            printf("✓ calloc清零测试通过\n");
        } else {
            printf("✗ calloc清零测试失败\n");
        }
        memory_pool_free(ptr2);
    } else {
        printf("✗ calloc分配测试失败\n");
    }
    
    // 测试realloc
    void *ptr3 = memory_pool_alloc(100);
    if (ptr3) {
        strcpy(ptr3, "原始数据");
        void *ptr4 = memory_pool_realloc(ptr3, 200);
        if (ptr4 && strcmp(ptr4, "原始数据") == 0) {
            printf("✓ realloc测试通过\n");
        } else {
            printf("✗ realloc测试失败\n");
        }
        memory_pool_free(ptr4);
    } else {
        printf("✗ realloc基础分配测试失败\n");
    }
    
    printf("\n");
}

// 测试内存池统计功能
void test_statistics() {
    printf("=== 测试内存池统计功能 ===\n");
    
    // 获取初始统计
    size_t initial_allocated = memory_pool_get_total_allocated();
    int initial_count = memory_pool_get_allocation_count();
    
    printf("初始状态:\n");
    printf("  总分配内存: %zu 字节\n", initial_allocated);
    printf("  分配次数: %d\n", initial_count);
    
    // 执行一些分配操作
    void *ptrs[10];
    for (int i = 0; i < 10; i++) {
        ptrs[i] = memory_pool_alloc(128);
    }
    
    // 获取分配后统计
    size_t after_alloc_allocated = memory_pool_get_total_allocated();
    int after_alloc_count = memory_pool_get_allocation_count();
    
    printf("分配后状态:\n");
    printf("  总分配内存: %zu 字节\n", after_alloc_allocated);
    printf("  分配次数: %d\n", after_alloc_count);
    
    // 释放内存
    for (int i = 0; i < 10; i++) {
        memory_pool_free(ptrs[i]);
    }
    
    // 获取释放后统计
    size_t after_free_allocated = memory_pool_get_total_allocated();
    int after_free_count = memory_pool_get_allocation_count();
    
    printf("释放后状态:\n");
    printf("  总分配内存: %zu 字节\n", after_free_allocated);
    printf("  分配次数: %d\n", after_free_count);
    
    printf("✓ 统计功能测试完成\n");
    printf("\n");
}

int main() {
    printf("=== 内存池模块测试程序 ===\n\n");
    
    // 注意：这个测试程序需要在内存池模块已经初始化的环境中运行
    // 在实际使用中，应该通过主程序启动内存池模块
    
    printf("注意：此测试程序需要在内存池模块已初始化的环境中运行\n");
    printf("建议通过主程序启动内存池模块后再运行此测试\n\n");
    
    // 测试基本功能
    test_basic_functionality();
    
    // 测试性能
    test_performance();
    
    // 测试边界情况
    test_edge_cases();
    
    // 测试统计功能
    test_statistics();
    
    printf("=== 内存池测试完成 ===\n");
    return 0;
}
