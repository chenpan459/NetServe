# JSON解析器模块文档

## 概述

JSON解析器模块提供了完整的JSON数据处理功能，包括解析、生成、验证和操作。该模块专为高性能和内存效率而设计，支持所有标准JSON数据类型，并提供丰富的API用于JSON操作。

## 主要特性

### 1. 完整的JSON支持
- 支持所有JSON数据类型：null, boolean, number, string, array, object
- 符合RFC 7159标准
- 支持嵌套结构和复杂数据类型
- 处理Unicode字符串和转义字符

### 2. 高性能解析
- 递归下降解析器
- 内存高效的树形结构
- 支持流式解析
- 错误恢复机制

### 3. 灵活的API
- 类型安全的操作函数
- 链式调用支持
- 内存管理自动化
- 错误处理机制

### 4. 内存管理
- 自动内存分配和释放
- 内存泄漏检测
- 可配置的内存限制
- 内存池集成

## 架构设计

### 数据结构

#### JSON值类型
```c
typedef enum {
    JSON_TYPE_NULL,      // null值
    JSON_TYPE_BOOL,      // 布尔值
    JSON_TYPE_NUMBER,    // 数字（整数或浮点数）
    JSON_TYPE_STRING,    // 字符串
    JSON_TYPE_ARRAY,     // 数组
    JSON_TYPE_OBJECT     // 对象
} json_type_t;
```

#### JSON值结构
```c
typedef struct json_value {
    json_type_t type;    // 值类型
    union {
        int bool_val;    // 布尔值
        double number;   // 数字值
        char *string;    // 字符串值
        json_array_t *array;    // 数组值
        json_object_t *object;  // 对象值
    } data;
} json_value_t;
```

#### JSON数组结构
```c
typedef struct json_array {
    json_value_t **values;   // 值数组
    size_t size;             // 当前大小
    size_t capacity;         // 容量
} json_array_t;
```

#### JSON对象结构
```c
typedef struct json_object {
    json_pair_t **pairs;     // 键值对数组
    size_t size;             // 当前大小
    size_t capacity;         // 容量
} json_object_t;
```

#### JSON键值对结构
```c
typedef struct json_pair {
    char *key;               // 键名
    json_value_t *value;     // 值
} json_pair_t;
```

### 解析器状态
```c
typedef struct json_parser_state {
    const char *input;       // 输入字符串
    size_t length;           // 输入长度
    size_t position;         // 当前位置
    int line;                // 当前行号
    int column;              // 当前列号
    char *error_message;     // 错误信息
} json_parser_state_t;
```

## API参考

### 核心解析函数

#### `json_parse`
```c
int json_parse(const char *json_string, size_t length, json_value_t **result);
```
解析JSON字符串。

**参数：**
- `json_string`: JSON字符串
- `length`: 字符串长度
- `result`: 解析结果指针

**返回值：**
- 0: 成功
- -1: 失败

**示例：**
```c
const char *json_str = "{\"name\":\"John\",\"age\":30}";
json_value_t *value;
if (json_parse(json_str, strlen(json_str), &value) == 0) {
    // 处理解析结果
    json_free(value);
}
```

#### `json_parse_file`
```c
int json_parse_file(const char *filename, json_value_t **result);
```
从文件解析JSON。

#### `json_stringify`
```c
char *json_stringify(const json_value_t *value);
```
将JSON值转换为字符串。

#### `json_write_file`
```c
int json_write_file(const char *filename, const json_value_t *value);
```
将JSON值写入文件。

### 值创建函数

#### 基本类型创建
```c
json_value_t *json_create_null(void);
json_value_t *json_create_bool(int value);
json_value_t *json_create_number(double value);
json_value_t *json_create_string(const char *value);
```

#### 复合类型创建
```c
json_value_t *json_create_array(void);
json_value_t *json_create_object(void);
```

**示例：**
```c
// 创建JSON对象
json_value_t *user = json_create_object();
json_object_set(user, "name", json_create_string("John"));
json_object_set(user, "age", json_create_number(30));

// 创建JSON数组
json_value_t *hobbies = json_create_array();
json_array_add(hobbies, json_create_string("reading"));
json_array_add(hobbies, json_create_string("swimming"));
json_object_set(user, "hobbies", hobbies);

// 转换为字符串
char *json_str = json_stringify(user);
printf("JSON: %s\n", json_str);

// 清理
free(json_str);
json_free(user);
```

