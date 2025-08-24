#include "network_module.h"
#include "config_module.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_CLIENT_CAPACITY 16

// 网络模块接口定义
module_interface_t network_module = {
    .name = "network",
    .version = "1.0.0",
    .init = network_module_init,
    .start = network_module_start,
    .stop = network_module_stop,
    .cleanup = network_module_cleanup,
    .state = MODULE_STATE_UNINITIALIZED,
    .private_data = NULL,
    .dependencies = NULL,
    .dependency_count = 0
};

// 默认配置
static network_config_t default_config = {
    .port = 8080,
    .host = "0.0.0.0",
    .backlog = 128,
    .max_connections = 1000
};

// 扩展客户端数组容量
static int expand_client_capacity(network_private_data_t *data) {
    size_t new_capacity = data->client_capacity * 2;
    uv_tcp_t **new_clients = realloc(data->clients, 
                                     sizeof(uv_tcp_t*) * new_capacity);
    if (!new_clients) {
        return -1;
    }
    
    data->clients = new_clients;
    data->client_capacity = new_capacity;
    return 0;
}

// 添加客户端连接
static int add_client(network_private_data_t *data, uv_tcp_t *client) {
    if (data->client_count >= data->client_capacity) {
        if (expand_client_capacity(data) != 0) {
            return -1;
        }
    }
    
    data->clients[data->client_count] = client;
    data->client_count++;
    
    printf("新客户端连接，当前连接数: %zu\n", data->client_count);
    return 0;
}

// 移除客户端连接
static int remove_client(network_private_data_t *data, uv_tcp_t *client) {
    for (size_t i = 0; i < data->client_count; i++) {
        if (data->clients[i] == client) {
            // 移动后面的客户端
            for (size_t j = i; j < data->client_count - 1; j++) {
                data->clients[j] = data->clients[j + 1];
            }
            data->client_count--;
            printf("客户端断开连接，当前连接数: %zu\n", data->client_count);
            return 0;
        }
    }
    return -1;
}

// 客户端关闭回调
static void on_client_close(uv_handle_t *handle) {
    network_private_data_t *data = (network_private_data_t*) handle->data;
    remove_client(data, (uv_tcp_t*) handle);
    free(handle);
}

// 写入完成回调
static void on_write_complete(uv_write_t *req, int status) {
    if (status) {
        fprintf(stderr, "写入错误: %s\n", uv_strerror(status));
    }
    
    // 释放写入请求
    free(req);
}

// 读取回调
static void on_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    if (nread > 0) {
        // 显示接收到的消息
        printf("收到消息: %.*s\n", (int)nread, buf->base);
        
        // 构造回复消息
        char reply[256];
        snprintf(reply, sizeof(reply), "服务器已收到消息: %.*s", (int)nread, buf->base);
        
        // 发送回复
        uv_write_t *write_req = malloc(sizeof(uv_write_t));
        if (write_req) {
            uv_buf_t reply_buf = uv_buf_init(strdup(reply), strlen(reply));
            uv_write(write_req, stream, &reply_buf, 1, on_write_complete);
        }
        
        free(buf->base);
    } else if (nread < 0) {
        if (nread != UV_EOF) {
            fprintf(stderr, "读取错误: %s\n", uv_err_name(nread));
        }
        uv_close((uv_handle_t*) stream, on_client_close);
    } else {
        free(buf->base);
    }
}

// 分配缓冲区回调
static void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    (void)handle; // 避免未使用参数警告
    buf->base = malloc(suggested_size);
    buf->len = suggested_size;
}

// 新连接回调
static void on_new_connection(uv_stream_t *server, int status) {
    if (status < 0) {
        fprintf(stderr, "新连接错误: %s\n", uv_strerror(status));
        return;
    }
    
    network_private_data_t *data = (network_private_data_t*) server->data;
    
    // 创建新的客户端连接
    uv_tcp_t *client = malloc(sizeof(uv_tcp_t));
    if (!client) {
        fprintf(stderr, "内存分配失败\n");
        return;
    }
    
    uv_tcp_init(server->loop, client);
    client->data = data;
    
    if (uv_accept(server, (uv_stream_t*) client) == 0) {
        if (add_client(data, client) == 0) {
            uv_read_start((uv_stream_t*) client, alloc_buffer, on_read);
            printf("新客户端连接\n");
        } else {
            uv_close((uv_handle_t*) client, on_client_close);
        }
    } else {
        uv_close((uv_handle_t*) client, on_client_close);
    }
}

