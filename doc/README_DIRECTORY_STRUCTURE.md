# NetServe 项目目录结构调整说明

## 概述

本文档说明了 NetServe 项目的目录结构调整，以及相应的 CMakeLists.txt 和包含路径的更新。

## 新的目录结构

```
src/
├── main.c                          # 主程序入口
├── modules/                        # 核心模块
│   └── module_manager.h           # 模块管理器
├── config/                         # 配置模块
│   ├── config_module.h
│   └── config_module.c
├── memory/                         # 内存管理模块
│   ├── memory_pool_module.h
│   └── memory_pool_module.c
├── thread/                         # 线程管理模块
│   ├── threadpool_module.h
│   └── threadpool_module.c
├── net/                           # 网络模块
│   ├── enhanced_network_module.h
│   └── enhanced_network_module.c
├── log/                           # 日志模块
│   ├── logger_module.h
│   └── logger_module.c
├── http/                          # HTTP模块
│   ├── http_module.h
│   ├── http_module.c
│   └── http_routes.c
└── json/                          # JSON解析模块
    ├── json_parser_module.h
    └── json_parser_module.c
```

## CMakeLists.txt 更新

### 1. 模块源文件路径更新

```cmake
# 模块源文件
file(GLOB MODULE_SRCS 
    "src/modules/*.c"
    "src/log/*.c"
    "src/http/*.c"
    "src/config/*.c"
    "src/memory/*.c"
    "src/thread/*.c"
    "src/net/*.c"
    "src/json/*.c"
)
```

### 2. 包含目录更新

```cmake
# 设置包含目录
target_include_directories(tcp_server_multithreaded PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules
    ${CMAKE_CURRENT_SOURCE_DIR}/src/http
    ${CMAKE_CURRENT_SOURCE_DIR}/src/log
    ${CMAKE_CURRENT_SOURCE_DIR}/src/config
    ${CMAKE_CURRENT_SOURCE_DIR}/src/memory
    ${CMAKE_CURRENT_SOURCE_DIR}/src/thread
    ${CMAKE_CURRENT_SOURCE_DIR}/src/net
    ${CMAKE_CURRENT_SOURCE_DIR}/src/json
    ${LIBUV_INCLUDE_DIRS}
)
```

## 包含路径更新

### 1. src/main.c

```c
#include "src/modules/module_manager.h"
#include "src/memory/memory_pool_module.h"
#include "src/thread/threadpool_module.h"
#include "src/net/enhanced_network_module.h"
#include "src/log/logger_module.h"
#include "src/config/config_module.h"
#include "src/http/http_module.h"
#include "src/json/json_parser_module.h"
#include "src/http/http_routes.h"
```

### 2. src/http/http_routes.c

```c
#include "src/http/http_module.h"
#include "src/json/json_parser_module.h"
```

### 3. src/http/http_module.h

```c
#include "src/modules/module_manager.h"
```

### 4. src/log/logger_module.h

```c
#include "src/modules/module_manager.h"
```

## 解决的问题

1. **链接错误**: 通过将 `src/http/*.c` 添加到 `MODULE_SRCS` 中，解决了 `http_add_route` 等函数的未定义引用错误。

2. **函数声明顺序**: 通过重新排列 `log_internal_sync` 和 `log_internal_async` 函数的顺序，解决了函数声明冲突问题。

3. **目录结构清晰**: 将相关功能的模块组织到对应的目录中，提高了代码的可维护性。

## 构建说明

现在可以使用以下命令构建项目：

```bash
mkdir build
cd build
cmake ..
make
```

## 注意事项

1. 确保所有模块的头文件都使用正确的相对路径。
2. 如果添加新的模块目录，需要同时更新 `CMakeLists.txt` 中的 `MODULE_SRCS` 和包含目录。
3. 模块之间的依赖关系应该通过 `module_manager.h` 来管理，避免循环依赖。
