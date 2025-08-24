# HTTP模块文档

## 概述

HTTP模块为应用程序提供了完整的HTTP服务器功能，支持RESTful API开发，专门用于JSON数据解析和处理。该模块基于libuv构建，具有高性能、异步I/O和模块化设计的特点。

## 主要特性

### 1. HTTP服务器功能
- 支持HTTP/1.1协议
- 异步I/O处理，基于libuv事件循环
- 支持多种HTTP方法：GET, POST, PUT, DELETE, PATCH, HEAD, OPTIONS
- 内置CORS支持
- 可配置的连接池和超时设置

### 2. 路由系统
- 灵活的路由注册机制
- 支持路径匹配和参数解析
- 支持中间件和自定义处理函数
- 线程安全的路由管理

### 3. JSON处理
- 完整的JSON解析和生成功能
- 支持所有JSON数据类型（null, boolean, number, string, array, object）
- 内存高效的JSON操作
- 错误处理和验证

### 4. 配置管理
- 可配置的服务器参数
- 支持运行时配置更新
- 环境变量和配置文件支持

## 架构设计

### 模块结构
```
HTTP模块
├── HTTP服务器核心
├── 路由管理器
├── 请求/响应处理器
├── JSON解析器
└── 配置管理器
```

### 核心组件

#### 1. HTTP服务器
- 基于libuv的TCP服务器
- 异步连接处理
- 可配置的连接池大小

#### 2. 路由系统
- 链表式路由存储
- 支持动态路由注册/注销
- 路径匹配算法

#### 3. JSON解析器
- 递归下降解析器
- 内存池管理
- 错误恢复机制

## API参考

### 模块生命周期函数

#### `http_module_init`
```c
int http_module_init(module_interface_t *self, uv_loop_t *loop);
```
初始化HTTP模块。

**参数：**
- `self`: 模块接口指针
- `loop`: libuv事件循环

**返回值：**
- 0: 成功
- -1: 失败

#### `http_module_start`
```c
int http_module_start(module_interface_t *self);
```
启动HTTP服务器。

**参数：**
- `self`: 模块接口指针

**返回值：**
- 0: 成功
- -1: 失败

#### `http_module_stop`
```c
int http_module_stop(module_interface_t *self);
```
停止HTTP服务器。

#### `http_module_cleanup`
```c
int http_module_cleanup(module_interface_t *self);
```
清理HTTP模块资源。

### 路由管理函数

#### `http_add_route`
```c
int http_add_route(http_method_t method, const char *path, 
                   http_route_handler_t handler, void *user_data);
```
添加HTTP路由。

**参数：**
- `method`: HTTP方法
- `path`: 路径
- `handler`: 处理函数
- `user_data`: 用户数据

**示例：**
```c
http_add_route(HTTP_METHOD_GET, "/api/users", handle_get_users, NULL);
```

#### `http_remove_route`
```c
int http_remove_route(http_method_t method, const char *path);
```
移除HTTP路由。

#### `http_clear_routes`
```c
void http_clear_routes(void);
```
清理所有路由。

### JSON处理函数

#### `http_set_json_parser`
```c
int http_set_json_parser(json_parser_callback_t parser, void *user_data);
```
设置JSON解析器回调函数。

#### `http_parse_json_request`
```c
int http_parse_json_request(const http_request_t *request, void **parsed_data);
```
解析JSON请求体。

#### `http_create_json_response`
```c
int http_create_json_response(http_response_t *response, http_status_t status, 
                             const char *json_data);
```
创建JSON响应。

### 预定义响应函数

#### `http_send_ok_response`
```c
int http_send_ok_response(http_response_t *response, const char *json_data);
```
发送成功响应（200 OK）。

#### `http_send_error_response`
```c
int http_send_error_response(http_response_t *response, http_status_t status, 
                            const char *message);
```
发送错误响应。

#### `http_send_not_found_response`
```c
int http_send_not_found_response(http_response_t *response);
```
发送404 Not Found响应。

#### `http_send_bad_request_response`
```c
int http_send_bad_request_response(http_response_t *response, const char *message);
```
发送400 Bad Request响应。

## 使用示例

### 1. 基本路由注册

```c
#include "modules/http_module.h"

// 处理函数
int handle_hello(const http_request_t *request, http_response_t *response, void *user_data) {
    (void)request;
    (void)user_data;
    
    const char *json_data = "{\"message\":\"Hello, World!\"}";
    return http_send_ok_response(response, json_data);
}

// 注册路由
void register_routes() {
    http_add_route(HTTP_METHOD_GET, "/api/hello", handle_hello, NULL);
}
```

### 2. JSON数据处理

