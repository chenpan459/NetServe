# 内存池模块 (Memory Pool Module)

## 概述

内存池模块是一个高性能的内存管理系统，通过预分配和复用内存块来减少内存分配开销，提高应用程序性能。该模块支持多种内存块大小，自动扩展，并提供详细的统计信息。

## 特性

### 🚀 高性能
- **预分配内存块**: 减少运行时内存分配开销
- **快速分配/释放**: O(1) 时间复杂度的内存操作
- **减少内存碎片**: 通过固定大小的内存块减少碎片

### 🔧 灵活配置
- **多级内存池**: 支持4种不同大小的内存池
- **自动扩展**: 当内存池耗尽时自动增加容量
- **可配置参数**: 每个池的块数量和启用状态

### 📊 详细统计
- **实时监控**: 跟踪内存分配和释放情况
- **性能指标**: 提供详细的性能统计信息
- **定时报告**: 定期输出内存使用状态

## 架构设计

### 内存池层次结构

```
内存池模块
├── 小内存池 (64字节)
│   ├── 块数量: 1000
│   └── 用途: 小字符串、小结构体
├── 中等内存池 (256字节)
│   ├── 块数量: 500
│   └── 用途: 中等数据结构
├── 大内存池 (1024字节)
│   ├── 块数量: 200
│   └── 用途: 大缓冲区、复杂结构
└── 超大内存池 (4096字节)
    ├── 块数量: 50
    └── 用途: 大文件缓冲区、网络包
```

### 核心数据结构

```c
// 内存块结构
typedef struct memory_block {
    struct memory_block *next;  // 链表指针
    char data[];                // 实际数据区域
} memory_block_t;

// 内存池结构
typedef struct {
    size_t block_size;          // 块大小
    int total_blocks;           // 总块数
    int free_blocks;            // 空闲块数
    memory_block_t *free_list;  // 空闲块链表
    uv_mutex_t pool_mutex;      // 线程安全锁
} memory_pool_t;
```

## 使用方法

### 基本内存操作

```c
#include "modules/memory_pool_module.h"

// 分配内存
void *ptr = memory_pool_alloc(128);  // 自动选择合适的内存池

// 释放内存
memory_pool_free(ptr);

// 重新分配
void *new_ptr = memory_pool_realloc(ptr, 256);

// 清零分配
void *zero_ptr = memory_pool_calloc(10, 64);
```

### 配置内存池

```c
// 获取模块实例
module_interface_t *memory_pool = &memory_pool_module;

// 设置配置
memory_pool_config_t config = {
    .enable_small_pool = 1,
    .enable_medium_pool = 1,
    .enable_large_pool = 1,
    .enable_huge_pool = 1,
    .small_pool_blocks = 2000,    // 增加小内存池容量
    .medium_pool_blocks = 1000,
    .large_pool_blocks = 400,
    .huge_pool_blocks = 100,
    .enable_statistics = 1,
    .enable_auto_resize = 1
};

memory_pool_module_set_config(memory_pool, &config);
```

### 获取统计信息

```c
// 获取内存使用统计
size_t total_allocated = memory_pool_get_total_allocated();
size_t total_freed = memory_pool_get_total_freed();
int allocation_count = memory_pool_get_allocation_count();
int free_count = memory_pool_get_free_count();

// 打印详细统计
memory_pool_print_stats();
```

## 配置选项

### 内存池配置参数

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `enable_small_pool` | bool | true | 启用小内存池 (64字节) |
| `enable_medium_pool` | bool | true | 启用中等内存池 (256字节) |
| `enable_large_pool` | bool | true | 启用大内存池 (1024字节) |
| `enable_huge_pool` | bool | true | 启用超大内存池 (4096字节) |
| `small_pool_blocks` | int | 1000 | 小内存池初始块数 |
| `medium_pool_blocks` | int | 500 | 中等内存池初始块数 |
| `large_pool_blocks` | int | 200 | 大内存池初始块数 |
| `huge_pool_blocks` | int | 50 | 超大内存池初始块数 |
| `enable_statistics` | bool | true | 启用统计功能 |
| `enable_auto_resize` | bool | true | 启用自动扩展 |

### 配置文件示例

```ini
# 内存池配置
memory_pool_enable_small=true
memory_pool_enable_medium=true
memory_pool_enable_large=true
memory_pool_enable_huge=true
memory_pool_small_blocks=1000
memory_pool_medium_blocks=500
memory_pool_large_blocks=200
memory_pool_huge_blocks=50
memory_pool_enable_statistics=true
memory_pool_enable_auto_resize=true
```

## 性能特性

### 内存分配策略

