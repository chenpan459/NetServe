#include "src/http/http_module.h"
#include "src/log/logger_module.h"
#include "src/http/http_routes.h"
#include "src/json/json_parser_module.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <uv.h>

// 默认配置
static http_config_t default_config = {
    .port = 8080,
    .host = "0.0.0.0",
    .max_connections = 1000,
    .request_timeout_ms = 30000,
    .enable_cors = 1,
    .cors_origin = "*",
    .enable_logging = 1,
    .enable_json_parsing = 1
};

// HTTP模块接口定义
module_interface_t http_module = {
    .name = "http",
    .version = "1.0.0",
    .init = http_module_init,
    .start = http_module_start,
    .stop = http_module_stop,
    .cleanup = http_module_cleanup,
    .state = MODULE_STATE_UNINITIALIZED,
    .private_data = NULL,
    .dependencies = NULL,
    .dependency_count = 0
};

// 全局HTTP模块数据
static http_private_data_t *global_http_data = NULL;

// 客户端连接结构
typedef struct http_client {
    uv_tcp_t tcp;
    uv_write_t write_req;
    char *read_buffer;
    size_t read_buffer_size;
    size_t read_buffer_used;
    http_request_t current_request;
    http_response_t current_response;
    struct http_client *next;
} http_client_t;

// 客户端连接池
static http_client_t *client_pool = NULL;
static int active_clients = 0;
static uv_mutex_t client_pool_mutex;

// 内部函数声明
static void on_new_connection(uv_stream_t *server, int status);
static void on_client_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);
static void on_client_write(uv_write_t *req, int status);
static void on_client_close(uv_handle_t *handle);
static void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf);
static int parse_http_request(const char *data, size_t length, http_request_t *request);
static int create_http_response(const http_request_t *request, http_response_t *response);
static void send_response(http_client_t *client, const http_response_t *response);
static http_route_t* find_matching_route(const http_request_t *request);
static int parse_url(const char *url, char **path, char **query_string);
static char* url_decode(const char *str);
static int add_header_to_response(http_response_t *response, const char *name, const char *value);

// HTTP模块初始化
int http_module_init(module_interface_t *self, uv_loop_t *loop) {
    (void)loop; // 避免未使用参数警告
    
    if (!self) {
        return -1;
    }
    
    // 分配私有数据
    http_private_data_t *data = malloc(sizeof(http_private_data_t));
    if (!data) {
        return -1;
    }
    
    // 初始化私有数据
    memset(data, 0, sizeof(http_private_data_t));
    data->config = default_config;
    data->routes = NULL;
    data->route_count = 0;
    data->json_parser = NULL;
    data->json_parser_user_data = NULL;
    
    // 初始化互斥锁
    if (uv_mutex_init(&data->routes_mutex) != 0) {
        free(data);
        return -1;
    }
    
    // 初始化客户端连接池互斥锁
    if (uv_mutex_init(&client_pool_mutex) != 0) {
        uv_mutex_destroy(&data->routes_mutex);
        free(data);
        return -1;
    }
    
    self->private_data = data;
    global_http_data = data;
    
    log_info("HTTP模块初始化成功");
    return 0;
}

// HTTP模块启动
int http_module_start(module_interface_t *self) {
    if (!self || !self->private_data) {
        return -1;
    }
    
    http_private_data_t *data = (http_private_data_t*) self->private_data;
    
    // 初始化TCP服务器
    uv_tcp_init(uv_default_loop(), &data->server);
    data->server.data = data;
    
    // 绑定地址
    struct sockaddr_in addr;
    uv_ip4_addr(data->config.host, data->config.port, &addr);
    
    int bind_result = uv_tcp_bind(&data->server, (const struct sockaddr*)&addr, 0);
    if (bind_result != 0) {
        log_error("HTTP服务器绑定地址失败: %s", uv_strerror(bind_result));
        return -1;
    }
    
    // 开始监听
    int listen_result = uv_listen((uv_stream_t*) &data->server, data->config.max_connections, on_new_connection);
    if (listen_result != 0) {
        log_error("HTTP服务器监听失败: %s", uv_strerror(listen_result));
        return -1;
    }
    
    log_info("HTTP模块启动成功，监听 %s:%d", data->config.host, data->config.port);
    return 0;
}

