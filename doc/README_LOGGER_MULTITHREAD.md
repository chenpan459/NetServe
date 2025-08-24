# NetServe 多线程日志模块

## 概述

NetServe 的日志模块现在完全支持多线程环境，使用队列和多线程消息同步来解决多线程写日志时的乱序问题。

## 核心特性

### 1. 异步日志处理
- **消息队列**: 使用线程安全的队列存储日志消息
- **工作线程**: 专门的线程处理日志写入，避免阻塞主线程
- **条件变量**: 使用条件变量实现线程间的消息同步

### 2. 线程安全
- **互斥锁**: 保护共享资源（文件写入、队列操作）
- **原子操作**: 确保队列操作的原子性
- **无锁设计**: 减少锁竞争，提高性能

### 3. 灵活配置
- **异步/同步**: 可选择异步或同步日志记录
- **队列大小**: 可配置最大队列大小
- **刷新间隔**: 可配置文件刷新间隔

## 架构设计

```
┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│   线程1     │    │   线程2     │    │   线程N     │
│  log_info() │    │  log_warn() │    │ log_error() │
└─────────────┘    └─────────────┘    └─────────────┘
       │                   │                   │
       └───────────────────┼───────────────────┘
                           │
                    ┌─────────────┐
                    │  消息队列   │ ← 线程安全队列
                    │  (FIFO)    │
                    └─────────────┘
                           │
                    ┌─────────────┐
                    │  工作线程   │ ← 专门处理日志
                    │             │
                    └─────────────┘
                           │
              ┌─────────────────────────┐
              │                         │
        ┌─────────────┐        ┌─────────────┐
        │   控制台    │        │   日志文件  │
        │             │        │             │
        └─────────────┘        └─────────────┘
```

## API 接口

### 异步日志函数（推荐）
```c
void log_debug(const char *format, ...);    // 调试日志
void log_info(const char *format, ...);     // 信息日志
void log_warn(const char *format, ...);     // 警告日志
void log_error(const char *format, ...);    // 错误日志
void log_fatal(const char *format, ...);    // 致命错误日志
```

### 同步日志函数（紧急情况）
```c
void log_debug_sync(const char *format, ...);    // 同步调试日志
void log_info_sync(const char *format, ...);     // 同步信息日志
void log_warn_sync(const char *format, ...);     // 同步警告日志
void log_error_sync(const char *format, ...);    // 同步错误日志
void log_fatal_sync(const char *format, ...);    // 同步致命错误日志
```

### 队列管理函数
```c
int log_queue_push(log_message_t *message);     // 添加消息到队列
log_message_t* log_queue_pop(void);             // 从队列取出消息
void log_queue_clear(void);                     // 清空队列
size_t log_queue_size(void);                    // 获取队列大小
```

### 控制函数
```c
int logger_enable_async(int enable);            // 启用/禁用异步日志
int logger_flush(void);                         // 强制刷新日志
```

## 配置选项

### 日志模块配置结构
```c
typedef struct {
    log_level_t level;              // 日志级别
    char *log_file;                 // 日志文件路径
    int enable_console;             // 启用控制台输出
    int enable_file;                // 启用文件输出
    int enable_timestamp;           // 启用时间戳
    int enable_async;               // 启用异步日志
    size_t max_queue_size;          // 最大队列大小
    int flush_interval_ms;          // 刷新间隔（毫秒）
} logger_config_t;
```

### 默认配置
```c
static logger_config_t default_config = {
    .level = LOG_LEVEL_INFO,        // 默认日志级别
    .log_file = NULL,               // 默认不输出到文件
    .enable_console = 1,            // 默认启用控制台
    .enable_file = 0,               // 默认禁用文件输出
    .enable_timestamp = 1,          // 默认启用时间戳
    .enable_async = 1,              // 默认启用异步日志
    .max_queue_size = 10000,        // 默认最大队列大小
    .flush_interval_ms = 100        // 默认刷新间隔100ms
};
```

## 使用示例

### 1. 基本使用
```c
#include "src/modules/logger_module.h"

// 设置日志级别
logger_set_level(LOG_LEVEL_DEBUG);

// 记录日志
log_info("服务器启动成功");
log_warn("内存使用率较高: %d%%", memory_usage);
log_error("连接失败: %s", error_message);
```

