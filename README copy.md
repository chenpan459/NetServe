# TCP 通信程序 - 模块化架构

这是一个基于 libuv 的模块化 TCP 通信程序，采用插件式架构设计，每个功能都以模块方式加入。

## 🏗️ 架构设计

### 核心架构
- **主程序** (`main.c`) - 程序入口，负责初始化和协调各模块
- **模块管理器** (`modules/module_manager.*`) - 管理所有模块的生命周期
- **网络模块** (`modules/network_module.*`) - 处理TCP连接和通信
- **日志模块** (`modules/logger_module.*`) - 提供日志记录功能
- **配置模块** (`modules/config_module.*`) - 管理程序配置

### 模块接口
每个模块都实现统一的接口：
```c
typedef struct module_interface {
    const char *name;                    // 模块名称
    const char *version;                 // 模块版本
    int (*init)(struct module_interface *self, uv_loop_t *loop);
    int (*start)(struct module_interface *self);
    int (*stop)(struct module_interface *self);
    int (*cleanup)(struct module_interface *self);
    module_state_t state;                // 模块状态
    void *private_data;                  // 模块私有数据
    const char **dependencies;           // 依赖模块列表
    size_t dependency_count;             // 依赖模块数量
} module_interface_t;
```

## 📁 文件结构

```
.
├── main.c                           # 主程序
├── Makefile                        # 编译配置
├── README.md                       # 说明文档
└── modules/                        # 模块目录
    ├── module_manager.h            # 模块管理器头文件
    ├── module_manager.c            # 模块管理器实现
    ├── network_module.h            # 网络模块头文件
    ├── network_module.c            # 网络模块实现
    ├── logger_module.h             # 日志模块头文件
    ├── logger_module.c             # 日志模块实现
    ├── config_module.h             # 配置模块头文件
    └── config_module.c             # 配置模块实现
```

## 🔧 编译要求

- GCC 编译器 (支持 C99 标准)
- libuv 库 (异步I/O)
- pthread 库 (多线程支持)

## 🚀 编译和运行

### 编译程序
```bash
# 编译所有模块
make

# 编译调试版本
make debug

# 编译发布版本
make release
```

### 运行程序
```bash
# 运行程序
make run

# 或者直接运行
./build/tcp_server_modular
```

### 清理编译文件
```bash
make clean
```

## 📋 模块功能

### 1. 模块管理器 (Module Manager)
- **功能**: 管理所有模块的注册、启动、停止和清理
- **特性**: 
  - 模块依赖检查
  - 生命周期管理
  - 状态监控
  - 动态模块注册

### 2. 网络模块 (Network Module)
- **功能**: 处理TCP服务器和客户端连接
- **特性**:
  - 异步TCP服务器
  - 多客户端支持
  - 消息收发
  - 连接管理
  - 可配置端口和主机

### 3. 日志模块 (Logger Module)
- **功能**: 提供统一的日志记录功能
- **特性**:
  - 多级别日志 (DEBUG, INFO, WARN, ERROR, FATAL)
  - 彩色控制台输出
  - 文件日志支持
  - 时间戳
  - 可配置日志级别

### 4. 配置模块 (Config Module)
- **功能**: 管理程序配置
- **特性**:
  - 支持多种数据类型 (字符串、整数、浮点数、布尔值)
  - 配置文件读写
  - 运行时配置修改
  - 自动保存
  - 线程安全

## 🔌 扩展新模块

### 创建新模块步骤

1. **定义模块接口**
```c
// my_module.h
#include "module_manager.h"

extern module_interface_t my_module;

int my_module_init(module_interface_t *self, uv_loop_t *loop);
int my_module_start(module_interface_t *self);
int my_module_stop(module_interface_t *self);
int my_module_cleanup(module_interface_t *self);
```

2. **实现模块功能**
```c
// my_module.c
#include "my_module.h"

module_interface_t my_module = {
    .name = "my_module",
    .version = "1.0.0",
    .init = my_module_init,
    .start = my_module_start,
    .stop = my_module_stop,
    .cleanup = my_module_cleanup,
    .state = MODULE_STATE_UNINITIALIZED,
    .private_data = NULL,
    .dependencies = NULL,
    .dependency_count = 0
};

// 实现各个函数...
```

3. **在主程序中注册模块**
```c
// 在 main.c 中添加
#include "modules/my_module.h"

// 在 initialize_program() 中注册
module_manager_register_module(module_mgr, &my_module);
```

## ⚙️ 配置说明

### 网络模块配置
```c
network_config_t net_config = {
    .port = 8080,
    .host = "0.0.0.0",
    .backlog = 128,
    .max_connections = 1000
};
```

### 日志模块配置
```c
logger_config_t log_config = {
    .level = LOG_LEVEL_INFO,
    .log_file = "app.log",
    .enable_console = 1,
    .enable_file = 1,
    .enable_timestamp = 1
};
```

### 配置模块配置
```c
config_module_config_t cfg_config = {
    .config_file = "config.ini",
    .auto_save = 1,
    .auto_reload = 0
};
```

## 🧪 测试和调试

### 运行测试
```bash
# 启动服务器
./build/tcp_server_modular

# 使用 telnet 测试连接
telnet localhost 8080

# 或者使用 netcat
nc localhost 8080
```

### 调试模式
```bash
# 编译调试版本
make debug

# 使用 gdb 调试
gdb ./build/tcp_server_modular
```

## 📊 性能特性

- **异步I/O**: 基于 libuv 的事件驱动架构
- **多线程**: 支持多线程并发处理
- **内存管理**: 正确的内存分配和释放
- **模块化**: 按需加载和卸载模块
- **可扩展**: 易于添加新功能和模块

## 🔒 安全特性

- **输入验证**: 所有用户输入都经过验证
- **内存安全**: 防止内存泄漏和缓冲区溢出
- **错误处理**: 完善的错误处理和恢复机制
- **资源管理**: 正确的资源分配和释放

## 🚧 故障排除

### 常见问题

1. **编译错误**
   - 检查 libuv 库是否正确安装
   - 确认编译器支持 C99 标准
   - 检查头文件路径

2. **运行时错误**
   - 检查端口是否被占用
   - 确认防火墙设置
   - 查看日志输出

3. **模块加载失败**
   - 检查模块依赖关系
   - 确认模块初始化函数
   - 查看模块状态

## 📈 未来扩展

- **插件系统**: 支持动态加载插件
- **配置热重载**: 运行时重新加载配置
- **监控接口**: 提供性能监控接口
- **集群支持**: 支持多实例集群
- **安全模块**: 添加认证和加密功能

## 📄 许可证

此代码基于 libuv 许可证，遵循相应的开源协议。

## 🤝 贡献

欢迎提交 Issue 和 Pull Request 来改进这个项目！
