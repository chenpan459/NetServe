# 配置文件管理

## 概述

本项目使用模块化的配置文件管理系统，配置文件存储在`config`目录中，程序启动时会自动检查和创建必要的配置文件。

## 目录结构

```
项目根目录/
├── config/                    # 配置文件目录
│   ├── config.ini            # 主配置文件
│   ├── config_manager.h      # 配置文件管理器头文件
│   └── config_manager.c      # 配置文件管理器实现
├── modules/                   # 模块目录
├── main.c                     # 主程序
└── Makefile                   # 构建文件
```

## 配置文件

### 主配置文件 (config/config.ini)

主配置文件包含所有模块的配置参数，采用INI格式：

```ini
# 网络配置
network_port=8080
network_host=0.0.0.0
network_backlog=128
network_max_connections=1000

# 日志配置
log_level=1
log_file=server.log
log_enable_console=true
log_enable_file=true
log_enable_timestamp=true

# 服务器配置
server_name=TCP_Modular_Server
server_version=1.0.0
server_debug_mode=false
server_auto_restart=true

# 性能配置
max_threads=4
buffer_size=8192
timeout_seconds=30
keep_alive=true

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

# 线程池配置
threadpool_thread_count=4
threadpool_max_queue_size=1000
threadpool_enable_work_stealing=true
threadpool_enable_priority_queue=true

# 增强网络配置
enhanced_network_enable_threadpool=true
enhanced_network_max_concurrent_requests=100
enhanced_network_request_timeout_ms=30000
```

## 配置文件管理器

### 功能特性

- **自动目录创建**: 程序启动时自动检查并创建`config`目录
- **配置文件检查**: 验证配置文件是否存在和格式是否正确
- **默认配置生成**: 如果配置文件不存在，自动生成默认配置
- **跨平台支持**: 支持Windows和Unix/Linux系统

### 主要函数

```c
// 确保配置目录存在
int ensure_config_directory(void);

// 检查配置文件是否存在
int check_config_file(const char *config_path);

// 创建默认配置文件
int create_default_config_file(const char *config_path);

// 验证配置文件格式
int validate_config_file(const char *config_path);
```

## 使用方法

### 程序启动流程

1. **目录检查**: 程序启动时检查`config`目录是否存在
2. **配置文件检查**: 检查`config/config.ini`是否存在
3. **自动创建**: 如果配置文件不存在，自动创建默认配置
4. **格式验证**: 验证配置文件的格式是否正确
5. **模块加载**: 配置模块加载配置文件内容

### 手动创建配置文件

如果需要手动创建配置文件，可以：

1. 创建`config`目录
2. 在`config`目录中创建`config.ini`文件
3. 按照上述格式编写配置内容

### 修改配置

1. 编辑`config/config.ini`文件
2. 修改相应的配置参数
3. 重启程序使配置生效

## 配置参数说明

### 网络配置

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `network_port` | 服务器监听端口 | 8080 |
| `network_host` | 服务器监听地址 | 0.0.0.0 |
| `network_backlog` | 连接队列长度 | 128 |
| `network_max_connections` | 最大连接数 | 1000 |

### 日志配置

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `log_level` | 日志级别 (0-4) | 1 |
| `log_file` | 日志文件名 | server.log |
| `log_enable_console` | 启用控制台输出 | true |
| `log_enable_file` | 启用文件输出 | true |
| `log_enable_timestamp` | 启用时间戳 | true |

### 内存池配置

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `memory_pool_enable_small` | 启用小内存池 | true |
| `memory_pool_small_blocks` | 小内存池块数 | 1000 |
| `memory_pool_enable_medium` | 启用中等内存池 | true |
| `memory_pool_medium_blocks` | 中等内存池块数 | 500 |
| `memory_pool_enable_large` | 启用大内存池 | true |
| `memory_pool_large_blocks` | 大内存池块数 | 200 |
| `memory_pool_enable_huge` | 启用超大内存池 | true |
| `memory_pool_huge_blocks` | 超大内存池块数 | 50 |

### 线程池配置

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `threadpool_thread_count` | 工作线程数 | 4 |
| `threadpool_max_queue_size` | 最大队列大小 | 1000 |
| `threadpool_enable_work_stealing` | 启用工作窃取 | true |
| `threadpool_enable_priority_queue` | 启用优先级队列 | true |

## 扩展配置

### 添加新的配置项

1. 在`config/config.ini`中添加新的配置行
2. 在相应的模块中使用配置模块API读取配置
3. 在`config_manager.c`的默认配置中添加新项

### 配置验证

配置文件管理器会验证：
- 文件是否存在
- 文件格式是否正确
- 是否包含有效的配置项

## 故障排除

### 常见问题

1. **配置文件不存在**
   - 程序会自动创建默认配置文件
   - 检查程序是否有写入权限

2. **配置文件格式错误**
   - 确保使用正确的INI格式
   - 检查是否有语法错误

3. **配置不生效**
   - 重启程序使配置生效
   - 检查配置参数名称是否正确

### 调试技巧

1. 启用详细日志输出
2. 检查配置文件权限
3. 验证配置文件格式

## 最佳实践

1. **备份配置**: 定期备份重要的配置文件
2. **版本控制**: 将配置文件纳入版本控制系统
3. **环境分离**: 为不同环境使用不同的配置文件
4. **参数验证**: 在程序中使用配置前验证参数有效性

## 许可证

本项目采用MIT许可证，详见LICENSE文件。
