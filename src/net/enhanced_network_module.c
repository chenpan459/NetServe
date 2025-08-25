#include "src/net/enhanced_network_module.h"
#include "src/log/logger_module.h"
#include "src/thread/threadpool_module.h"
#include "src/config/config_module.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define INITIAL_CLIENT_CAPACITY 16

// 增强网络模块接口定义
module_interface_t enhanced_network_module = {
    .name = "enhanced_network",
    .version = "1.0.0",
    .init = enhanced_network_module_init,
    .start = enhanced_network_module_start,
    .stop = enhanced_network_module_stop,
    .cleanup = enhanced_network_module_cleanup,
    .state = MODULE_STATE_UNINITIALIZED,
    .private_data = NULL,
    .dependencies = NULL,
    .dependency_count = 0
};

// 默认配置
static enhanced_network_config_t default_config = {
    .port = 8080,
    .host = "0.0.0.0",
    .backlog = 128,
    .max_connections = 1000,
    .enable_threadpool = 1,
    .max_concurrent_requests = 100,
    .request_timeout_ms = 30000
};

// 扩展客户端数组容量
static int expand_client_capacity(enhanced_network_private_data_t *data) {
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
static int add_client(enhanced_network_private_data_t *data, uv_tcp_t *client) {
    if (data->client_count >= data->client_capacity) {
        if (expand_client_capacity(data) != 0) {
            return -1;
        }
    }
    
    data->clients[data->client_count] = client;
    data->client_count++;
    
            log_info("新客户端连接，当前连接数: %zu", data->client_count);
    return 0;
}

// 移除客户端连接
static int remove_client(enhanced_network_private_data_t *data, uv_tcp_t *client) {
    for (size_t i = 0; i < data->client_count; i++) {
        if (data->clients[i] == client) {
            // 移动后面的客户端
            for (size_t j = i; j < data->client_count - 1; j++) {
                data->clients[j] = data->clients[j + 1];
            }
            data->client_count--;
            log_info("客户端断开连接，当前连接数: %zu", data->client_count);
            return 0;
        }
    }
    return -1;
}

// 客户端关闭回调
static void on_client_close(uv_handle_t *handle) {
    enhanced_network_private_data_t *data = (enhanced_network_private_data_t*) handle->data;
    remove_client(data, (uv_tcp_t*) handle);
    free(handle);
}

// 写入完成回调
static void on_write_complete(uv_write_t *req, int status) {
    if (status) {
        log_error("写入错误: %s", uv_strerror(status));
    }
    
    // 释放写入请求
    free(req);
}

// 在线程池中处理请求
void process_request_in_threadpool(void *ctx) {
    if (!ctx) return;
    
    // 类型转换
    request_context_t *request_ctx = (request_context_t*) ctx;
    
    // 模拟处理时间（在实际应用中这里会进行真正的业务逻辑处理）
    int processing_time = rand() % 100 + 10; // 10-110ms
#ifdef _WIN32
    Sleep(processing_time); // Windows下使用毫秒
#else
    usleep(processing_time * 1000); // Unix下使用微秒
#endif
    
    // 构造响应消息
    char response[512];
    snprintf(response, sizeof(response), 
             "线程池处理完成，请求大小: %zu 字节，处理时间: %d ms", 
             request_ctx->request_size, processing_time);
    
    // 调用响应回调
    if (request_ctx->response_callback) {
        request_ctx->response_callback(request_ctx->client, response);
    }
    
    // 释放请求上下文
    free(request_ctx->request_data);
    free(request_ctx);
}

// 处理响应
void handle_response(uv_tcp_t *client, const char *response) {
    if (!client || !response) return;
    
    // 创建写入请求
    uv_write_t *write_req = malloc(sizeof(uv_write_t));
    if (!write_req) return;
    
    // 创建响应缓冲区
    char *response_copy = strdup(response);
    if (!response_copy) {
        free(write_req);
        return;
    }
    
    uv_buf_t response_buf = uv_buf_init(response_copy, strlen(response_copy));
    
    // 异步写入响应
    uv_write(write_req, (uv_stream_t*) client, &response_buf, 1, on_write_complete);
}

// 读取回调
static void on_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    if (nread > 0) {
        enhanced_network_private_data_t *data = (enhanced_network_private_data_t*) stream->data;
        
        // 显示接收到的消息
        log_info("收到消息: %.*s", (int)nread, buf->base);
        
        // 如果启用了线程池，在线程池中处理请求
        if (data->config.enable_threadpool) {
            // 创建请求上下文
            request_context_t *ctx = malloc(sizeof(request_context_t));
            if (ctx) {
                ctx->client = (uv_tcp_t*) stream;
                ctx->request_data = malloc(nread + 1);
                if (ctx->request_data) {
                    memcpy(ctx->request_data, buf->base, nread);
                    ctx->request_data[nread] = '\0';
                    ctx->request_size = nread;
                    ctx->response_callback = handle_response;
                    
                    // 提交到线程池处理
                    if (threadpool_submit_work(process_request_in_threadpool, ctx) == 0) {
                        data->total_requests++;
                        data->active_requests++;
                        log_info("请求已提交到线程池处理");
                    } else {
                        log_error("提交请求到线程池失败");
                        free(ctx->request_data);
                        free(ctx);
                    }
                } else {
                    free(ctx);
                }
            }
        } else {
            // 直接处理请求（同步方式）
            char response[256];
            snprintf(response, sizeof(response), "同步处理完成，消息: %.*s", (int)nread, buf->base);
            handle_response((uv_tcp_t*) stream, response);
        }
        
        free(buf->base);
    } else if (nread < 0) {
        if (nread != UV_EOF) {
            log_error("读取错误: %s", uv_err_name(nread));
        }
        uv_close((uv_handle_t*) stream, on_client_close);
    } else {
        free(buf->base);
    }
}