// HTTP模块停止
int http_module_stop(module_interface_t *self) {
    if (!self || !self->private_data) {
        return -1;
    }
    
    http_private_data_t *data = (http_private_data_t*) self->private_data;
    
    // 关闭服务器
    uv_close((uv_handle_t*) &data->server, NULL);
    
    log_info("HTTP模块已停止");
    return 0;
}

// HTTP模块清理
int http_module_cleanup(module_interface_t *self) {
    if (!self || !self->private_data) {
        return -1;
    }
    
    http_private_data_t *data = (http_private_data_t*) self->private_data;
    
    // 清理路由
    http_clear_routes();
    
    // 销毁互斥锁
    uv_mutex_destroy(&data->routes_mutex);
    uv_mutex_destroy(&client_pool_mutex);
    
    // 释放配置
    if (data->config.host != default_config.host) {
        free(data->config.host);
    }
    if (data->config.cors_origin != default_config.cors_origin) {
        free(data->config.cors_origin);
    }
    
    // 释放私有数据
    free(data);
    self->private_data = NULL;
    global_http_data = NULL;
    
    log_info("HTTP模块清理完成");
    return 0;
}

// 新连接回调
static void on_new_connection(uv_stream_t *server, int status) {
    if (status < 0) {
        log_error("新HTTP连接错误: %s", uv_strerror(status));
        return;
    }
    
    (void)server; // 避免未使用参数警告
    
    // 创建新的客户端连接
    http_client_t *client = malloc(sizeof(http_client_t));
    if (!client) {
        log_error("内存分配失败");
        return;
    }
    
    // 初始化客户端
    memset(client, 0, sizeof(http_client_t));
    uv_tcp_init(server->loop, &client->tcp);
    client->tcp.data = client;
    client->read_buffer_size = 4096;
    client->read_buffer = malloc(client->read_buffer_size);
    
    if (uv_accept(server, (uv_stream_t*) &client->tcp) == 0) {
        // 添加到连接池
        uv_mutex_lock(&client_pool_mutex);
        client->next = client_pool;
        client_pool = client;
        active_clients++;
        uv_mutex_unlock(&client_pool_mutex);
        
        // 开始读取数据
        uv_read_start((uv_stream_t*) &client->tcp, alloc_buffer, on_client_read);
        log_info("新HTTP客户端连接，当前连接数: %d", active_clients);
    } else {
        free(client->read_buffer);
        free(client);
    }
}

// 分配缓冲区回调
static void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    (void)handle; // 避免未使用参数警告
    buf->base = malloc(suggested_size);
    buf->len = suggested_size;
}

// 客户端读取回调
static void on_client_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    http_client_t *client = (http_client_t*) stream->data;
    
    if (nread > 0) {
        // 扩展读取缓冲区
        if (client->read_buffer_used + nread > client->read_buffer_size) {
            size_t new_size = client->read_buffer_size * 2;
            char *new_buffer = realloc(client->read_buffer, new_size);
            if (!new_buffer) {
                log_error("缓冲区扩展失败");
                uv_close((uv_handle_t*) stream, on_client_close);
                return;
            }
            client->read_buffer = new_buffer;
            client->read_buffer_size = new_size;
        }
        
        // 复制数据到缓冲区
        memcpy(client->read_buffer + client->read_buffer_used, buf->base, nread);
        client->read_buffer_used += nread;
        
        // 尝试解析HTTP请求
        http_request_t request;
        if (parse_http_request(client->read_buffer, client->read_buffer_used, &request) == 0) {
            // 创建响应
            http_response_t response;
            if (create_http_response(&request, &response) == 0) {
                send_response(client, &response);
            }
            
            // 清理请求数据
            if (request.path) free(request.path);
            if (request.query_string) free(request.query_string);
            if (request.body) free(request.body);
            if (request.content_type) free(request.content_type);
            if (request.user_agent) free(request.user_agent);
            if (request.authorization) free(request.authorization);
            if (request.headers) free(request.headers);
            
            // 重置读取缓冲区
            client->read_buffer_used = 0;
        }
        
        free(buf->base);
    } else if (nread < 0) {
        if (nread != UV_EOF) {
            log_error("HTTP读取错误: %s", uv_err_name(nread));
        }
        uv_close((uv_handle_t*) stream, on_client_close);
        free(buf->base);
    } else {
        free(buf->base);
    }
}