### 数组操作函数

#### 基本操作
```c
int json_array_add(json_array_t *array, json_value_t *value);
json_value_t *json_array_get(const json_array_t *array, size_t index);
int json_array_set(json_array_t *array, size_t index, json_value_t *value);
size_t json_array_size(const json_array_t *array);
```

#### 高级操作
```c
int json_array_insert(json_array_t *array, size_t index, json_value_t *value);
int json_array_remove(json_array_t *array, size_t index);
int json_array_clear(json_array_t *array);
```

**示例：**
```c
json_value_t *array = json_create_array();

// 添加元素
json_array_add(array, json_create_string("first"));
json_array_add(array, json_create_number(42));
json_array_add(array, json_create_bool(1));

// 访问元素
json_value_t *first = json_array_get(array, 0);
if (json_is_string(first)) {
    printf("First element: %s\n", json_get_string(first));
}

// 修改元素
json_array_set(array, 1, json_create_string("modified"));

// 获取大小
printf("Array size: %zu\n", json_array_size(array));
```

### 对象操作函数

#### 基本操作
```c
int json_object_set(json_object_t *object, const char *key, json_value_t *value);
json_value_t *json_object_get(const json_object_t *object, const char *key);
int json_object_has(const json_object_t *object, const char *key);
int json_object_remove(json_object_t *object, const char *key);
size_t json_object_size(const json_object_t *object);
```

#### 高级操作
```c
int json_object_merge(json_object_t *target, const json_object_t *source);
char **json_object_keys(const json_object_t *object);
int json_object_clear(json_object_t *object);
```

**示例：**
```c
json_value_t *config = json_create_object();

// 设置配置项
json_object_set(config, "server", json_create_string("localhost"));
json_object_set(config, "port", json_create_number(8080));
json_object_set(config, "debug", json_create_bool(1));

// 检查配置项
if (json_object_has(config, "server")) {
    json_value_t *server = json_object_get(config, "server");
    printf("Server: %s\n", json_get_string(server));
}

// 获取所有键
char **keys = json_object_keys(config);
for (size_t i = 0; i < json_object_size(config); i++) {
    printf("Key: %s\n", keys[i]);
}
free(keys);
```

### 类型检查函数

#### 类型判断
```c
int json_is_null(const json_value_t *value);
int json_is_bool(const json_value_t *value);
int json_is_number(const json_value_t *value);
int json_is_string(const json_value_t *value);
int json_is_array(const json_value_t *value);
int json_is_object(const json_value_t *value);
```

#### 类型获取
```c
json_type_t json_get_type(const json_value_t *value);
```

### 值获取函数

#### 安全获取
```c
int json_get_bool(const json_value_t *value);
double json_get_number(const json_value_t *value);
const char *json_get_string(const json_value_t *value);
```

**示例：**
```c
json_value_t *value = json_object_get(user, "age");
if (json_is_number(value)) {
    double age = json_get_number(value);
    printf("Age: %.0f\n", age);
} else {
    printf("Age is not a number\n");
}
```

### 内存管理函数

#### 释放函数
```c
void json_free(json_value_t *value);
void json_free_array(json_array_t *array);
void json_free_object(json_object_t *object);
```

#### 内存统计
```c
size_t json_get_memory_usage(const json_value_t *value);
void json_print_memory_stats(void);
```

### 验证和工具函数

#### 验证函数
```c
int json_validate(const char *json_string, size_t length);
int json_is_valid(const char *json_string, size_t length);
```

#### 工具函数
```c
json_value_t *json_clone(const json_value_t *value);
int json_merge(json_value_t *target, const json_value_t *source);
void json_pretty_print(const json_value_t *value, int indent);
```

## 使用示例

### 1. 基本JSON解析

