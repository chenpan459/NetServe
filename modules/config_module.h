#ifndef CONFIG_MODULE_H
#define CONFIG_MODULE_H

#include "module_manager.h"
#include <stddef.h>

// 配置项类型
typedef enum {
    CONFIG_TYPE_STRING,
    CONFIG_TYPE_INT,
    CONFIG_TYPE_FLOAT,
    CONFIG_TYPE_BOOL
} config_type_t;

// 配置项结构
typedef struct config_item {
    char *key;
    config_type_t type;
    union {
        char *string_value;
        int int_value;
        float float_value;
        int bool_value;
    } value;
    struct config_item *next;
} config_item_t;

// 配置模块配置
typedef struct {
    char *config_file;
    int auto_save;
    int auto_reload;
} config_module_config_t;

// 配置模块私有数据
typedef struct {
    config_item_t *items;
    config_module_config_t config;
    uv_mutex_t config_mutex;
} config_private_data_t;

// 配置模块接口
extern module_interface_t config_module;

// 配置模块函数
int config_module_init(module_interface_t *self, uv_loop_t *loop);
int config_module_start(module_interface_t *self);
int config_module_stop(module_interface_t *self);
int config_module_cleanup(module_interface_t *self);

// 配置项操作函数
int config_set_string(const char *key, const char *value);
int config_set_int(const char *key, int value);
int config_set_float(const char *key, float value);
int config_set_bool(const char *key, int value);

const char* config_get_string(const char *key, const char *default_value);
int config_get_int(const char *key, int default_value);
float config_get_float(const char *key, float default_value);
int config_get_bool(const char *key, int default_value);

// 配置模块配置
int config_module_set_config(module_interface_t *self, config_module_config_t *config);
config_module_config_t* config_module_get_config(module_interface_t *self);

// 配置文件操作
int config_load_from_file(const char *filename);
int config_save_to_file(const char *filename);
void config_list_all(void);

#endif // CONFIG_MODULE_H
