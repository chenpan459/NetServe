#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <uv.h>
#include "modules/module_manager.h"
#include "modules/network_module.h"
#include "modules/logger_module.h"
#include "modules/config_module.h"

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
    printf("\n收到信号 %d，正在退出...\n", signum);
    cleanup_and_exit(0);
}

// 主程序初始化
int initialize_program() {
    printf("=== TCP 通信程序启动 ===\n");
    
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
    
    if (module_manager_register_module(module_mgr, &network_module) != 0) {
        fprintf(stderr, "注册网络模块失败\n");
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
    
    // 运行事件循环
    int result = uv_run(main_loop, UV_RUN_DEFAULT);
    
    if (result != 0) {
        fprintf(stderr, "事件循环运行失败: %s\n", uv_strerror(result));
        return -1;
    }
    
    return 0;
}

int main(int argc, char *argv[]) {
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
