# NetServe CMake 构建系统

本项目已从 Makefile 迁移到 CMake 构建系统。

## 系统要求

- CMake 3.10 或更高版本
- C 编译器 (GCC, Clang, MSVC)
- libuv 库
- pthread 库

## 快速开始

### Windows 用户

1. 确保已安装 CMake 和 MinGW
2. 双击运行 `build.bat` 文件
3. 或者手动执行以下命令：

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

## 构建选项

### 调试版本
```bash
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make
```

### 发布版本
```bash
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
```

### 自定义编译器
```bash
cd build
cmake .. -DCMAKE_C_COMPILER=gcc
make
```

## 可用目标

- `make` 或 `cmake --build .` - 构建项目
- `make run` - 运行程序
- `make test` - 启动服务器进行测试
- `make clean` - 清理构建文件

## 项目结构

```
NetServe/
├── CMakeLists.txt          # CMake 配置文件
├── main.c                  # 主程序
├── modules/                # 模块源代码
├── http/                   # HTTP 相关代码
├── config/                 # 配置文件
├── build/                  # 构建输出目录
│   ├── bin/               # 可执行文件
│   └── lib/               # 库文件
├── build.bat              # Windows 构建脚本
└── build.sh               # Linux/macOS 构建脚本
```

## 依赖管理

### libuv 库

CMake 会自动查找 libuv 库。如果库未安装或找不到，请：

1. 安装 libuv 开发包：
   - Ubuntu/Debian: `sudo apt-get install libuv1-dev`
   - CentOS/RHEL: `sudo yum install libuv-devel`
   - macOS: `brew install libuv`

2. 或者指定 libuv 路径：
```bash
cmake .. -DLIBUV_ROOT=/path/to/libuv
```

### pthread 库

pthread 库会自动查找并链接。

## 故障排除

### 常见问题

1. **CMake 找不到 libuv**
   - 确保 libuv 已正确安装
   - 检查 PKG_CONFIG_PATH 环境变量

2. **编译错误**
   - 检查 C 编译器是否正确安装
   - 确保所有依赖库都已安装

3. **链接错误**
   - 检查库文件路径
   - 确保库版本兼容

### 获取帮助

如果遇到问题，请检查：
- CMake 错误信息
- 编译器错误信息
- 依赖库版本兼容性

## 从 Makefile 迁移

如果您之前使用 Makefile，主要变化：

- 使用 `cmake` 和 `make` 替代 `make` 命令
- 构建输出在 `build/` 目录中
- 配置文件为 `CMakeLists.txt`
- 支持更多平台和编译器

## 高级配置

### 自定义编译选项

```bash
cmake .. -DCMAKE_C_FLAGS="-O3 -march=native"
```

### 交叉编译

```bash
cmake .. -DCMAKE_TOOLCHAIN_FILE=path/to/toolchain.cmake
```

### 安装

```bash
cd build
make install
```

默认安装到 `/usr/local/bin/` 和 `/usr/local/etc/netserve/`