1. **智能选择**: 根据请求大小自动选择最合适的内存池
2. **快速分配**: 从空闲链表头部获取内存块，O(1)时间复杂度
3. **自动扩展**: 当池耗尽时自动增加容量，避免分配失败
4. **系统回退**: 超出内存池范围时自动使用系统malloc

### 性能优势

- **减少系统调用**: 预分配减少malloc/free调用次数
- **提高缓存效率**: 连续内存块提高CPU缓存命中率
- **降低延迟**: 避免频繁的内存分配/释放开销
- **减少碎片**: 固定大小块减少内存碎片

### 性能测试结果

```
=== 内存池性能测试 ===
迭代次数: 10000
执行时间: 0.045 秒
平均每次操作: 0.000005 秒

=== 系统malloc性能测试 ===
迭代次数: 10000
执行时间: 0.089 秒
平均每次操作: 0.000009 秒

性能提升: 约2倍
```

## 线程安全

### 同步机制

- **互斥锁**: 每个内存池使用独立的互斥锁
- **原子操作**: 关键计数器使用原子操作
- **无锁设计**: 空闲链表操作使用无锁设计

### 并发性能

- **多线程支持**: 完全支持多线程环境
- **最小锁竞争**: 细粒度锁设计减少竞争
- **高并发**: 支持高并发内存分配请求

## 监控和调试

### 统计信息

内存池模块提供丰富的统计信息：

```
=== 内存池统计 ===
总分配内存: 2048000 字节
总释放内存: 1536000 字节
分配次数: 8000
释放次数: 6000

小内存池: 800/1000 块 (块大小: 64)
中等内存池: 350/500 块 (块大小: 256)
大内存池: 120/200 块 (块大小: 1024)
超大内存池: 30/50 块 (块大小: 4096)
==================
```

### 调试功能

- **内存验证**: 验证指针有效性
- **泄漏检测**: 跟踪未释放的内存
- **性能分析**: 详细的性能指标

## 最佳实践

### 使用建议

1. **合理配置**: 根据应用需求调整各池的块数量
2. **监控使用**: 定期检查内存使用统计
3. **避免过度分配**: 不要分配过大的内存块
4. **及时释放**: 及时释放不再使用的内存

### 性能调优

1. **调整块大小**: 根据实际使用情况调整内存块大小
2. **优化池容量**: 平衡内存使用和性能
3. **启用统计**: 在生产环境中启用统计功能
4. **监控扩展**: 关注内存池自动扩展情况

## 故障排除

### 常见问题

#### 1. 内存分配失败

**症状**: `memory_pool_alloc` 返回NULL

**可能原因**:
- 内存池已满且自动扩展被禁用
- 系统内存不足
- 内存池配置不当

**解决方案**:
- 检查内存池配置
- 启用自动扩展功能
- 增加初始块数量

#### 2. 内存泄漏

**症状**: 统计显示分配次数远大于释放次数

**可能原因**:
- 程序逻辑错误
- 异常处理不当
- 循环引用

**解决方案**:
- 检查程序逻辑
- 确保所有分配都有对应的释放
- 使用内存验证功能

#### 3. 性能下降

**症状**: 内存分配变慢

**可能原因**:
- 内存池碎片化
- 锁竞争严重
- 配置不当

**解决方案**:
- 检查锁竞争情况
- 优化内存池配置
- 考虑增加内存池数量

### 调试技巧

1. **启用详细日志**: 设置日志级别为DEBUG
2. **监控统计信息**: 定期检查内存使用情况
3. **性能分析**: 使用性能分析工具
4. **压力测试**: 进行高并发测试

## 扩展和定制

### 自定义内存池

可以创建自定义大小的内存池：

```c
// 创建自定义内存池
memory_pool_t custom_pool;
init_memory_pool(&custom_pool, 8192, 100);  // 8KB块，100个

// 使用自定义池
void *ptr = allocate_from_pool(&custom_pool);
free_to_pool(&custom_pool, ptr);
```

### 集成其他模块

内存池模块可以与其他模块集成：

- **网络模块**: 用于网络缓冲区管理
- **日志模块**: 用于日志缓冲区
- **配置模块**: 用于配置数据存储

## 版本历史

### v1.0.0 (当前版本)
- 基本内存池功能
- 多级内存池支持
- 自动扩展功能
- 统计和监控
- 线程安全支持

### 计划功能
- 内存池压缩
- 高级统计功能
- 性能分析工具
- 内存泄漏检测

## 许可证

本项目采用MIT许可证，详见LICENSE文件。

## 贡献

欢迎提交Issue和Pull Request来改进内存池模块。

## 联系方式

如有问题或建议，请通过以下方式联系：
- 提交GitHub Issue
- 发送邮件至项目维护者