// 网络模块初始化
int network_module_init(module_interface_t *self, uv_loop_t *loop) {
    if (!self || !loop) {
        return -1;
    }
    
    // 分配私有数据
    network_private_data_t *data = malloc(sizeof(network_private_data_t));
    if (!data) {
        return -1;
    }
    
    // 初始化私有数据
    memset(data, 0, sizeof(network_private_data_t));
    data->config = default_config;
    
    data->clients = malloc(sizeof(uv_tcp_t*) * INITIAL_CLIENT_CAPACITY);
    if (!data->clients) {
        free(data);
        return -1;
    }
    
    data->client_capacity = INITIAL_CLIENT_CAPACITY;
    data->client_count = 0;
    
    // 初始化TCP服务器
    uv_tcp_init(loop, &data->server);
    data->server.data = data;
    
    self->private_data = data;
    
    printf("网络模块初始化成功\n");
    return 0;
}

// 网络模块启动
int network_module_start(module_interface_t *self) {
    if (!self || !self->private_data) {
        return -1;
    }
    
    network_private_data_t *data = (network_private_data_t*) self->private_data;
    
    // 从配置文件读取端口设置
    int config_port = config_get_int("network_port", data->config.port);
    data->config.port = config_port;
    printf("网络模块配置端口: %d (默认: %d)\n", config_port, data->config.port);
    
    // 绑定地址
    struct sockaddr_in addr;
    uv_ip4_addr(data->config.host, data->config.port, &addr);
    
    int bind_result = uv_tcp_bind(&data->server, (const struct sockaddr*)&addr, 0);
    if (bind_result != 0) {
        fprintf(stderr, "绑定地址失败: %s\n", uv_strerror(bind_result));
        return -1;
    }
    
    // 开始监听
    int listen_result = uv_listen((uv_stream_t*) &data->server, data->config.backlog, on_new_connection);
    if (listen_result != 0) {
        fprintf(stderr, "监听失败: %s\n", uv_strerror(listen_result));
        return -1;
    }
    
    printf("网络模块启动成功，监听 %s:%d\n", data->config.host, data->config.port);
    return 0;
}

// 网络模块停止
int network_module_stop(module_interface_t *self) {
    if (!self || !self->private_data) {
        return -1;
    }
    
    network_private_data_t *data = (network_private_data_t*) self->private_data;
    
    // 关闭所有客户端连接
    for (size_t i = 0; i < data->client_count; i++) {
        uv_close((uv_handle_t*) data->clients[i], on_client_close);
    }
    
    // 关闭服务器
    uv_close((uv_handle_t*) &data->server, NULL);
    
    printf("网络模块已停止\n");
    return 0;
}

// 网络模块清理
int network_module_cleanup(module_interface_t *self) {
    if (!self || !self->private_data) {
        return -1;
    }
    
    network_private_data_t *data = (network_private_data_t*) self->private_data;
    
    // 释放客户端数组
    if (data->clients) {
        free(data->clients);
    }
    
    // 释放配置
    if (data->config.host != default_config.host) {
        free(data->config.host);
    }
    
    // 释放私有数据
    free(data);
    self->private_data = NULL;
    
    printf("网络模块清理完成\n");
    return 0;
}

// 设置网络模块配置
int network_module_set_config(module_interface_t *self, network_config_t *config) {
    if (!self || !self->private_data || !config) {
        return -1;
    }
    
    network_private_data_t *data = (network_private_data_t*) self->private_data;
    
    // 更新配置
    data->config.port = config->port;
    data->config.backlog = config->backlog;
    data->config.max_connections = config->max_connections;
    
    // 更新主机地址
    if (data->config.host != default_config.host) {
        free(data->config.host);
    }
    data->config.host = strdup(config->host);
    
    printf("网络模块配置已更新\n");
    return 0;
}

// 获取网络模块配置
network_config_t* network_module_get_config(module_interface_t *self) {
    if (!self || !self->private_data) {
        return NULL;
    }
    
    network_private_data_t *data = (network_private_data_t*) self->private_data;
    return &data->config;
}

// 获取客户端连接数
size_t network_module_get_client_count(module_interface_t *self) {
    if (!self || !self->private_data) {
        return 0;
    }
    
    network_private_data_t *data = (network_private_data_t*) self->private_data;
    return data->client_count;
}

// 列出所有客户端
void network_module_list_clients(module_interface_t *self) {
    if (!self || !self->private_data) {
        return;
    }
    
    network_private_data_t *data = (network_private_data_t*) self->private_data;
    
    printf("当前客户端连接数: %zu\n", data->client_count);
    for (size_t i = 0; i < data->client_count; i++) {
        printf("  客户端 %zu: %p\n", i, data->clients[i]);
    }
}