```c
#include "modules/http_module.h"
#include "modules/json_parser_module.h"

int handle_create_user(const http_request_t *request, http_response_t *response, void *user_data) {
    (void)user_data;
    
    if (!request->body || request->body_length == 0) {
        return http_send_bad_request_response(response, "请求体不能为空");
    }
    
    // 解析JSON
    json_value_t *user_data_json;
    if (json_parse(request->body, request->body_length, &user_data_json) != 0) {
        return http_send_bad_request_response(response, "无效的JSON格式");
    }
    
    // 处理数据...
    
    // 创建响应
    json_value_t *response_obj = json_create_object();
    json_object_set(response_obj, "message", json_create_string("用户创建成功"));
    
    char *json_string = json_stringify(response_obj);
    int result = http_send_ok_response(response, json_string);
    
    // 清理
    free(json_string);
    json_free(response_obj);
    json_free(user_data_json);
    
    return result;
}
```

### 3. 错误处理

```c
int handle_user_operation(const http_request_t *request, http_response_t *response, void *user_data) {
    // 验证用户权限
    if (!check_user_permission(request)) {
        return http_send_error_response(response, HTTP_STATUS_UNAUTHORIZED, "权限不足");
    }
    
    // 验证请求参数
    if (!validate_request_params(request)) {
        return http_send_bad_request_response(response, "参数验证失败");
    }
    
    // 处理业务逻辑...
    
    return http_send_ok_response(response, "{\"status\":\"success\"}");
}
```

## 配置选项

### HTTP模块配置

```ini
# HTTP模块配置
http_port=8080                    # 监听端口
http_host=0.0.0.0                # 监听地址
http_max_connections=1000         # 最大连接数
http_request_timeout_ms=30000     # 请求超时时间（毫秒）
http_enable_cors=true             # 启用CORS
http_cors_origin=*                # CORS允许的源
http_enable_logging=true          # 启用日志
http_enable_json_parsing=true     # 启用JSON解析
```

### JSON解析器配置

```c
json_parser_config_t config = {
    .enable_unicode_escape = 0,    // 禁用Unicode转义
    .enable_comments = 0,         // 禁用注释
    .strict_mode = 1,             // 严格模式
    .max_depth = 100,             // 最大嵌套深度
    .max_string_length = 1024 * 1024  // 最大字符串长度（1MB）
};
```

## 性能优化

### 1. 连接池管理
- 预分配连接对象
- 连接复用
- 自动清理空闲连接

### 2. 内存管理
- 使用内存池减少分配开销
- 缓冲区预分配
- 及时释放不需要的内存

### 3. 异步处理
- 非阻塞I/O操作
- 事件驱动架构
- 多线程支持

## 安全特性

### 1. 输入验证
- JSON格式验证
- 路径参数验证
- 请求大小限制

### 2. CORS支持
- 可配置的跨域策略
- 支持预检请求
- 灵活的头部控制

### 3. 错误处理
- 详细的错误信息
- 安全的错误响应
- 日志记录

## 调试和监控

### 1. 日志系统
- 请求/响应日志
- 错误日志
- 性能统计

### 2. 健康检查
- `/api/health` 端点
- 服务器状态监控
- 性能指标

### 3. 调试工具
- HTTP请求/响应查看
- JSON解析调试
- 路由匹配调试

## 最佳实践

### 1. 路由设计
- 使用RESTful风格
- 清晰的URL结构
- 一致的响应格式

### 2. 错误处理
- 统一的错误响应格式
- 适当的HTTP状态码
- 详细的错误信息

### 3. 性能优化
- 合理设置连接池大小
- 使用异步处理
- 避免阻塞操作

### 4. 安全考虑
- 输入验证和清理
- 权限检查
- 限制请求大小

## 故障排除

### 常见问题

#### 1. 连接被拒绝
- 检查端口是否被占用
- 确认防火墙设置
- 验证服务器是否启动

#### 2. JSON解析失败
- 检查JSON格式
- 验证字符编码
- 确认请求头设置

#### 3. 路由不匹配
- 检查路径格式
- 确认HTTP方法
- 验证路由注册顺序

### 调试技巧

#### 1. 启用详细日志
```c
// 设置日志级别
logger_set_level(LOG_LEVEL_DEBUG);
```

#### 2. 检查路由注册
```c
// 打印所有路由
printf("已注册的路由数量: %d\n", get_route_count());
```

#### 3. 监控连接状态
```c
// 获取当前连接数
printf("当前连接数: %d\n", get_active_connections());
```

## 版本历史

### v1.0.0
- 初始版本
- 基本HTTP服务器功能
- 路由系统
- JSON解析器
- 模块化架构

## 许可证

本项目采用MIT许可证，详见LICENSE文件。

## 贡献

欢迎提交问题报告和功能请求。如果您想贡献代码，请：

1. Fork项目
2. 创建功能分支
3. 提交更改
4. 推送到分支
5. 创建Pull Request

## 联系方式

如有问题或建议，请通过以下方式联系：

- 项目Issues页面
- 邮件：[your-email@example.com]
- 项目主页：[project-url]