// 分配缓冲区回调
static void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    (void)handle; // 抑制未使用参数警告
    buf->base = malloc(suggested_size);
    buf->len = suggested_size;
}

// 新连接回调
static void on_new_connection(uv_stream_t *server, int status) {
    if (status < 0) {
        log_error("新连接错误: %s", uv_strerror(status));
        return;
    }
    
    enhanced_network_private_data_t *data = (enhanced_network_private_data_t*) server->data;
    
    // 创建新的客户端连接
    uv_tcp_t *client = malloc(sizeof(uv_tcp_t));
    if (!client) {
        log_error("内存分配失败");
        return;
    }
    
    uv_tcp_init(server->loop, client);
    client->data = data;
    
    if (uv_accept(server, (uv_stream_t*) client) == 0) {
        if (add_client(data, client) == 0) {
            uv_read_start((uv_stream_t*) client, alloc_buffer, on_read);
            log_info("新客户端连接");
        } else {
            uv_close((uv_handle_t*) client, on_client_close);
        }
    } else {
        uv_close((uv_handle_t*) client, on_client_close);
    }
}

// 统计定时器回调
static void on_stats_timer(uv_timer_t *handle) {
    enhanced_network_private_data_t *data = (enhanced_network_private_data_t*) handle->data;
    
    log_info("\n=== 网络模块统计 ===");
    log_info("当前连接数: %zu", data->client_count);
    log_info("总请求数: %d", data->total_requests);
    log_info("活跃请求数: %d", data->active_requests);
    
    if (data->config.enable_threadpool) {
        log_info("线程池状态:");
        log_info("  活跃线程数: %d", threadpool_get_active_thread_count());
        log_info("  队列中工作数: %d", threadpool_get_queued_work_count());
    }
    log_info("==================\n\n");
}

// 增强网络模块初始化
int enhanced_network_module_init(module_interface_t *self, uv_loop_t *loop) {
    if (!self || !loop) {
        return -1;
    }
    
    // 分配私有数据
    enhanced_network_private_data_t *data = malloc(sizeof(enhanced_network_private_data_t));
    if (!data) {
        return -1;
    }
    
    // 初始化私有数据
    memset(data, 0, sizeof(enhanced_network_private_data_t));
    data->config = default_config;
    
    data->clients = malloc(sizeof(uv_tcp_t*) * INITIAL_CLIENT_CAPACITY);
    if (!data->clients) {
        free(data);
        return -1;
    }
    
    data->client_capacity = INITIAL_CLIENT_CAPACITY;
    data->client_count = 0;
    data->total_requests = 0;
    data->active_requests = 0;
    
    // 初始化TCP服务器
    uv_tcp_init(loop, &data->server);
    data->server.data = data;
    
    // 初始化统计定时器
    uv_timer_init(loop, &data->stats_timer);
    data->stats_timer.data = data;
    
    self->private_data = data;
    
    log_info("增强网络模块初始化成功");
    return 0;
}

