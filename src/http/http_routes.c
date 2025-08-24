#include "src/http/http_module.h"
#include "src/json/json_parser_module.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 示例：用户数据结构
typedef struct {
    int id;
    char name[64];
    char email[128];
    int age;
} user_t;

// 模拟用户数据库
static user_t users[] = {
    {1, "张三", "zhangsan@example.com", 25},
    {2, "李四", "lisi@example.com", 30},
    {3, "王五", "wangwu@example.com", 28}
};
static int user_count = 3;

// GET /api/users - 获取所有用户
int handle_get_users(const http_request_t *request, http_response_t *response, void *user_data) {
    (void)request;
    (void)user_data;
    
    printf("处理GET /api/users请求\n");
    
    // 创建JSON响应
    json_value_t *users_array = json_create_array();
    if (!users_array) {
        return http_send_error_response(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, "内存分配失败");
    }
    
    // 添加用户到数组
    for (int i = 0; i < user_count; i++) {
        json_value_t *user_obj = json_create_object();
        if (!user_obj) continue;
        
        json_object_set(user_obj, "id", json_create_number(users[i].id));
        json_object_set(user_obj, "name", json_create_string(users[i].name));
        json_object_set(user_obj, "email", json_create_string(users[i].email));
        json_object_set(user_obj, "age", json_create_number(users[i].age));
        
        json_array_add(users_array, user_obj);
        json_free(user_obj);
    }
    
    // 转换为JSON字符串
    char *json_string = json_stringify(users_array);
    if (!json_string) {
        json_free(users_array);
        return http_send_error_response(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, "JSON生成失败");
    }
    
    // 发送响应
    int result = http_send_ok_response(response, json_string);
    
    // 清理
    free(json_string);
    json_free(users_array);
    
    return result;
}

// GET /api/users/{id} - 获取指定用户
int handle_get_user(const http_request_t *request, http_response_t *response, void *user_data) {
    (void)user_data;
    
    printf("处理GET /api/users/{id}请求，路径: %s\n", request->path);
    
    // 解析用户ID（简化处理，假设路径格式为 /api/users/{id}）
    const char *path = request->path;
    if (strncmp(path, "/api/users/", 11) != 0) {
        return http_send_bad_request_response(response, "无效的用户路径");
    }
    
    int user_id = atoi(path + 11);
    if (user_id <= 0) {
        return http_send_bad_request_response(response, "无效的用户ID");
    }
    
    // 查找用户
    user_t *user = NULL;
    for (int i = 0; i < user_count; i++) {
        if (users[i].id == user_id) {
            user = &users[i];
            break;
        }
    }
    
    if (!user) {
        return http_send_error_response(response, HTTP_STATUS_NOT_FOUND, "用户不存在");
    }
    
    // 创建用户JSON对象
    json_value_t *user_obj = json_create_object();
    if (!user_obj) {
        return http_send_error_response(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, "内存分配失败");
    }
    
    json_object_set(user_obj, "id", json_create_number(user->id));
    json_object_set(user_obj, "name", json_create_string(user->name));
    json_object_set(user_obj, "email", json_create_string(user->email));
    json_object_set(user_obj, "age", json_create_number(user->age));
    
    // 转换为JSON字符串
    char *json_string = json_stringify(user_obj);
    if (!json_string) {
        json_free(user_obj);
        return http_send_error_response(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, "JSON生成失败");
    }
    
    // 发送响应
    int result = http_send_ok_response(response, json_string);
    
    // 清理
    free(json_string);
    json_free(user_obj);
    
    return result;
}

