# IPC模块化架构说明

## 概述

本项目将IPC（进程间通信）模块重新设计为模块化架构，每种通信方式都有独立的实现模块，然后通过统一的接口进行汇总。这种设计提高了代码的模块化程度、可维护性和可扩展性。

## 架构设计

### 1. 模块化结构

```
src/ipc/
├── ipc_socket.c/h          # Socket IPC通信模块
├── ipc_pipe.c/h            # Pipe IPC通信模块  
├── ipc_shmem.c/h           # Shared Memory IPC通信模块
├── ipc_msgqueue.c/h        # Message Queue IPC通信模块
├── ipc_semaphore.c/h       # Semaphore IPC通信模块
├── ipc_mutex.c/h           # Mutex IPC通信模块
├── ipc_unified.c/h         # 统一IPC接口模块
├── ipc_module.c/h          # 主IPC模块（兼容性接口）
└── README_MODULAR.md       # 本文档
```

### 2. 通信方式分类

#### 2.1 Socket IPC (`ipc_socket`)
- **类型**: Unix域套接字
- **特点**: 面向连接、可靠、支持双向通信
- **适用场景**: 需要可靠传输的客户端-服务器通信
- **主要函数**:
  - `ipc_socket_create_server()` - 创建Socket服务器
  - `ipc_socket_connect_client()` - 客户端连接
  - `ipc_socket_send_message()` - 发送消息
  - `ipc_socket_receive_message()` - 接收消息

#### 2.2 Pipe IPC (`ipc_pipe`)
- **类型**: 命名管道（FIFO）
- **特点**: 单向通信、简单易用
- **适用场景**: 简单的数据流传输
- **主要函数**:
  - `ipc_pipe_create_server()` - 创建命名管道
  - `ipc_pipe_connect_client()` - 连接管道
  - `ipc_pipe_send_message()` - 发送消息
  - `ipc_pipe_receive_message()` - 接收消息

#### 2.3 Shared Memory IPC (`ipc_shmem`)
- **类型**: 共享内存
- **特点**: 最高性能、需要同步机制
- **适用场景**: 大数据量、高频访问
- **主要函数**:
  - `ipc_shmem_create_server()` - 创建共享内存
  - `ipc_shmem_connect_client()` - 连接共享内存
  - `ipc_shmem_send_message()` - 写入数据
  - `ipc_shmem_receive_message()` - 读取数据

#### 2.4 Message Queue IPC (`ipc_msgqueue`)
- **类型**: System V消息队列
- **特点**: 异步、支持消息类型、队列管理
- **适用场景**: 需要消息分类和优先级
- **主要函数**:
  - `ipc_msgqueue_create_server()` - 创建消息队列
  - `ipc_msgqueue_connect_client()` - 连接消息队列
  - `ipc_msgqueue_send_message()` - 发送消息
  - `ipc_msgqueue_receive_message()` - 接收消息

#### 2.5 Semaphore IPC (`ipc_semaphore`)
- **类型**: System V信号量
- **特点**: 同步原语、计数信号量
- **适用场景**: 进程同步、资源计数
- **主要函数**:
  - `ipc_semaphore_create_server()` - 创建信号量
  - `ipc_semaphore_wait()` - 等待信号量
  - `ipc_semaphore_signal()` - 释放信号量

#### 2.6 Mutex IPC (`ipc_mutex`)
- **类型**: 基于信号量的互斥锁
- **特点**: 互斥访问、防止死锁
- **适用场景**: 临界区保护
- **主要函数**:
  - `ipc_mutex_create_server()` - 创建互斥锁
  - `ipc_mutex_lock()` - 锁定
  - `ipc_mutex_unlock()` - 解锁

### 3. 统一接口层 (`ipc_unified`)

统一接口层提供了统一的API，隐藏了不同IPC方式的实现细节：

```c
// 统一的配置结构
typedef struct {
    char name[64];              // IPC名称
    ipc_type_t type;            // IPC类型
    size_t buffer_size;         // 缓冲区大小
    size_t max_msg_size;        // 最大消息大小
    int timeout_ms;             // 超时时间
    int max_connections;        // 最大连接数
    bool enable_encryption;     // 是否启用加密
    bool enable_compression;    // 是否启用压缩
} ipc_config_t;

// 主要接口函数
int ipc_unified_init(void);
int ipc_unified_create_server(const ipc_config_t *config);
int ipc_unified_connect_to_server(const char *name, const ipc_config_t *config);
int ipc_unified_send_message(int conn_id, const void *data, size_t size);
int ipc_unified_receive_message(int conn_id, void **data, size_t *size, int timeout_ms);
```

### 4. 兼容性接口 (`ipc_module`)

为了保持向后兼容，主IPC模块通过宏定义将旧接口映射到新的统一接口：