// 增强网络模块启动
int enhanced_network_module_start(module_interface_t *self) {
    if (!self || !self->private_data) {
        return -1;
    }
    
    enhanced_network_private_data_t *data = (enhanced_network_private_data_t*) self->private_data;
    
    // 从配置文件读取端口设置
    int config_port = config_get_int("enhanced_network_port", data->config.port);
    data->config.port = config_port;
    log_info("增强网络模块配置端口: %d (默认: %d)", config_port, data->config.port);
    
    // 绑定地址
    struct sockaddr_in addr;
    uv_ip4_addr(data->config.host, data->config.port, &addr);
    
    int bind_result = uv_tcp_bind(&data->server, (const struct sockaddr*)&addr, 0);
    if (bind_result != 0) {
        log_error("绑定地址失败: %s", uv_strerror(bind_result));
        return -1;
    }
    
    // 开始监听
    int listen_result = uv_listen((uv_stream_t*) &data->server, data->config.backlog, on_new_connection);
    if (listen_result != 0) {
        log_error("监听失败: %s", uv_strerror(listen_result));
        return -1;
    }
    
    // 启动统计定时器（每5秒打印一次统计信息）
    uv_timer_start(&data->stats_timer, on_stats_timer, 5000, 5000);
    
    log_info("增强网络模块启动成功，监听 %s:%d", data->config.host, data->config.port);
    log_info("线程池处理: %s", data->config.enable_threadpool ? "启用" : "禁用");
    return 0;
}

// 增强网络模块停止
int enhanced_network_module_stop(module_interface_t *self) {
    if (!self || !self->private_data) {
        return -1;
    }
    
    enhanced_network_private_data_t *data = (enhanced_network_private_data_t*) self->private_data;
    
    // 停止统计定时器
    uv_timer_stop(&data->stats_timer);
    
    // 关闭所有客户端连接
    for (size_t i = 0; i < data->client_count; i++) {
        uv_close((uv_handle_t*) data->clients[i], on_client_close);
    }
    
    // 关闭服务器
    uv_close((uv_handle_t*) &data->server, NULL);
    
    log_info("增强网络模块已停止");
    return 0;
}

// 增强网络模块清理
int enhanced_network_module_cleanup(module_interface_t *self) {
    if (!self || !self->private_data) {
        return -1;
    }
    
    enhanced_network_private_data_t *data = (enhanced_network_private_data_t*) self->private_data;
    
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
    
    log_info("增强网络模块清理完成");
    return 0;
}

// 设置增强网络模块配置
int enhanced_network_module_set_config(module_interface_t *self, enhanced_network_config_t *config) {
    if (!self || !self->private_data || !config) {
        return -1;
    }
    
    enhanced_network_private_data_t *data = (enhanced_network_private_data_t*) self->private_data;
    
    // 更新配置
    data->config = *config;
    
    log_info("增强网络模块配置已更新");
    return 0;
}

// 获取增强网络模块配置
enhanced_network_config_t* enhanced_network_module_get_config(module_interface_t *self) {
    if (!self || !self->private_data) {
        return NULL;
    }
    
    enhanced_network_private_data_t *data = (enhanced_network_private_data_t*) self->private_data;
    return &data->config;
}

// 打印增强网络模块统计信息
void enhanced_network_module_print_stats(module_interface_t *self) {
    if (!self || !self->private_data) {
        log_info("增强网络模块未初始化");
        return;
    }
    
    enhanced_network_private_data_t *data = (enhanced_network_private_data_t*) self->private_data;
    
    log_info("\n=== 增强网络模块统计 ===");
    log_info("当前连接数: %zu", data->client_count);
    log_info("总请求数: %d", data->total_requests);
    log_info("活跃请求数: %d", data->active_requests);
    log_info("线程池处理: %s", data->config.enable_threadpool ? "启用" : "禁用");
    log_info("最大并发请求数: %d", data->config.max_concurrent_requests);
    log_info("请求超时时间: %d ms", data->config.request_timeout_ms);
    log_info("========================\n\n");
}
