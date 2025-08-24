# 端口冲突问题解决方案

## 问题描述

如果遇到 "HTTP服务器监听失败: address already in use" 错误，说明端口已被占用。

## 原因分析

您的项目中有多个网络模块，它们之前都配置在同一个端口上：
- **HTTP模块**: 8080端口
- **网络模块**: 8081端口  
- **增强网络模块**: 8082端口

## 解决方案

### 1. 检查端口占用情况

#### Windows用户：
```cmd
check_ports.bat
```

#### Linux/macOS用户：
```bash
chmod +x check_ports.sh
./check_ports.sh
```

### 2. 手动检查端口

#### Windows:
```cmd
netstat -an | findstr :8080
netstat -an | findstr :8081
netstat -an | findstr :8082
```

#### Linux/macOS:
```bash
netstat -an | grep :8080
netstat -an | grep :8081
netstat -an | grep :8082
```

### 3. 终止占用端口的进程

如果发现端口被占用，可以终止相关进程：

#### Windows:
```cmd
# 查找占用端口的进程
netstat -ano | findstr :8080

# 终止进程 (替换 PID 为实际的进程ID)
taskkill /PID <PID> /F
```

#### Linux/macOS:
```bash
# 查找占用端口的进程
lsof -i :8080

# 终止进程 (替换 PID 为实际的进程ID)
kill -9 <PID>
```

### 4. 修改端口配置

如果不想终止现有进程，可以修改配置文件中的端口：

编辑 `config/config.ini` 文件：

```ini
# 网络配置
network_port=8081          # 网络模块端口
network_host=0.0.0.0

# 增强网络配置  
enhanced_network_port=8082 # 增强网络模块端口
enhanced_network_host=0.0.0.0

# HTTP模块配置
http_port=8080             # HTTP模块端口
http_host=0.0.0.0
```

### 5. 重新构建和运行

修改配置后，重新构建项目：

```bash
# 清理旧的构建文件
rm -rf build

# 重新构建
mkdir build
cd build
cmake ..
make
```

## 端口分配建议

为了避免端口冲突，建议使用以下端口分配：

- **HTTP模块**: 8080 (Web服务)
- **网络模块**: 8081 (TCP通信)
- **增强网络模块**: 8082 (高级网络功能)
- **其他服务**: 8083, 8084, ...

## 常见问题

### Q: 为什么会有端口冲突？
A: 多个网络模块同时启动时，如果配置了相同的端口，就会出现冲突。

### Q: 如何避免端口冲突？
A: 确保每个网络模块使用不同的端口，并在配置文件中明确指定。

### Q: 端口被系统保留怎么办？
A: 某些端口可能被系统服务占用，建议使用1024以上的端口。

### Q: 如何动态分配端口？
A: 可以修改代码，让系统自动分配可用端口，或者实现端口检测和自动切换功能。

## 测试验证

修改配置后，可以通过以下方式验证：

1. 运行端口检查脚本
2. 启动程序，观察启动日志
3. 检查各模块是否成功监听不同端口
4. 测试各模块的功能是否正常

## 联系支持

如果问题仍然存在，请：
1. 检查完整的错误日志
2. 确认配置文件格式正确
3. 验证所有依赖库已正确安装
4. 提供系统环境信息