```c
#include "modules/json_parser_module.h"

void parse_simple_json() {
    const char *json_str = "{\"name\":\"Alice\",\"age\":25,\"active\":true}";
    
    json_value_t *value;
    if (json_parse(json_str, strlen(json_str), &value) == 0) {
        if (json_is_object(value)) {
            json_object_t *obj = value->data.object;
            
            // 获取姓名
            json_value_t *name = json_object_get(obj, "name");
            if (json_is_string(name)) {
                printf("Name: %s\n", json_get_string(name));
            }
            
            // 获取年龄
            json_value_t *age = json_object_get(obj, "age");
            if (json_is_number(age)) {
                printf("Age: %.0f\n", json_get_number(age));
            }
            
            // 获取状态
            json_value_t *active = json_object_get(obj, "active");
            if (json_is_bool(active)) {
                printf("Active: %s\n", json_get_bool(active) ? "true" : "false");
            }
        }
        
        json_free(value);
    } else {
        printf("JSON解析失败: %s\n", json_get_last_error());
    }
}
```

### 2. 创建复杂JSON结构

```c
void create_complex_json() {
    // 创建用户对象
    json_value_t *user = json_create_object();
    
    // 基本信息
    json_object_set(user, "id", json_create_number(1));
    json_object_set(user, "username", json_create_string("john_doe"));
    json_object_set(user, "email", json_create_string("john@example.com"));
    json_object_set(user, "active", json_create_bool(1));
    
    // 地址信息
    json_value_t *address = json_create_object();
    json_object_set(address, "street", json_create_string("123 Main St"));
    json_object_set(address, "city", json_create_string("New York"));
    json_object_set(address, "zip", json_create_string("10001"));
    json_object_set(user, "address", address);
    
    // 标签数组
    json_value_t *tags = json_create_array();
    json_array_add(tags, json_create_string("developer"));
    json_array_add(tags, json_create_string("admin"));
    json_array_add(tags, json_create_string("user"));
    json_object_set(user, "tags", tags);
    
    // 转换为字符串并打印
    char *json_str = json_stringify(user);
    printf("Created JSON:\n%s\n", json_str);
    
    // 清理
    free(json_str);
    json_free(user);
}
```

### 3. JSON文件操作

```c
void json_file_operations() {
    // 创建配置对象
    json_value_t *config = json_create_object();
    
    // 数据库配置
    json_value_t *database = json_create_object();
    json_object_set(database, "host", json_create_string("localhost"));
    json_object_set(database, "port", json_create_number(5432));
    json_object_set(database, "name", json_create_string("mydb"));
    json_object_set(database, "user", json_create_string("admin"));
    json_object_set(database, "password", json_create_string("secret"));
    json_object_set(config, "database", database);
    
    // 服务器配置
    json_value_t *server = json_create_object();
    json_object_set(server, "host", json_create_string("0.0.0.0"));
    json_object_set(server, "port", json_create_number(8080));
    json_object_set(server, "workers", json_create_number(4));
    json_object_set(config, "server", server);
    
    // 写入文件
    if (json_write_file("config.json", config) == 0) {
        printf("配置已写入文件\n");
    }
    
    // 从文件读取
    json_value_t *loaded_config;
    if (json_parse_file("config.json", &loaded_config) == 0) {
        printf("配置已从文件加载\n");
        
        // 验证加载的配置
        json_value_t *db_host = json_object_get(
            json_object_get(loaded_config, "database"), "host");
        if (json_is_string(db_host)) {
            printf("Database host: %s\n", json_get_string(db_host));
        }
        
        json_free(loaded_config);
    }
    
    json_free(config);
}
```

### 4. 错误处理和验证

```c
void json_error_handling() {
    // 测试有效JSON
    const char *valid_json = "{\"valid\": true}";
    if (json_is_valid(valid_json, strlen(valid_json))) {
        printf("JSON格式有效\n");
    }
    
    // 测试无效JSON
    const char *invalid_json = "{\"invalid\": true,"; // 缺少闭合括号
    if (!json_is_valid(invalid_json, strlen(invalid_json))) {
        printf("JSON格式无效\n");
    }
    
    // 尝试解析无效JSON
    json_value_t *value;
    if (json_parse(invalid_json, strlen(invalid_json), &value) != 0) {
        printf("解析错误: %s\n", json_get_last_error());
    }
    
    // 测试类型安全
    json_value_t *obj = json_create_object();
    json_object_set(obj, "number", json_create_string("not_a_number"));
    
    json_value_t *number_val = json_object_get(obj, "number");
    if (json_is_string(number_val)) {
        printf("值类型是字符串: %s\n", json_get_string(number_val));
    }
    
    // 尝试获取数字值（会失败）
    if (json_is_number(number_val)) {
        double num = json_get_number(number_val);
        printf("数字值: %f\n", num);
    } else {
        printf("值不是数字类型\n");
    }
    
    json_free(obj);
}
```