// 客户端写入回调
static void on_client_write(uv_write_t *req, int status) {
    if (status) {
        log_error("HTTP写入错误: %s", uv_strerror(status));
    }
    
    // 释放写入请求
    free(req);
}

// 客户端关闭回调
static void on_client_close(uv_handle_t *handle) {
    http_client_t *client = (http_client_t*) handle->data;
    
    // 从连接池移除
    uv_mutex_lock(&client_pool_mutex);
    if (client_pool == client) {
        client_pool = client->next;
    } else {
        http_client_t *prev = client_pool;
        while (prev && prev->next != client) {
            prev = prev->next;
        }
        if (prev) {
            prev->next = client->next;
        }
    }
    active_clients--;
    uv_mutex_unlock(&client_pool_mutex);
    
    // 释放资源
    if (client->read_buffer) {
        free(client->read_buffer);
    }
    free(client);
    
    log_info("HTTP客户端断开连接，当前连接数: %d", active_clients);
}

// 解析HTTP请求
static int parse_http_request(const char *data, size_t length, http_request_t *request) {
    if (!data || !request || length == 0) {
        return -1;
    }
    
    memset(request, 0, sizeof(http_request_t));
    
    // 查找请求行结束
    const char *line_end = strstr(data, "\r\n");
    if (!line_end) {
        return -1;
    }
    
    // 解析请求行
    char *request_line = strndup(data, line_end - data);
    char *method_str = strtok(request_line, " ");
    char *url = strtok(NULL, " ");
    char *version = strtok(NULL, " ");
    
    if (!method_str || !url || !version) {
        free(request_line);
        return -1;
    }
    
    // 设置HTTP方法
    request->method = http_string_to_method(method_str);
    
    // 解析URL
    if (parse_url(url, &request->path, &request->query_string) != 0) {
        free(request_line);
        return -1;
    }
    
    // 解析头部
    const char *headers_start = line_end + 2;
    const char *body_start = strstr(headers_start, "\r\n\r\n");
    
    if (body_start) {
        // 解析头部
        size_t headers_length = body_start - headers_start;
        char *headers_str = strndup(headers_start, headers_length);
        
        // 简单的头部解析（这里可以扩展）
        char *line = strtok(headers_str, "\r\n");
        while (line) {
            char *colon = strchr(line, ':');
            if (colon) {
                *colon = '\0';
                char *name = line;
                char *value = colon + 1;
                
                // 跳过前导空格
                while (*value == ' ') value++;
                
                if (strcasecmp(name, "Content-Type") == 0) {
                    request->content_type = strdup(value);
                } else if (strcasecmp(name, "User-Agent") == 0) {
                    request->user_agent = strdup(value);
                } else if (strcasecmp(name, "Authorization") == 0) {
                    request->authorization = strdup(value);
                }
            }
            line = strtok(NULL, "\r\n");
        }
        
        free(headers_str);
        
        // 设置请求体
        request->body = strdup(body_start + 4);
        request->body_length = length - (body_start + 4 - data);
    }
    
    free(request_line);
    return 0;
}