// POST /api/users - 创建新用户
int handle_create_user(const http_request_t *request, http_response_t *response, void *user_data) {
    (void)user_data;
    
    printf("处理POST /api/users请求\n");
    
    if (!request->body || request->body_length == 0) {
        return http_send_bad_request_response(response, "请求体不能为空");
    }
    
    // 解析JSON请求体
    json_value_t *user_data_json;
    if (json_parse(request->body, request->body_length, &user_data_json) != 0) {
        return http_send_bad_request_response(response, "无效的JSON格式");
    }
    
    if (!json_is_object(user_data_json)) {
        json_free(user_data_json);
        return http_send_bad_request_response(response, "请求体必须是JSON对象");
    }
    
    // 提取用户信息
    json_value_t *name_value = json_object_get(user_data_json, "name");
    json_value_t *email_value = json_object_get(user_data_json, "email");
    json_value_t *age_value = json_object_get(user_data_json, "age");
    
    if (!name_value || !email_value || !age_value ||
        !json_is_string(name_value) || !json_is_string(email_value) || !json_is_number(age_value)) {
        json_free(user_data_json);
        return http_send_bad_request_response(response, "缺少必需的字段：name, email, age");
    }
    
    const char *name = json_get_string(name_value);
    const char *email = json_get_string(email_value);
    double age = json_get_number(age_value);
    
    // 验证数据
    if (strlen(name) == 0 || strlen(email) == 0 || age < 0 || age > 150) {
        json_free(user_data_json);
        return http_send_bad_request_response(response, "数据验证失败");
    }
    
    // 创建新用户（简化实现，实际应该保存到数据库）
    int new_id = user_count + 1;
    
    // 创建响应JSON
    json_value_t *response_obj = json_create_object();
    if (!response_obj) {
        json_free(user_data_json);
        return http_send_error_response(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, "内存分配失败");
    }
    
    json_object_set(response_obj, "id", json_create_number(new_id));
    json_object_set(response_obj, "name", json_create_string(name));
    json_object_set(response_obj, "email", json_create_string(email));
    json_object_set(response_obj, "age", json_create_number(age));
    json_object_set(response_obj, "message", json_create_string("用户创建成功"));
    
    // 转换为JSON字符串
    char *json_string = json_stringify(response_obj);
    if (!json_string) {
        json_free(response_obj);
        json_free(user_data_json);
        return http_send_error_response(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, "JSON生成失败");
    }
    
    // 发送响应
    int result = http_create_json_response(response, HTTP_STATUS_CREATED, json_string);
    
    // 清理
    free(json_string);
    json_free(response_obj);
    json_free(user_data_json);
    
    return result;
}

// PUT /api/users/{id} - 更新用户
int handle_update_user(const http_request_t *request, http_response_t *response, void *user_data) {
    (void)user_data;
    
    printf("处理PUT /api/users/{id}请求，路径: %s\n", request->path);
    
    // 解析用户ID
    const char *path = request->path;
    if (strncmp(path, "/api/users/", 11) != 0) {
        return http_send_bad_request_response(response, "无效的用户路径");
    }
    
    int user_id = atoi(path + 11);
    if (user_id <= 0) {
        return http_send_bad_request_response(response, "无效的用户ID");
    }
    
    if (!request->body || request->body_length == 0) {
        return http_send_bad_request_response(response, "请求体不能为空");
    }
    
    // 解析JSON请求体
    json_value_t *user_data_json;
    if (json_parse(request->body, request->body_length, &user_data_json) != 0) {
        return http_send_bad_request_response(response, "无效的JSON格式");
    }
    
    if (!json_is_object(user_data_json)) {
        json_free(user_data_json);
        return http_send_bad_request_response(response, "请求体必须是JSON对象");
    }
    
    // 查找用户
    user_t *user = NULL;
    for (int i = 0; i < user_count; i++) {
        if (users[i].id == user_id) {
            user = &users[i];
            break;
        }
    }
    
    if (!user) {
        json_free(user_data_json);
        return http_send_error_response(response, HTTP_STATUS_NOT_FOUND, "用户不存在");
    }
    
    // 更新用户信息（简化实现）
    json_value_t *name_value = json_object_get(user_data_json, "name");
    json_value_t *email_value = json_object_get(user_data_json, "email");
    json_value_t *age_value = json_object_get(user_data_json, "age");
    
    if (name_value && json_is_string(name_value)) {
        strncpy(user->name, json_get_string(name_value), sizeof(user->name) - 1);
        user->name[sizeof(user->name) - 1] = '\0';
    }
    
    if (email_value && json_is_string(email_value)) {
        strncpy(user->email, json_get_string(email_value), sizeof(user->email) - 1);
        user->email[sizeof(user->email) - 1] = '\0';
    }
    
    if (age_value && json_is_number(age_value)) {
        double age = json_get_number(age_value);
        if (age >= 0 && age <= 150) {
            user->age = (int)age;
        }
    }
    
    // 创建响应JSON
    json_value_t *response_obj = json_create_object();
    if (!response_obj) {
        json_free(user_data_json);
        return http_send_error_response(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, "内存分配失败");
    }
    
    json_object_set(response_obj, "id", json_create_number(user->id));
    json_object_set(response_obj, "name", json_create_string(user->name));
    json_object_set(response_obj, "email", json_create_string(user->email));
    json_object_set(response_obj, "age", json_create_number(user->age));
    json_object_set(response_obj, "message", json_create_string("用户更新成功"));
    
    // 转换为JSON字符串
    char *json_string = json_stringify(response_obj);
    if (!json_string) {
        json_free(response_obj);
        json_free(user_data_json);
        return http_send_error_response(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, "JSON生成失败");
    }
    
    // 发送响应
    int result = http_send_ok_response(response, json_string);
    
    // 清理
    free(json_string);
    json_free(response_obj);
    json_free(user_data_json);
    
    return result;
}

