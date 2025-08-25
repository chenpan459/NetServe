#include "src/modules/module_manager.h"
#include "src/log/logger_module.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>

#define INITIAL_MODULE_CAPACITY 16

// 创建模块管理器
module_manager_t* module_manager_create(uv_loop_t *loop) {
    module_manager_t *mgr = malloc(sizeof(module_manager_t));
    if (!mgr) {
        return NULL;
    }
    
    mgr->loop = loop;
    mgr->modules = malloc(sizeof(module_interface_t*) * INITIAL_MODULE_CAPACITY);
    if (!mgr->modules) {
        free(mgr);
        return NULL;
    }
    
    mgr->module_count = 0;
    mgr->module_capacity = INITIAL_MODULE_CAPACITY;
    
    return mgr;
}

// 销毁模块管理器
void module_manager_destroy(module_manager_t *mgr) {
    if (!mgr) return;
    
    // 停止所有模块
    module_manager_shutdown(mgr);
    
    // 释放模块数组
    if (mgr->modules) {
        free(mgr->modules);
    }
    
    free(mgr);
}

// 扩展模块数组容量
static int expand_module_capacity(module_manager_t *mgr) {
    size_t new_capacity = mgr->module_capacity * 2;
    module_interface_t **new_modules = realloc(mgr->modules, 
                                              sizeof(module_interface_t*) * new_capacity);
    if (!new_modules) {
        return -1;
    }
    
    mgr->modules = new_modules;
    mgr->module_capacity = new_capacity;
    return 0;
}

// 注册模块
int module_manager_register_module(module_manager_t *mgr, module_interface_t *module) {
    if (!mgr || !module || !module->name) {
        return -1;
    }
    
    // 检查模块是否已存在
    for (size_t i = 0; i < mgr->module_count; i++) {
        if (strcmp(mgr->modules[i]->name, module->name) == 0) {
            log_error("模块 %s 已存在", module->name);
            return -1;
        }
    }
    
    // 检查容量
    if (mgr->module_count >= mgr->module_capacity) {
        if (expand_module_capacity(mgr) != 0) {
            return -1;
        }
    }
    
    // 添加模块
    mgr->modules[mgr->module_count] = module;
    mgr->module_count++;
    
    log_info("模块 %s 注册成功", module->name);
    return 0;
}

// 注销模块
int module_manager_unregister_module(module_manager_t *mgr, const char *module_name) {
    if (!mgr || !module_name) {
        return -1;
    }
    
    for (size_t i = 0; i < mgr->module_count; i++) {
        if (strcmp(mgr->modules[i]->name, module_name) == 0) {
            // 移动后面的模块
            for (size_t j = i; j < mgr->module_count - 1; j++) {
                mgr->modules[j] = mgr->modules[j + 1];
            }
            mgr->module_count--;
            log_info("模块 %s 注销成功", module_name);
            return 0;
        }
    }
    
    log_error("模块 %s 不存在", module_name);
    return -1;
}

// 获取模块
module_interface_t* module_manager_get_module(module_manager_t *mgr, const char *module_name) {
    if (!mgr || !module_name) {
        return NULL;
    }
    
    for (size_t i = 0; i < mgr->module_count; i++) {
        if (strcmp(mgr->modules[i]->name, module_name) == 0) {
            return mgr->modules[i];
        }
    }
    
    return NULL;
}

// 检查模块依赖
static int check_module_dependencies(module_manager_t *mgr, module_interface_t *module) {
    if (!module->dependencies || module->dependency_count == 0) {
        return 0;
    }
    
    for (size_t i = 0; i < module->dependency_count; i++) {
        module_interface_t *dep = module_manager_get_module(mgr, module->dependencies[i]);
        if (!dep || dep->state < MODULE_STATE_INITIALIZED) {
            log_error("模块 %s 依赖模块 %s 未初始化", 
                    module->name, module->dependencies[i]);
            return -1;
        }
    }
    
    return 0;
}

