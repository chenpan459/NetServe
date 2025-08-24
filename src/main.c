#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <uv.h>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define F_OK 0
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif
#include "src/modules/module_manager.h"
#include "src/memory/memory_pool_module.h"
#include "src/thread/threadpool_module.h"
#include "src/net/enhanced_network_module.h"
#include "src/log/logger_module.h"
#include "src/config/config_module.h"
#include "src/http/http_module.h"
#include "src/json/json_parser_module.h"
#include "src/db/database_module.h"
#include "src/http/http_routes.h"


// 全局变量
uv_loop_t *main_loop;
module_manager_t *module_mgr;

// 程序退出处理
void cleanup_and_exit(int exit_code) {
    printf("\n正在关闭程序...\n");
    
    // 关闭所有模块
    if (module_mgr) {
        module_manager_shutdown(module_mgr);
        module_manager_destroy(module_mgr);
    }
    
    // 关闭事件循环
    if (main_loop) {
        uv_loop_close(main_loop);
    }
    
    printf("程序已退出\n");
    exit(exit_code);
}

// 信号处理
void signal_handler(uv_signal_t *handle, int signum) {
    (void)handle; // 避免未使用参数警告
    printf("\n收到信号 %d，正在退出...\n", signum);
    cleanup_and_exit(0);
}

// 初始化配置文件系统
static int initialize_config_system() {
    // 检查配置文件是否存在
#ifdef _WIN32
    if (_access("config/config.ini", 0) != 0) {
#else
    if (access("config/config.ini", F_OK) != 0) {
#endif
        printf("配置文件不存在: config/config.ini\n");
        printf("请确保配置文件已正确放置在config目录中\n");
        return -1;
    }
    
    printf("配置文件系统初始化完成\n");
    return 0;
}

// 主程序初始化
int initialize_program() {
    printf("=== TCP 通信程序启动 ===\n");
    
    // 初始化配置文件系统
    if (initialize_config_system() != 0) {
        fprintf(stderr, "配置文件系统初始化失败\n");
        return -1;
    }
    
    // 创建主事件循环
    main_loop = uv_default_loop();
    if (!main_loop) {
        fprintf(stderr, "创建事件循环失败\n");
        return -1;
    }
    
    // 初始化模块管理器
    module_mgr = module_manager_create(main_loop);
    if (!module_mgr) {
        fprintf(stderr, "创建模块管理器失败\n");
        return -1;
    }
    
    // 注册各个模块
    if (module_manager_register_module(module_mgr, &config_module) != 0) {
        fprintf(stderr, "注册配置模块失败\n");
        return -1;
    }
    
    if (module_manager_register_module(module_mgr, &logger_module) != 0) {
        fprintf(stderr, "注册日志模块失败\n");
        return -1;
    }
    
    if (module_manager_register_module(module_mgr, &memory_pool_module) != 0) {
        fprintf(stderr, "注册内存池模块失败\n");
        return -1;
    }
    
    if (module_manager_register_module(module_mgr, &threadpool_module) != 0) {
        fprintf(stderr, "注册线程池模块失败\n");
        return -1;
    }
    
    if (module_manager_register_module(module_mgr, &enhanced_network_module) != 0) {
        fprintf(stderr, "注册增强网络模块失败\n");
        return -1;
    }
    
    if (module_manager_register_module(module_mgr, &http_module) != 0) {
        fprintf(stderr, "注册HTTP模块失败\n");
        return -1;
    }
    
    if (module_manager_register_module(module_mgr, (struct module_interface*)&database_module_interface) != 0) {
        fprintf(stderr, "注册数据库模块失败\n");
        return -1;
    }
    
    // 注册信号处理
    uv_signal_t *signal_handle = malloc(sizeof(uv_signal_t));
    uv_signal_init(main_loop, signal_handle);
    uv_signal_start(signal_handle, signal_handler, SIGINT);
    uv_signal_start(signal_handle, signal_handler, SIGTERM);
    
    printf("程序初始化完成\n");
    return 0;
}

// 主程序运行
int run_program() {
    printf("程序开始运行...\n");
    printf("按 Ctrl+C 退出程序\n");
    
    // 启动所有模块
    if (module_manager_start(module_mgr) != 0) {
        fprintf(stderr, "启动模块失败\n");
        return -1;
    }
    
    // 在模块启动后注册HTTP路由
    register_http_routes();
    
    // 运行事件循环
    int result = uv_run(main_loop, UV_RUN_DEFAULT);
    
    if (result != 0) {
        fprintf(stderr, "事件循环运行失败: %s\n", uv_strerror(result));
        return -1;
    }
    
    return 0;
}

int main(int argc, char *argv[]) {
    (void)argc; // 避免未使用参数警告
    (void)argv; // 避免未使用参数警告
    
    // 初始化程序
    if (initialize_program() != 0) {
        fprintf(stderr, "程序初始化失败\n");
        return 1;
    }
    
    // 运行程序
    if (run_program() != 0) {
        fprintf(stderr, "程序运行失败\n");
        cleanup_and_exit(1);
    }
    
    // 正常退出
    cleanup_and_exit(0);
    return 0;
}
