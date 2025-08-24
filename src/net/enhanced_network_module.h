#ifndef ENHANCED_NETWORK_MODULE_H
#define ENHANCED_NETWORK_MODULE_H

#include "module_manager.h"
#include "threadpool_module.h"
#include <uv.h>

// 增强网络模块配置
typedef struct {
    int port;
    char *host;
    int backlog;
    int max_connections;
    int enable_threadpool;
    int max_concurrent_requests;
    int request_timeout_ms;
} enhanced_network_config_t;

// 请求处理上下文
typedef struct {
    uv_tcp_t *client;
    char *request_data;
    size_t request_size;
    void (*response_callback)(uv_tcp_t *client, const char *response);
} request_context_t;

// 增强网络模块私有数据
typedef struct {
    uv_tcp_t server;
    uv_tcp_t **clients;
    size_t client_count;
    size_t client_capacity;
    enhanced_network_config_t config;
    uv_timer_t stats_timer;
    int total_requests;
    int active_requests;
} enhanced_network_private_data_t;

// 增强网络模块接口
extern module_interface_t enhanced_network_module;

// 增强网络模块函数
int enhanced_network_module_init(module_interface_t *self, uv_loop_t *loop);
int enhanced_network_module_start(module_interface_t *self);
int enhanced_network_module_stop(module_interface_t *self);
int enhanced_network_module_cleanup(module_interface_t *self);

// 请求处理函数
void process_request_in_threadpool(void *ctx);
void handle_response(uv_tcp_t *client, const char *response);

// 配置和统计函数
int enhanced_network_module_set_config(module_interface_t *self, enhanced_network_config_t *config);
enhanced_network_config_t* enhanced_network_module_get_config(module_interface_t *self);
void enhanced_network_module_print_stats(module_interface_t *self);

#endif // ENHANCED_NETWORK_MODULE_H
