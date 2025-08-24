#ifndef NETWORK_MODULE_H
#define NETWORK_MODULE_H

#include "module_manager.h"
#include <uv.h>

// 网络模块配置
typedef struct {
    int port;
    char *host;
    int backlog;
    int max_connections;
} network_config_t;

// 网络模块私有数据
typedef struct {
    uv_tcp_t server;
    uv_tcp_t **clients;
    size_t client_count;
    size_t client_capacity;
    network_config_t config;
} network_private_data_t;

// 网络模块接口
extern module_interface_t network_module;

// 网络模块函数
int network_module_init(module_interface_t *self, uv_loop_t *loop);
int network_module_start(module_interface_t *self);
int network_module_stop(module_interface_t *self);
int network_module_cleanup(module_interface_t *self);

// 网络模块配置
int network_module_set_config(module_interface_t *self, network_config_t *config);
network_config_t* network_module_get_config(module_interface_t *self);

// 网络模块统计
size_t network_module_get_client_count(module_interface_t *self);
void network_module_list_clients(module_interface_t *self);

#endif // NETWORK_MODULE_H
