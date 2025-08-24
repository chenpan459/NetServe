#!/bin/bash

echo "检查端口占用情况..."
echo

echo "检查端口 8080 (HTTP模块):"
netstat -an | grep :8080 || echo "端口 8080 未被占用"
echo

echo "检查端口 8081 (网络模块):"
netstat -an | grep :8081 || echo "端口 8081 未被占用"
echo

echo "检查端口 8082 (增强网络模块):"
netstat -an | grep :8082 || echo "端口 8082 未被占用"
echo

echo "检查完成！"