### 5. 高级操作

```c
void json_advanced_operations() {
    // 创建源对象
    json_value_t *source = json_create_object();
    json_object_set(source, "name", json_create_string("Source"));
    json_object_set(source, "value", json_create_number(100));
    
    // 创建目标对象
    json_value_t *target = json_create_object();
    json_object_set(target, "name", json_create_string("Target"));
    json_object_set(target, "count", json_create_number(5));
    
    // 合并对象
    json_merge(target, source);
    
    // 克隆对象
    json_value_t *cloned = json_clone(target);
    
    // 修改克隆对象
    json_object_set(cloned, "cloned", json_create_bool(1));
    
    // 打印所有对象
    printf("Source:\n");
    json_pretty_print(source, 2);
    
    printf("\nTarget (merged):\n");
    json_pretty_print(target, 2);
    
    printf("\nCloned:\n");
    json_pretty_print(cloned, 2);
    
    // 清理
    json_free(source);
    json_free(target);
    json_free(cloned);
}
```

## 配置选项

### 解析器配置
```c
json_parser_config_t config = {
    .enable_unicode_escape = 1,    // 启用Unicode转义
    .enable_comments = 0,          // 禁用注释
    .strict_mode = 1,              // 严格模式
    .max_depth = 100,              // 最大嵌套深度
    .max_string_length = 1024 * 1024,  // 最大字符串长度（1MB）
    .enable_float_precision = 1,   // 启用浮点数精度
    .max_array_size = 1000000,     // 最大数组大小
    .max_object_size = 1000000     // 最大对象大小
};
```

## 性能优化

### 1. 内存管理
- 使用内存池减少分配开销
- 预分配数组和对象容量
- 及时释放不需要的内存

### 2. 解析优化
- 流式解析支持
- 延迟解析
- 缓存常用值

### 3. 字符串处理
- 零拷贝字符串引用
- 字符串池
- 高效的字符串比较

## 错误处理

### 错误类型
- 语法错误
- 类型错误
- 内存错误
- 文件I/O错误

### 错误信息
```c
const char *error = json_get_last_error();
if (error) {
    printf("JSON错误: %s\n", error);
}
```

### 错误恢复
- 部分解析结果
- 错误位置信息
- 建议修复

## 最佳实践

### 1. 内存管理
- 始终调用`json_free`释放解析结果
- 检查内存分配返回值
- 使用内存统计监控使用情况

### 2. 类型安全
- 使用类型检查函数验证类型
- 安全地获取值
- 处理类型不匹配的情况

### 3. 错误处理
- 检查所有函数返回值
- 提供有意义的错误信息
- 实现错误恢复机制

### 4. 性能考虑
- 重用JSON对象
- 避免频繁的字符串操作
- 使用适当的初始容量

## 故障排除

### 常见问题

#### 1. 内存泄漏
- 确保调用`json_free`
- 检查循环引用
- 使用内存统计工具

#### 2. 解析失败
- 验证JSON格式
- 检查字符串编码
- 确认输入长度

#### 3. 类型错误
- 使用类型检查函数
- 验证数据结构
- 处理空值

### 调试技巧

#### 1. 启用详细日志
```c
// 设置日志级别
logger_set_level(LOG_LEVEL_DEBUG);
```

#### 2. 使用内存统计
```c
// 打印内存使用情况
json_print_memory_stats();
```

#### 3. 验证JSON格式
```c
// 在线JSON验证工具
// 使用json_validate函数
```

## 版本历史

### v1.0.0
- 初始版本
- 基本JSON解析功能
- 类型安全API
- 内存管理

### v1.1.0
- 性能优化
- 错误处理改进
- 内存池集成
- 流式解析支持

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