// 启动所有模块
int module_manager_start(module_manager_t *mgr) {
    if (!mgr) {
        return -1;
    }
    
    log_info("正在启动所有模块...");
    
    // 第一轮：初始化所有模块
    for (size_t i = 0; i < mgr->module_count; i++) {
        module_interface_t *module = mgr->modules[i];
        
        if (module->state == MODULE_STATE_UNINITIALIZED) {
            if (check_module_dependencies(mgr, module) != 0) {
                continue;
            }
            
            if (module->init && module->init(module, mgr->loop) == 0) {
                module->state = MODULE_STATE_INITIALIZED;
                log_info("模块 %s 初始化成功", module->name);
            } else {
                module->state = MODULE_STATE_ERROR;
                log_error("模块 %s 初始化失败", module->name);
            }
        }
    }
    
    // 第二轮：启动所有已初始化的模块
    for (size_t i = 0; i < mgr->module_count; i++) {
        module_interface_t *module = mgr->modules[i];
        
        if (module->state == MODULE_STATE_INITIALIZED) {
            if (module->start && module->start(module) == 0) {
                module->state = MODULE_STATE_STARTED;
                log_info("模块 %s 启动成功", module->name);
            } else {
                module->state = MODULE_STATE_ERROR;
                log_error("模块 %s 启动失败", module->name);
            }
        }
    }
    
    log_info("模块启动完成");
    return 0;
}

// 停止所有模块
int module_manager_stop(module_manager_t *mgr) {
    if (!mgr) {
        return -1;
    }
    
    log_info("正在停止所有模块...");
    
    for (size_t i = 0; i < mgr->module_count; i++) {
        module_interface_t *module = mgr->modules[i];
        
        if (module->state == MODULE_STATE_STARTED) {
            if (module->stop && module->stop(module) == 0) {
                module->state = MODULE_STATE_STOPPED;
                log_info("模块 %s 停止成功", module->name);
            } else {
                log_error("模块 %s 停止失败", module->name);
            }
        }
    }
    
    return 0;
}

// 关闭所有模块
int module_manager_shutdown(module_manager_t *mgr) {
    if (!mgr) {
        return -1;
    }
    
    log_info("正在关闭所有模块...");
    
    // 先停止所有模块
    module_manager_stop(mgr);
    
    // 然后清理所有模块
    for (size_t i = 0; i < mgr->module_count; i++) {
        module_interface_t *module = mgr->modules[i];
        
        if (module->state > MODULE_STATE_UNINITIALIZED) {
            if (module->cleanup && module->cleanup(module) == 0) {
                module->state = MODULE_STATE_UNINITIALIZED;
                log_info("模块 %s 清理成功", module->name);
            } else {
                log_error("模块 %s 清理失败", module->name);
            }
        }
    }
    
    return 0;
}

// 获取模块状态
module_state_t module_manager_get_module_state(module_manager_t *mgr, const char *module_name) {
    module_interface_t *module = module_manager_get_module(mgr, module_name);
    return module ? module->state : MODULE_STATE_UNINITIALIZED;
}

// 列出所有模块
void module_manager_list_modules(module_manager_t *mgr) {
    if (!mgr) {
        return;
    }
    
    log_info("\n=== 模块列表 ===");
    log_info("总模块数: %zu", mgr->module_count);
    
    for (size_t i = 0; i < mgr->module_count; i++) {
        module_interface_t *module = mgr->modules[i];
        const char *state_str;
        
        switch (module->state) {
            case MODULE_STATE_UNINITIALIZED: state_str = "未初始化"; break;
            case MODULE_STATE_INITIALIZED: state_str = "已初始化"; break;
            case MODULE_STATE_STARTED: state_str = "运行中"; break;
            case MODULE_STATE_STOPPED: state_str = "已停止"; break;
            case MODULE_STATE_ERROR: state_str = "错误"; break;
            default: state_str = "未知"; break;
        }
        
        log_info("  %s (v%s): %s", module->name, module->version, state_str);
    }
    log_info("================\n\n");
}
