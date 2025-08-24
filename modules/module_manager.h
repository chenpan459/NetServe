#ifndef MODULE_MANAGER_H
#define MODULE_MANAGER_H

#include <uv.h>
#include <stddef.h>

// 模块状态枚举
typedef enum {
    MODULE_STATE_UNINITIALIZED = 0,
    MODULE_STATE_INITIALIZED,
    MODULE_STATE_STARTED,
    MODULE_STATE_STOPPED,
    MODULE_STATE_ERROR
} module_state_t;

// 模块接口结构
typedef struct module_interface {
    const char *name;                    // 模块名称
    const char *version;                 // 模块版本
    
    // 模块生命周期函数
    int (*init)(struct module_interface *self, uv_loop_t *loop);
    int (*start)(struct module_interface *self);
    int (*stop)(struct module_interface *self);
    int (*cleanup)(struct module_interface *self);
    
    // 模块状态
    module_state_t state;
    
    // 模块私有数据
    void *private_data;
    
    // 模块依赖列表
    const char **dependencies;
    size_t dependency_count;
} module_interface_t;

// 模块管理器结构
typedef struct module_manager {
    uv_loop_t *loop;
    module_interface_t **modules;
    size_t module_count;
    size_t module_capacity;
} module_manager_t;

// 模块管理器函数
module_manager_t* module_manager_create(uv_loop_t *loop);
void module_manager_destroy(module_manager_t *mgr);

// 模块注册和管理
int module_manager_register_module(module_manager_t *mgr, module_interface_t *module);
int module_manager_unregister_module(module_manager_t *mgr, const char *module_name);
module_interface_t* module_manager_get_module(module_manager_t *mgr, const char *module_name);

// 模块生命周期管理
int module_manager_start(module_manager_t *mgr);
int module_manager_stop(module_manager_t *mgr);
int module_manager_shutdown(module_manager_t *mgr);

// 模块状态查询
module_state_t module_manager_get_module_state(module_manager_t *mgr, const char *module_name);
void module_manager_list_modules(module_manager_t *mgr);

#endif // MODULE_MANAGER_H
