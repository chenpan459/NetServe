# 配置文件注释功能说明

## 概述

NetServe 的配置解析器现在完全支持注释功能，包括行首注释和行内注释。

## 注释规则

### 1. 行首注释
以 `#` 开头的行会被完全忽略，不进行解析：

```ini
# 这是一个注释行
# 这行不会被执行
# network_port=9999  # 这行被注释掉了
```

### 2. 行内注释
在配置项后面的 `#` 符号及其后面的内容会被忽略：

```ini
network_port=8081          # 这是行内注释
http_port=8080             # HTTP服务器端口
server_name=NetServe       # 服务器名称
```

### 3. 空行处理
空行会被自动跳过，不会影响配置解析。

## 配置文件示例

```ini
# ========================================
# NetServe 配置文件
# ========================================

# 网络配置
network_port=8081          # 网络模块端口
network_host=0.0.0.0       # 监听地址
network_backlog=128         # 连接队列长度

# 日志配置
log_level=1                 # 日志级别 (1=INFO, 2=DEBUG, 3=ERROR)
log_file=server.log         # 日志文件名
log_enable_console=true     # 启用控制台输出

# 服务器配置
server_name=NetServe        # 服务器名称
server_version=1.0.0        # 服务器版本
server_debug_mode=false     # 调试模式 (true/false)

# 性能配置
max_threads=4               # 最大线程数
buffer_size=8192            # 缓冲区大小 (字节)
timeout_seconds=30          # 超时时间 (秒)

# 内存池配置
memory_pool_enable_small=true    # 启用小内存块
memory_pool_small_blocks=1000   # 小内存块数量

# 线程池配置
threadpool_thread_count=4        # 线程数量
threadpool_max_queue_size=1000   # 最大队列大小

# 增强网络配置
enhanced_network_port=8082       # 增强网络模块端口
enhanced_network_host=0.0.0.0    # 监听地址

# HTTP模块配置
http_port=8080                   # HTTP模块端口
http_host=0.0.0.0               # 监听地址
http_enable_cors=true            # 启用CORS支持

# 被注释掉的配置项示例
# disabled_feature=true
# old_setting=123
# deprecated_option=value

# 行内注释测试
test_value=456                   # 这个值应该是456
another_test=789                 # 这个值应该是789
```

## 注释最佳实践

### 1. 使用描述性注释
```ini
# 网络配置 - 控制服务器的网络行为
network_port=8081          # 服务器监听的端口号
network_host=0.0.0.0       # 0.0.0.0表示监听所有网络接口
```

### 2. 分组注释
```ini
# ========================================
# 网络配置组
# ========================================
network_port=8081
network_host=0.0.0.0

# ========================================
# 日志配置组
# ========================================
log_level=1
log_file=server.log
```

### 3. 说明配置项用途
```ini
# 性能调优参数
max_threads=4               # 根据CPU核心数调整，建议设置为CPU核心数的1-2倍
buffer_size=8192            # 网络缓冲区大小，影响网络性能
timeout_seconds=30          # 连接超时时间，防止僵尸连接
```

### 4. 版本和变更记录
```ini
# 配置文件版本: 1.0.0
# 最后更新: 2024-01-01
# 变更记录:
# - 添加了新的内存池配置
# - 调整了默认端口分配
# - 启用了CORS支持
```

## 测试注释功能

### 1. 运行测试程序
```bash
# 构建测试程序
mkdir test_build
cd test_build
cmake -f ../test_config_CMakeLists.txt ..
make

# 运行测试
./test_config_parser
```

### 2. 测试用例
测试程序会验证以下功能：
- 行首注释被正确忽略
- 行内注释被正确截断
- 被注释的配置项返回默认值
- 带注释的配置项正确解析

## 注意事项

### 1. 注释符号
- 只支持 `#` 作为注释符号
- 不支持其他注释符号（如 `//` 或 `/* */`）

### 2. 引号处理
- 注释处理在引号处理之前进行
- 如果配置值包含 `#` 符号，需要使用引号包围

### 3. 编码
- 建议使用 UTF-8 编码保存配置文件
- 支持中文注释

### 4. 性能
- 注释处理对性能影响很小
- 配置文件加载速度基本不受影响

## 故障排除

### 1. 注释不生效
- 检查 `#` 符号是否正确
- 确保没有多余的空格
- 验证配置文件编码

### 2. 配置项解析错误
- 检查行内注释格式
- 确保 `=` 符号前后没有多余空格
- 验证配置值格式

### 3. 中文注释乱码
- 确保配置文件使用 UTF-8 编码
- 检查终端/编辑器编码设置

## 总结

注释功能让配置文件更加清晰易读，便于维护和理解。通过合理使用注释，可以：
- 提高配置文件的可读性
- 记录配置项的用途和说明
- 便于团队协作和配置管理
- 减少配置错误的发生