```c
#define ipc_module_init() ipc_unified_init()
#define ipc_create_server(config) ipc_unified_create_server(config)
#define ipc_connect_to_server(name, config) ipc_unified_connect_to_server(name, config)
// ... 更多映射
```

## 使用示例

### 1. 基本使用流程

```c
#include "ipc_module.h"

int main() {
    // 初始化IPC模块
    if (ipc_module_init() != 0) {
        printf("IPC模块初始化失败\n");
        return -1;
    }
    
    // 配置IPC
    ipc_config_t config = {
        .name = "test_server",
        .type = IPC_TYPE_SOCKET,
        .buffer_size = 1024 * 1024,
        .max_msg_size = 64 * 1024,
        .timeout_ms = 5000,
        .max_connections = 10,
        .enable_encryption = false,
        .enable_compression = false
    };
    
    // 创建服务器
    int server_id = ipc_create_server(&config);
    if (server_id < 0) {
        printf("创建服务器失败\n");
        return -1;
    }
    
    // 客户端连接
    int conn_id = ipc_connect_to_server("test_server", &config);
    if (conn_id < 0) {
        printf("连接服务器失败\n");
        return -1;
    }
    
    // 发送消息
    const char *message = "Hello, IPC!";
    if (ipc_send_message(conn_id, message, strlen(message)) != 0) {
        printf("发送消息失败\n");
        return -1;
    }
    
    // 接收消息
    void *received_data;
    size_t received_size;
    if (ipc_receive_message(conn_id, &received_data, &received_size, 5000) == 0) {
        printf("收到消息: %.*s\n", (int)received_size, (char*)received_data);
        free(received_data);
    }
    
    // 断开连接
    ipc_disconnect(conn_id);
    
    // 清理资源
    ipc_module_cleanup();
    return 0;
}
```

### 2. 不同IPC类型的使用

```c
// Socket IPC
ipc_config_t socket_config = {
    .name = "socket_server",
    .type = IPC_TYPE_SOCKET,
    .max_connections = 100
};

// Pipe IPC
ipc_config_t pipe_config = {
    .name = "pipe_server",
    .type = IPC_TYPE_PIPE,
    .buffer_size = 64 * 1024
};

// Shared Memory IPC
ipc_config_t shmem_config = {
    .name = "shmem_server",
    .type = IPC_TYPE_SHMEM,
    .buffer_size = 1024 * 1024  // 1MB
};

// Message Queue IPC
ipc_config_t msgq_config = {
    .name = "msgq_server",
    .type = IPC_TYPE_MSGQUEUE
};

// Semaphore IPC
ipc_config_t sem_config = {
    .name = "sem_server",
    .type = IPC_TYPE_SEMAPHORE
};

// Mutex IPC
ipc_config_t mutex_config = {
    .name = "mutex_server",
    .type = IPC_TYPE_MUTEX
};
```

## 优势

### 1. 模块化设计
- 每种IPC方式独立实现，便于维护和调试
- 可以单独编译和测试各个模块
- 便于添加新的IPC通信方式

### 2. 统一接口
- 提供一致的API，降低学习成本
- 可以在运行时选择不同的IPC方式
- 便于代码重构和优化

### 3. 向后兼容
- 保持原有接口不变，现有代码无需修改
- 通过宏定义实现接口映射
- 平滑过渡到新架构

### 4. 性能优化
- 每种IPC方式可以针对特定场景优化
- 避免不必要的抽象层开销
- 支持混合使用不同的IPC方式

## 扩展性

### 1. 添加新的IPC方式
1. 创建新的模块文件（如 `ipc_newtype.c/h`）
2. 在 `ipc_type_t` 枚举中添加新类型
3. 在 `ipc_unified.c` 中添加相应的处理逻辑
4. 更新统一接口和兼容性接口

### 2. 添加新功能
1. 在相应的模块中实现新功能
2. 在统一接口中添加新的API
3. 在兼容性接口中添加相应的宏定义

## 注意事项

### 1. 编译依赖
- 需要链接 `pthread` 库（多线程支持）
- 某些IPC方式可能需要特定的系统库
- 建议使用CMake进行跨平台编译

### 2. 平台兼容性
- Unix域套接字主要在Unix/Linux系统上使用
- 某些IPC方式在Windows上可能需要不同的实现
- 建议添加平台检测和条件编译

### 3. 错误处理
- 所有函数都返回错误码，需要检查返回值
- 资源清理很重要，避免内存泄漏
- 建议使用统一的错误处理机制

## 总结

模块化IPC架构提供了更好的代码组织、维护性和扩展性。通过将不同的IPC通信方式分离到独立模块中，同时保持统一的接口，既满足了模块化设计的需求，又保持了向后兼容性。这种架构为未来的功能扩展和性能优化奠定了良好的基础。
