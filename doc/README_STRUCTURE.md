# NetServe 项目结构

## 目录结构

```
NetServe/
├── src/                           # 源代码目录
│   ├── main.c                     # 主程序入口
│   ├── modules/                   # 模块源代码
│   │   ├── module_manager.c      # 模块管理器
│   │   ├── config_module.c       # 配置模块
│   │   ├── logger_module.c       # 日志模块
│   │   ├── memory_pool_module.c  # 内存池模块
│   │   ├── threadpool_module.c   # 线程池模块
│   │   ├── network_module.c      # 基础网络模块
│   │   ├── enhanced_network_module.c  # 增强网络模块
│   │   ├── http_module.c         # HTTP模块
│   │   ├── json_parser_module.c  # JSON解析器模块
│   │   └── *.h                   # 模块头文件
│   └── http/                      # HTTP相关代码
│       ├── http_routes.c         # HTTP路由处理
│       └── http_routes.h         # HTTP路由头文件
├── config/                        # 配置文件目录
│   └── config.ini                # 主配置文件
├── test/                          # 测试代码目录
├── doc/                           # 文档目录
├── depend/                        # 依赖库目录
├── shell/                         # 脚本文件目录
├── CMakeLists.txt                 # CMake构建配置
├── build.bat                      # Windows构建脚本
├── build.sh                       # Linux/macOS构建脚本
└── .gitignore                     # Git忽略文件
```

## 构建说明

### Windows 用户

1. 确保已安装 CMake 和 MinGW
2. 双击运行 `build.bat` 文件
3. 或者手动执行：

```cmd
mkdir build
cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
```

### Linux/macOS 用户

1. 确保已安装 CMake 和构建工具
2. 运行构建脚本：

```bash
chmod +x build.sh
./build.sh
```

3. 或者手动执行：

```bash
mkdir build
cd build
cmake ..
make
```

## 端口配置

为了避免端口冲突，各模块使用不同的端口：

- **HTTP模块**: 8080端口
- **网络模块**: 8081端口  
- **增强网络模块**: 8082端口

## 模块说明

### 核心模块

- **module_manager**: 模块管理器，负责模块的生命周期管理
- **config_module**: 配置管理，读取和管理配置文件
- **logger_module**: 日志系统，提供统一的日志记录功能

### 网络模块

- **network_module**: 基础TCP网络通信
- **enhanced_network_module**: 增强的网络功能，支持线程池
- **http_module**: HTTP服务器，处理HTTP请求和响应

### 工具模块

- **memory_pool_module**: 内存池管理，提高内存分配效率
- **threadpool_module**: 线程池，管理并发任务
- **json_parser_module**: JSON解析器，处理JSON数据

## 开发指南

### 添加新模块

1. 在 `src/modules/` 目录下创建模块文件
2. 实现模块接口（参考现有模块）
3. 在 `main.c` 中注册新模块
4. 更新 `CMakeLists.txt` 如果添加了新的源文件

### 修改配置

编辑 `config/config.ini` 文件来修改模块配置。程序启动时会自动读取这些配置。

### 调试

使用调试版本构建：

```bash
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make
```

## 故障排除

### 常见问题

1. **端口冲突**: 检查配置文件中的端口设置
2. **编译错误**: 确保所有依赖库已正确安装
3. **运行时错误**: 检查配置文件格式和路径

### 获取帮助

- 查看各模块的源代码注释
- 检查配置文件格式
- 运行测试程序验证功能

## 版本信息

- 项目名称: NetServe
- 版本: 1.0.0
- 构建系统: CMake 3.10+
- 主要依赖: libuv, pthread