// 创建HTTP响应
static int create_http_response(const http_request_t *request, http_response_t *response) {
    if (!request || !response) {
        return -1;
    }
    
    memset(response, 0, sizeof(http_response_t));
    
    // 查找匹配的路由
    http_route_t *route = find_matching_route(request);
    if (route) {
        // 调用路由处理函数
        if (route->handler(request, response, route->user_data) != 0) {
            http_send_error_response(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, "Internal Server Error");
        }
    } else {
        // 没有找到匹配的路由
        http_send_not_found_response(response);
    }
    
    // 添加CORS头部
    if (global_http_data && global_http_data->config.enable_cors) {
        http_add_header(response, "Access-Control-Allow-Origin", global_http_data->config.cors_origin);
        http_add_header(response, "Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        http_add_header(response, "Access-Control-Allow-Headers", "Content-Type, Authorization");
    }
    
    return 0;
}

// 发送响应
static void send_response(http_client_t *client, const http_response_t *response) {
    if (!client || !response) {
        return;
    }
    
    // 构建HTTP响应
    char status_line[256];
    snprintf(status_line, sizeof(status_line), "HTTP/1.1 %d %s\r\n", 
             response->status, http_status_to_string(response->status));
    
    // 计算响应总长度
    size_t response_length = strlen(status_line);
    
    // 添加头部
    char headers_buffer[1024] = "";
    if (response->content_type) {
        char content_type_header[256];
        snprintf(content_type_header, sizeof(content_type_header), "Content-Type: %s\r\n", response->content_type);
        strcat(headers_buffer, content_type_header);
        response_length += strlen(content_type_header);
    }
    
    if (response->body) {
        char content_length_header[256];
        snprintf(content_length_header, sizeof(content_length_header), "Content-Length: %zu\r\n", response->body_length);
        strcat(headers_buffer, content_length_header);
        response_length += strlen(content_length_header);
    }
    
    // 添加自定义头部
    for (int i = 0; i < response->header_count; i++) {
        char custom_header[256];
        snprintf(custom_header, sizeof(custom_header), "%s: %s\r\n", 
                 response->headers[i].name, response->headers[i].value);
        strcat(headers_buffer, custom_header);
        response_length += strlen(custom_header);
    }
    
    // 添加头部结束标记
    strcat(headers_buffer, "\r\n");
    response_length += 2;
    
    // 添加响应体
    if (response->body) {
        response_length += response->body_length;
    }
    
    // 分配响应缓冲区
    char *response_buffer = malloc(response_length + 1);
    if (!response_buffer) {
        log_error("响应缓冲区分配失败");
        return;
    }
    
    // 组装响应
    char *ptr = response_buffer;
    strcpy(ptr, status_line);
    ptr += strlen(status_line);
    strcpy(ptr, headers_buffer);
    ptr += strlen(headers_buffer);
    
    if (response->body) {
        memcpy(ptr, response->body, response->body_length);
        ptr += response->body_length;
    }
    
    *ptr = '\0';
    
    // 发送响应
    uv_buf_t write_buf = uv_buf_init(response_buffer, response_length);
    uv_write_t *write_req = malloc(sizeof(uv_write_t));
    if (write_req) {
        write_req->data = client;
        uv_write(write_req, (uv_stream_t*) &client->tcp, &write_buf, 1, on_client_write);
    }
}

// 查找匹配的路由
static http_route_t* find_matching_route(const http_request_t *request) {
    if (!global_http_data || !request) {
        return NULL;
    }
    
    uv_mutex_lock(&global_http_data->routes_mutex);
    
    http_route_t *route = global_http_data->routes;
    while (route) {
        if (route->method == request->method && strcmp(route->path, request->path) == 0) {
            uv_mutex_unlock(&global_http_data->routes_mutex);
            return route;
        }
        route = route->next;
    }
    
    uv_mutex_unlock(&global_http_data->routes_mutex);
    return NULL;
}

// 解析URL
static int parse_url(const char *url, char **path, char **query_string) {
    if (!url) {
        return -1;
    }
    
    char *question_mark = strchr(url, '?');
    if (question_mark) {
        *path = strndup(url, question_mark - url);
        *query_string = strdup(question_mark + 1);
    } else {
        *path = strdup(url);
        *query_string = NULL;
    }
    
    // URL解码
    if (*path) {
        char *decoded_path = url_decode(*path);
        free(*path);
        *path = decoded_path;
    }
    
    if (*query_string) {
        char *decoded_query = url_decode(*query_string);
        free(*query_string);
        *query_string = decoded_query;
    }
    
    return 0;
}

// URL解码
static char* url_decode(const char *str) {
    if (!str) {
        return NULL;
    }
    
    char *result = malloc(strlen(str) + 1);
    if (!result) {
        return NULL;
    }
    
    char *dst = result;
    const char *src = str;
    
    while (*src) {
        if (*src == '%' && src[1] && src[2]) {
            char hex[3] = {src[1], src[2], 0};
            int value;
            if (sscanf(hex, "%x", &value) == 1) {
                *dst++ = (char)value;
                src += 3;
            } else {
                *dst++ = *src++;
            }
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    
    *dst = '\0';
    return result;
}

// 添加头部到响应
static int add_header_to_response(http_response_t *response, const char *name, const char *value) {
    if (!response || !name || !value) {
        return -1;
    }
    
    // 扩展头部数组
    http_header_t *new_headers = realloc(response->headers, 
                                        (response->header_count + 1) * sizeof(http_header_t));
    if (!new_headers) {
        return -1;
    }
    
    response->headers = new_headers;
    int index = response->header_count;
    
    response->headers[index].name = strdup(name);
    response->headers[index].value = strdup(value);
    response->header_count++;
    
    return 0;
}

// 公共API实现

// 添加路由
int http_add_route(http_method_t method, const char *path, http_route_handler_t handler, void *user_data) {
    if (!global_http_data || !path || !handler) {
        return -1;
    }
    
    http_route_t *route = malloc(sizeof(http_route_t));
    if (!route) {
        return -1;
    }
    
    route->method = method;
    route->path = strdup(path);
    route->handler = handler;
    route->user_data = user_data;
    
    uv_mutex_lock(&global_http_data->routes_mutex);
    
    route->next = global_http_data->routes;
    global_http_data->routes = route;
    global_http_data->route_count++;
    
    uv_mutex_unlock(&global_http_data->routes_mutex);
    
    log_info("添加HTTP路由: %s %s", http_method_to_string(method), path);
    return 0;
}

// 移除路由
int http_remove_route(http_method_t method, const char *path) {
    if (!global_http_data || !path) {
        return -1;
    }
    
    uv_mutex_lock(&global_http_data->routes_mutex);
    
    http_route_t *route = global_http_data->routes;
    http_route_t *prev = NULL;
    
    while (route) {
        if (route->method == method && strcmp(route->path, path) == 0) {
            if (prev) {
                prev->next = route->next;
            } else {
                global_http_data->routes = route->next;
            }
            
            free(route->path);
            free(route);
            global_http_data->route_count--;
            
            uv_mutex_unlock(&global_http_data->routes_mutex);
            log_info("移除HTTP路由: %s %s", http_method_to_string(method), path);
            return 0;
        }
        
        prev = route;
        route = route->next;
    }
    
    uv_mutex_unlock(&global_http_data->routes_mutex);
    return -1;
}

// 清理所有路由
void http_clear_routes(void) {
    if (!global_http_data) {
        return;
    }
    
    uv_mutex_lock(&global_http_data->routes_mutex);
    
    http_route_t *route = global_http_data->routes;
    while (route) {
        http_route_t *next = route->next;
        free(route->path);
        free(route);
        route = next;
    }
    
    global_http_data->routes = NULL;
    global_http_data->route_count = 0;
    
    uv_mutex_unlock(&global_http_data->routes_mutex);
    log_info("清理所有HTTP路由");
}

// 设置JSON解析器
int http_set_json_parser(json_parser_callback_t parser, void *user_data) {
    if (!global_http_data) {
        return -1;
    }
    
    global_http_data->json_parser = parser;
    global_http_data->json_parser_user_data = user_data;
    
    log_info("设置HTTP JSON解析器");
    return 0;
}

// 解析JSON请求
int http_parse_json_request(const http_request_t *request, void **parsed_data) {
    if (!global_http_data || !global_http_data->json_parser || !request || !parsed_data) {
        return -1;
    }
    
    if (request->body && request->body_length > 0) {
        return global_http_data->json_parser(request->body, request->body_length, 
                                           global_http_data->json_parser_user_data);
    }
    
    return -1;
}

// 创建JSON响应
int http_create_json_response(http_response_t *response, http_status_t status, const char *json_data) {
    if (!response || !json_data) {
        return -1;
    }
    
    response->status = status;
    response->content_type = strdup("application/json");
    response->body = strdup(json_data);
    response->body_length = strlen(json_data);
    
    return 0;
}

// 工具函数实现

const char* http_method_to_string(http_method_t method) {
    switch (method) {
        case HTTP_METHOD_GET: return "GET";
        case HTTP_METHOD_POST: return "POST";
        case HTTP_METHOD_PUT: return "PUT";
        case HTTP_METHOD_DELETE: return "DELETE";
        case HTTP_METHOD_PATCH: return "PATCH";
        case HTTP_METHOD_HEAD: return "HEAD";
        case HTTP_METHOD_OPTIONS: return "OPTIONS";
        default: return "UNKNOWN";
    }
}

http_method_t http_string_to_method(const char *method_str) {
    if (!method_str) {
        return HTTP_METHOD_UNKNOWN;
    }
    
    if (strcmp(method_str, "GET") == 0) return HTTP_METHOD_GET;
    if (strcmp(method_str, "POST") == 0) return HTTP_METHOD_POST;
    if (strcmp(method_str, "PUT") == 0) return HTTP_METHOD_PUT;
    if (strcmp(method_str, "DELETE") == 0) return HTTP_METHOD_DELETE;
    if (strcmp(method_str, "PATCH") == 0) return HTTP_METHOD_PATCH;
    if (strcmp(method_str, "HEAD") == 0) return HTTP_METHOD_HEAD;
    if (strcmp(method_str, "OPTIONS") == 0) return HTTP_METHOD_OPTIONS;
    
    return HTTP_METHOD_UNKNOWN;
}

const char* http_status_to_string(http_status_t status) {
    switch (status) {
        case HTTP_STATUS_OK: return "OK";
        case HTTP_STATUS_CREATED: return "Created";
        case HTTP_STATUS_NO_CONTENT: return "No Content";
        case HTTP_STATUS_BAD_REQUEST: return "Bad Request";
        case HTTP_STATUS_UNAUTHORIZED: return "Unauthorized";
        case HTTP_STATUS_FORBIDDEN: return "Forbidden";
        case HTTP_STATUS_NOT_FOUND: return "Not Found";
        case HTTP_STATUS_METHOD_NOT_ALLOWED: return "Method Not Allowed";
        case HTTP_STATUS_INTERNAL_SERVER_ERROR: return "Internal Server Error";
        case HTTP_STATUS_NOT_IMPLEMENTED: return "Not Implemented";
        case HTTP_STATUS_SERVICE_UNAVAILABLE: return "Service Unavailable";
        default: return "Unknown";
    }
}

int http_add_header(http_response_t *response, const char *name, const char *value) {
    return add_header_to_response(response, name, value);
}

int http_get_header(const http_request_t *request, const char *name, char **value) {
    if (!request || !name || !value) {
        return -1;
    }
    
    for (int i = 0; i < request->header_count; i++) {
        if (strcasecmp(request->headers[i].name, name) == 0) {
            *value = strdup(request->headers[i].value);
            return 0;
        }
    }
    
    return -1;
}

// 预定义响应函数实现

int http_send_ok_response(http_response_t *response, const char *json_data) {
    return http_create_json_response(response, HTTP_STATUS_OK, json_data);
}

int http_send_error_response(http_response_t *response, http_status_t status, const char *message) {
    if (!response || !message) {
        return -1;
    }
    
    char json_error[512];
    snprintf(json_error, sizeof(json_error), 
             "{\"error\": true, \"status\": %d, \"message\": \"%s\"}", status, message);
    
    return http_create_json_response(response, status, json_error);
}

int http_send_not_found_response(http_response_t *response) {
    return http_send_error_response(response, HTTP_STATUS_NOT_FOUND, "Not Found");
}

int http_send_bad_request_response(http_response_t *response, const char *message) {
    return http_send_error_response(response, HTTP_STATUS_BAD_REQUEST, message);
}