### 2. 多线程环境
```c
#include <pthread.h>

void* worker_thread(void *arg) {
    int thread_id = *(int*)arg;
    
    for (int i = 0; i < 100; i++) {
        // 异步日志（推荐）
        log_info("线程 %d: 处理任务 %d", thread_id, i);
        
        // 模拟工作
        usleep(1000);
    }
    
    return NULL;
}

int main() {
    pthread_t threads[5];
    int thread_ids[5];
    
    // 创建多个线程
    for (int i = 0; i < 5; i++) {
        thread_ids[i] = i + 1;
        pthread_create(&threads[i], NULL, worker_thread, &thread_ids[i]);
    }
    
    // 等待线程完成
    for (int i = 0; i < 5; i++) {
        pthread_join(threads[i], NULL);
    }
    
    return 0;
}
```

### 3. 配置日志模块
```c
// 配置日志模块
logger_config_t config = {
    .level = LOG_LEVEL_DEBUG,
    .log_file = "server.log",
    .enable_console = 1,
    .enable_file = 1,
    .enable_timestamp = 1,
    .enable_async = 1,
    .max_queue_size = 5000,
    .flush_interval_ms = 50
};

logger_module_set_config(&logger_mod, &config);
```

## 性能特性

### 1. 异步处理优势
- **非阻塞**: 日志记录不会阻塞业务线程
- **高性能**: 批量处理日志消息，减少I/O操作
- **低延迟**: 快速返回，提高响应速度

### 2. 队列管理
- **内存效率**: 预分配消息结构，减少内存分配
- **溢出保护**: 队列满时自动降级为同步写入
- **批量处理**: 工作线程批量处理队列中的消息

### 3. 文件I/O优化
- **缓冲写入**: 使用标准库缓冲，减少系统调用
- **定时刷新**: 定期刷新缓冲区，平衡性能和数据安全
- **互斥保护**: 文件写入使用互斥锁，确保线程安全

## 故障排除

### 1. 常见问题

#### 队列满错误
```
错误: 日志队列已满，降级为同步写入
解决: 增加 max_queue_size 或减少日志频率
```

#### 工作线程启动失败
```
错误: 无法创建工作线程
解决: 检查系统资源，确保有足够的线程配额
```

#### 日志文件写入失败
```
错误: 无法打开日志文件
解决: 检查文件路径、权限和磁盘空间
```

### 2. 性能调优

#### 队列大小调整
```c
// 高并发环境
config.max_queue_size = 20000;

// 低延迟要求
config.flush_interval_ms = 25;
```

#### 日志级别控制
```c
// 生产环境
logger_set_level(LOG_LEVEL_INFO);

// 开发环境
logger_set_level(LOG_LEVEL_DEBUG);
```

### 3. 监控和调试

#### 队列状态监控
```c
size_t queue_size = log_queue_size();
printf("当前日志队列大小: %zu\n", queue_size);
```

#### 强制刷新
```c
// 程序退出前强制刷新
logger_flush();
```

## 最佳实践

### 1. 日志级别使用
- **DEBUG**: 详细的调试信息，仅在开发时使用
- **INFO**: 一般信息，记录程序运行状态
- **WARN**: 警告信息，不影响程序运行但需要注意
- **ERROR**: 错误信息，程序可以继续运行
- **FATAL**: 致命错误，程序无法继续运行

### 2. 日志内容规范
```c
// 好的日志格式
log_info("用户 %s 登录成功，IP: %s", username, ip_address);
log_warn("数据库连接池使用率: %d%%，接近阈值", usage_percent);
log_error("API调用失败: %s, 状态码: %d", api_name, status_code);

// 避免的日志格式
log_info("something happened");           // 信息不明确
log_error("error");                       // 错误信息不详细
log_debug("debug info");                  // 生产环境避免DEBUG
```

### 3. 性能考虑
- 使用异步日志函数，避免阻塞主线程
- 合理设置队列大小，平衡内存使用和性能
- 定期检查队列状态，及时发现问题

## 测试验证

### 1. 运行测试程序
```bash
# 构建测试程序
mkdir test_build
cd test_build
cmake -f ../test_logger_CMakeLists.txt ..
make

# 运行多线程测试
./test_logger_multithread
```

### 2. 验证要点
- 多线程日志记录不出现乱序
- 队列正常工作，消息正确传递
- 文件和控制台输出一致
- 性能表现符合预期

## 总结

新的多线程日志模块通过以下技术解决了多线程写日志的乱序问题：

1. **消息队列**: 确保日志消息按顺序存储
2. **工作线程**: 专门处理日志写入，避免竞争
3. **条件变量**: 实现线程间的消息同步
4. **互斥锁**: 保护共享资源，确保线程安全

这些改进使得日志模块在高并发环境下更加稳定和高效，同时保持了良好的性能和易用性。
