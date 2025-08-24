#!/bin/bash

echo "正在构建 NetServe 项目..."

# 创建构建目录
mkdir -p build
cd build

# 配置项目
echo "配置项目..."
cmake ..

# 构建项目
echo "构建项目..."
make -j$(nproc)

echo "构建完成！"
echo "可执行文件位置: build/bin/tcp_server_multithreaded"

# 返回上级目录
cd ..

# 显示可执行文件信息
if [ -f "build/bin/tcp_server_multithreaded" ]; then
    echo "构建成功！"
    ls -la build/bin/
else
    echo "构建失败！"
    exit 1
fi
