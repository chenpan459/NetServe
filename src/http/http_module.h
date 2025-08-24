#ifndef HTTP_MODULE_H
#define HTTP_MODULE_H

#include "src/modules/module_manager.h"
#include <uv.h>
#include <stddef.h>

// HTTP方法枚举
typedef enum {
    HTTP_METHOD_GET,
    HTTP_METHOD_POST,
    HTTP_METHOD_PUT,
    HTTP_METHOD_DELETE,
    HTTP_METHOD_PATCH,
    HTTP_METHOD_HEAD,
    HTTP_METHOD_OPTIONS,
    HTTP_METHOD_UNKNOWN
} http_method_t;

// HTTP状态码
typedef enum {
    HTTP_STATUS_OK = 200,
    HTTP_STATUS_CREATED = 201,
    HTTP_STATUS_NO_CONTENT = 204,
    HTTP_STATUS_BAD_REQUEST = 400,
    HTTP_STATUS_UNAUTHORIZED = 401,
    HTTP_STATUS_FORBIDDEN = 403,
    HTTP_STATUS_NOT_FOUND = 404,
    HTTP_STATUS_METHOD_NOT_ALLOWED = 405,
    HTTP_STATUS_INTERNAL_SERVER_ERROR = 500,
    HTTP_STATUS_NOT_IMPLEMENTED = 501,
    HTTP_STATUS_SERVICE_UNAVAILABLE = 503
} http_status_t;

// HTTP请求结构
typedef struct http_request {
    http_method_t method;
    char *path;
    char *query_string;
    char *body;
    size_t body_length;
    char *content_type;
    char *user_agent;
    char *authorization;
    struct http_header *headers;
    int header_count;
} http_request_t;

// HTTP响应结构
typedef struct http_response {
    http_status_t status;
    char *content_type;
    char *body;
    size_t body_length;
    struct http_header *headers;
    int header_count;
} http_response_t;

// HTTP头部结构
typedef struct http_header {
    char *name;
    char *value;
} http_header_t;

// JSON解析回调函数类型
typedef int (*json_parser_callback_t)(const char *json_data, size_t data_length, void *user_data);

// HTTP路由处理函数类型
typedef int (*http_route_handler_t)(const http_request_t *request, http_response_t *response, void *user_data);

// HTTP模块配置
typedef struct {
    int port;
    char *host;
    int max_connections;
    int request_timeout_ms;
    int enable_cors;
    char *cors_origin;
    int enable_logging;
    int enable_json_parsing;
} http_config_t;

// HTTP路由项
typedef struct http_route {
    http_method_t method;
    char *path;
    http_route_handler_t handler;
    void *user_data;
    struct http_route *next;
} http_route_t;

// HTTP模块私有数据
typedef struct {
    http_config_t config;
    uv_tcp_t server;
    http_route_t *routes;
    int route_count;
    uv_mutex_t routes_mutex;
    json_parser_callback_t json_parser;
    void *json_parser_user_data;
} http_private_data_t;

// HTTP模块接口
extern module_interface_t http_module;

// 模块生命周期函数
int http_module_init(module_interface_t *self, uv_loop_t *loop);
int http_module_start(module_interface_t *self);
int http_module_stop(module_interface_t *self);
int http_module_cleanup(module_interface_t *self);

// 路由管理函数
int http_add_route(http_method_t method, const char *path, http_route_handler_t handler, void *user_data);
int http_remove_route(http_method_t method, const char *path);
void http_clear_routes(void);

// JSON处理函数
int http_set_json_parser(json_parser_callback_t parser, void *user_data);
int http_parse_json_request(const http_request_t *request, void **parsed_data);
int http_create_json_response(http_response_t *response, http_status_t status, const char *json_data);

// 工具函数
const char* http_method_to_string(http_method_t method);
http_method_t http_string_to_method(const char *method_str);
const char* http_status_to_string(http_status_t status);
int http_add_header(http_response_t *response, const char *name, const char *value);
int http_get_header(const http_request_t *request, const char *name, char **value);

// 预定义响应函数
int http_send_ok_response(http_response_t *response, const char *json_data);
int http_send_error_response(http_response_t *response, http_status_t status, const char *message);
int http_send_not_found_response(http_response_t *response);
int http_send_bad_request_response(http_response_t *response, const char *message);

#endif // HTTP_MODULE_H