// DELETE /api/users/{id} - 删除用户
int handle_delete_user(const http_request_t *request, http_response_t *response, void *user_data) {
    (void)user_data;
    
    printf("处理DELETE /api/users/{id}请求，路径: %s\n", request->path);
    
    // 解析用户ID
    const char *path = request->path;
    if (strncmp(path, "/api/users/", 11) != 0) {
        return http_send_bad_request_response(response, "无效的用户路径");
    }
    
    int user_id = atoi(path + 11);
    if (user_id <= 0) {
        return http_send_bad_request_response(response, "无效的用户ID");
    }
    
    // 查找用户
    int user_index = -1;
    for (int i = 0; i < user_count; i++) {
        if (users[i].id == user_id) {
            user_index = i;
            break;
        }
    }
    
    if (user_index == -1) {
        return http_send_error_response(response, HTTP_STATUS_NOT_FOUND, "用户不存在");
    }
    
    // 删除用户（简化实现，实际应该从数据库删除）
    for (int i = user_index; i < user_count - 1; i++) {
        users[i] = users[i + 1];
    }
    user_count--;
    
    // 创建响应JSON
    json_value_t *response_obj = json_create_object();
    if (!response_obj) {
        return http_send_error_response(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, "内存分配失败");
    }
    
    json_object_set(response_obj, "message", json_create_string("用户删除成功"));
    json_object_set(response_obj, "deleted_id", json_create_number(user_id));
    
    // 转换为JSON字符串
    char *json_string = json_stringify(response_obj);
    if (!json_string) {
        json_free(response_obj);
        return http_send_error_response(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, "JSON生成失败");
    }
    
    // 发送响应
    int result = http_send_ok_response(response, json_string);
    
    // 清理
    free(json_string);
    json_free(response_obj);
    
    return result;
}

// GET /api/health - 健康检查
int handle_health_check(const http_request_t *request, http_response_t *response, void *user_data) {
    (void)request;
    (void)user_data;
    
    printf("处理GET /api/health请求\n");
    
    // 创建健康状态JSON
    json_value_t *health_obj = json_create_object();
    if (!health_obj) {
        return http_send_error_response(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, "内存分配失败");
    }
    
    json_object_set(health_obj, "status", json_create_string("healthy"));
    json_object_set(health_obj, "timestamp", json_create_string("2024-01-01T00:00:00Z"));
    json_object_set(health_obj, "version", json_create_string("1.0.0"));
    
    // 转换为JSON字符串
    char *json_string = json_stringify(health_obj);
    if (!json_string) {
        json_free(health_obj);
        return http_send_error_response(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, "JSON生成失败");
    }
    
    // 发送响应
    int result = http_send_ok_response(response, json_string);
    
    // 清理
    free(json_string);
    json_free(health_obj);
    
    return result;
}

// 注册所有HTTP路由
void register_http_routes(void) {
    printf("注册HTTP路由...\n");
    
    // 用户管理API
    http_add_route(HTTP_METHOD_GET, "/api/users", handle_get_users, NULL);
    http_add_route(HTTP_METHOD_GET, "/api/users/1", handle_get_user, NULL);
    http_add_route(HTTP_METHOD_GET, "/api/users/2", handle_get_user, NULL);
    http_add_route(HTTP_METHOD_GET, "/api/users/3", handle_get_user, NULL);
    http_add_route(HTTP_METHOD_POST, "/api/users", handle_create_user, NULL);
    http_add_route(HTTP_METHOD_PUT, "/api/users/1", handle_update_user, NULL);
    http_add_route(HTTP_METHOD_PUT, "/api/users/2", handle_update_user, NULL);
    http_add_route(HTTP_METHOD_PUT, "/api/users/3", handle_update_user, NULL);
    http_add_route(HTTP_METHOD_DELETE, "/api/users/1", handle_delete_user, NULL);
    http_add_route(HTTP_METHOD_DELETE, "/api/users/2", handle_delete_user, NULL);
    http_add_route(HTTP_METHOD_DELETE, "/api/users/3", handle_delete_user, NULL);
    
    // 系统API
    http_add_route(HTTP_METHOD_GET, "/api/health", handle_health_check, NULL);
    
    printf("HTTP路由注册完成\n");
}
