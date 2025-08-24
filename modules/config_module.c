#include "config_module.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 默认配置
static config_module_config_t default_config = {
    .config_file = "config/config.ini",
    .auto_save = 1,
    .auto_reload = 0
};

// 配置模块接口定义
module_interface_t config_module = {
    .name = "config",
    .version = "1.0.0",
    .init = config_module_init,
    .start = config_module_start,
    .stop = config_module_stop,
    .cleanup = config_module_cleanup,
    .state = MODULE_STATE_UNINITIALIZED,
    .private_data = NULL,
    .dependencies = NULL,
    .dependency_count = 0
};

// 全局配置数据
static config_private_data_t *global_config_data = NULL;

// 查找配置项
static config_item_t* find_config_item(const char *key) {
    if (!global_config_data || !key) {
        return NULL;
    }
    
    config_item_t *item = global_config_data->items;
    while (item) {
        if (strcmp(item->key, key) == 0) {
            return item;
        }
        item = item->next;
    }
    
    return NULL;
}

// 创建新的配置项
static config_item_t* create_config_item(const char *key, config_type_t type) {
    config_item_t *item = malloc(sizeof(config_item_t));
    if (!item) {
        return NULL;
    }
    
    item->key = strdup(key);
    item->type = type;
    item->next = NULL;
    
    // 初始化值
    switch (type) {
        case CONFIG_TYPE_STRING:
            item->value.string_value = NULL;
            break;
        case CONFIG_TYPE_INT:
            item->value.int_value = 0;
            break;
        case CONFIG_TYPE_FLOAT:
            item->value.float_value = 0.0f;
            break;
        case CONFIG_TYPE_BOOL:
            item->value.bool_value = 0;
            break;
    }
    
    return item;
}

// 添加配置项到链表
static int add_config_item(config_item_t *item) {
    if (!global_config_data || !item) {
        return -1;
    }
    
    // 如果链表为空，直接添加
    if (!global_config_data->items) {
        global_config_data->items = item;
        return 0;
    }
    
    // 添加到链表末尾
    config_item_t *current = global_config_data->items;
    while (current->next) {
        current = current->next;
    }
    current->next = item;
    
    return 0;
}

// 配置模块初始化
int config_module_init(module_interface_t *self, uv_loop_t *loop) {
    (void)loop; // 避免未使用参数警告
    
    if (!self) {
        return -1;
    }
    
    // 分配私有数据
    config_private_data_t *data = malloc(sizeof(config_private_data_t));
    if (!data) {
        return -1;
    }
    
    // 初始化私有数据
    memset(data, 0, sizeof(config_private_data_t));
    data->config = default_config;
    data->items = NULL;
    
    // 初始化互斥锁
    if (uv_mutex_init(&data->config_mutex) != 0) {
        free(data);
        return -1;
    }
    
    self->private_data = data;
    global_config_data = data;
    
    printf("配置模块初始化成功\n");
    return 0;
}

// 配置模块启动
int config_module_start(module_interface_t *self) {
    if (!self || !self->private_data) {
        return -1;
    }
    
    config_private_data_t *data = (config_private_data_t*) self->private_data;
    
    // 尝试加载配置文件
    if (data->config.config_file) {
        config_load_from_file(data->config.config_file);
    }
    
    printf("配置模块启动成功\n");
    return 0;
}

// 配置模块停止
int config_module_stop(module_interface_t *self) {
    if (!self || !self->private_data) {
        return -1;
    }
    
    printf("配置模块已停止\n");
    return 0;
}

// 配置模块清理
int config_module_cleanup(module_interface_t *self) {
    if (!self || !self->private_data) {
        return -1;
    }
    
    config_private_data_t *data = (config_private_data_t*) self->private_data;
    
    // 保存配置到文件
    if (data->config.auto_save && data->config.config_file) {
        config_save_to_file(data->config.config_file);
    }
    
    // 释放所有配置项
    config_item_t *item = data->items;
    while (item) {
        config_item_t *next = item->next;
        
        if (item->key) {
            free(item->key);
        }
        
        if (item->type == CONFIG_TYPE_STRING && item->value.string_value) {
            free(item->value.string_value);
        }
        
        free(item);
        item = next;
    }
    
    // 释放配置文件路径
    if (data->config.config_file != default_config.config_file) {
        free(data->config.config_file);
    }
    
    // 销毁互斥锁
    uv_mutex_destroy(&data->config_mutex);
    
    // 释放私有数据
    free(data);
    self->private_data = NULL;
    global_config_data = NULL;
    
    printf("配置模块清理完成\n");
    return 0;
}

// 设置字符串配置
int config_set_string(const char *key, const char *value) {
    if (!key || !value) {
        return -1;
    }
    
    uv_mutex_lock(&global_config_data->config_mutex);
    
    config_item_t *item = find_config_item(key);
    if (item) {
        // 更新现有项
        if (item->type == CONFIG_TYPE_STRING) {
            if (item->value.string_value) {
                free(item->value.string_value);
            }
            item->value.string_value = strdup(value);
        }
    } else {
        // 创建新项
        item = create_config_item(key, CONFIG_TYPE_STRING);
        if (item) {
            item->value.string_value = strdup(value);
            add_config_item(item);
        }
    }
    
    uv_mutex_unlock(&global_config_data->config_mutex);
    
    return item ? 0 : -1;
}

// 设置整数配置
int config_set_int(const char *key, int value) {
    if (!key) {
        return -1;
    }
    
    uv_mutex_lock(&global_config_data->config_mutex);
    
    config_item_t *item = find_config_item(key);
    if (item) {
        // 更新现有项
        if (item->type == CONFIG_TYPE_INT) {
            item->value.int_value = value;
        }
    } else {
        // 创建新项
        item = create_config_item(key, CONFIG_TYPE_INT);
        if (item) {
            item->value.int_value = value;
            add_config_item(item);
        }
    }
    
    uv_mutex_unlock(&global_config_data->config_mutex);
    
    return item ? 0 : -1;
}

// 设置浮点数配置
int config_set_float(const char *key, float value) {
    if (!key) {
        return -1;
    }
    
    uv_mutex_lock(&global_config_data->config_mutex);
    
    config_item_t *item = find_config_item(key);
    if (item) {
        // 更新现有项
        if (item->type == CONFIG_TYPE_FLOAT) {
            item->value.float_value = value;
        }
    } else {
        // 创建新项
        item = create_config_item(key, CONFIG_TYPE_FLOAT);
        if (item) {
            item->value.float_value = value;
            add_config_item(item);
        }
    }
    
    uv_mutex_unlock(&global_config_data->config_mutex);
    
    return item ? 0 : -1;
}

// 设置布尔配置
int config_set_bool(const char *key, int value) {
    if (!key) {
        return -1;
    }
    
    uv_mutex_lock(&global_config_data->config_mutex);
    
    config_item_t *item = find_config_item(key);
    if (item) {
        // 更新现有项
        if (item->type == CONFIG_TYPE_BOOL) {
            item->value.bool_value = value ? 1 : 0;
        }
    } else {
        // 创建新项
        item = create_config_item(key, CONFIG_TYPE_BOOL);
        if (item) {
            item->value.bool_value = value ? 1 : 0;
            add_config_item(item);
        }
    }
    
    uv_mutex_unlock(&global_config_data->config_mutex);
    
    return item ? 0 : -1;
}

// 获取字符串配置
const char* config_get_string(const char *key, const char *default_value) {
    if (!key) {
        return default_value;
    }
    
    uv_mutex_lock(&global_config_data->config_mutex);
    
    config_item_t *item = find_config_item(key);
    const char *result = default_value;
    
    if (item && item->type == CONFIG_TYPE_STRING && item->value.string_value) {
        result = item->value.string_value;
    }
    
    uv_mutex_unlock(&global_config_data->config_mutex);
    
    return result;
}

// 获取整数配置
int config_get_int(const char *key, int default_value) {
    if (!key) {
        return default_value;
    }
    
    uv_mutex_lock(&global_config_data->config_mutex);
    
    config_item_t *item = find_config_item(key);
    int result = default_value;
    
    if (item && item->type == CONFIG_TYPE_INT) {
        result = item->value.int_value;
    }
    
    uv_mutex_unlock(&global_config_data->config_mutex);
    
    return result;
}

// 获取浮点数配置
float config_get_float(const char *key, float default_value) {
    if (!key) {
        return default_value;
    }
    
    uv_mutex_lock(&global_config_data->config_mutex);
    
    config_item_t *item = find_config_item(key);
    float result = default_value;
    
    if (item && item->type == CONFIG_TYPE_FLOAT) {
        result = item->value.float_value;
    }
    
    uv_mutex_unlock(&global_config_data->config_mutex);
    
    return result;
}

// 获取布尔配置
int config_get_bool(const char *key, int default_value) {
    if (!key) {
        return default_value;
    }
    
    uv_mutex_lock(&global_config_data->config_mutex);
    
    config_item_t *item = find_config_item(key);
    int result = default_value;
    
    if (item && item->type == CONFIG_TYPE_BOOL) {
        result = item->value.bool_value;
    }
    
    uv_mutex_unlock(&global_config_data->config_mutex);
    
    return result;
}

// 设置配置模块配置
int config_module_set_config(module_interface_t *self, config_module_config_t *config) {
    if (!self || !self->private_data || !config) {
        return -1;
    }
    
    config_private_data_t *data = (config_private_data_t*) self->private_data;
    
    // 更新配置
    data->config.auto_save = config->auto_save;
    data->config.auto_reload = config->auto_reload;
    
    // 更新配置文件路径
    if (data->config.config_file != default_config.config_file) {
        free(data->config.config_file);
    }
    data->config.config_file = config->config_file ? strdup(config->config_file) : NULL;
    
    printf("配置模块配置已更新\n");
    return 0;
}

// 获取配置模块配置
config_module_config_t* config_module_get_config(module_interface_t *self) {
    if (!self || !self->private_data) {
        return NULL;
    }
    
    config_private_data_t *data = (config_private_data_t*) self->private_data;
    return &data->config;
}

// 从文件加载配置
int config_load_from_file(const char *filename) {
    if (!filename) {
        return -1;
    }
    
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("无法打开配置文件: %s\n", filename);
        return -1;
    }
    
    char line[256];
    int line_count = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        line_count++;
        
        // 跳过注释和空行
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }
        
        // 解析配置行
        char *key = strtok(line, "=");
        char *value = strtok(NULL, "=");
        
        if (key && value) {
            // 去除空白字符
            char *k = key;
            while (*k == ' ' || *k == '\t') k++;
            char *end = k + strlen(k) - 1;
            while (end > k && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) end--;
            *(end + 1) = '\0';
            
            char *v = value;
            while (*v == ' ' || *v == '\t') v++;
            end = v + strlen(v) - 1;
            while (end > v && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) end--;
            *(end + 1) = '\0';
            
            // 尝试解析为不同类型的值
            if (strcmp(v, "true") == 0 || strcmp(v, "1") == 0) {
                config_set_bool(k, 1);
            } else if (strcmp(v, "false") == 0 || strcmp(v, "0") == 0) {
                config_set_bool(k, 0);
            } else if (strchr(v, '.') != NULL) {
                // 可能是浮点数
                float fval = atof(v);
                config_set_float(k, fval);
            } else {
                // 可能是整数
                int ival = atoi(v);
                if (ival != 0 || strcmp(v, "0") == 0) {
                    config_set_int(k, ival);
                } else {
                    // 可能是字符串
                    config_set_string(k, v);
                }
            }
        }
    }
    
    fclose(fp);
    printf("从文件加载配置完成: %s\n", filename);
    return 0;
}

// 保存配置到文件
int config_save_to_file(const char *filename) {
    if (!filename) {
        return -1;
    }
    
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        printf("无法创建配置文件: %s\n", filename);
        return -1;
    }
    
    fprintf(fp, "# 配置文件 - 自动生成\n");
    fprintf(fp, "# 请勿手动编辑此文件\n\n");
    
    config_item_t *item = global_config_data->items;
    while (item) {
        switch (item->type) {
            case CONFIG_TYPE_STRING:
                if (item->value.string_value) {
                    fprintf(fp, "%s=%s\n", item->key, item->value.string_value);
                }
                break;
            case CONFIG_TYPE_INT:
                fprintf(fp, "%s=%d\n", item->key, item->value.int_value);
                break;
            case CONFIG_TYPE_FLOAT:
                fprintf(fp, "%s=%.6f\n", item->key, item->value.float_value);
                break;
            case CONFIG_TYPE_BOOL:
                fprintf(fp, "%s=%s\n", item->key, item->value.bool_value ? "true" : "false");
                break;
        }
        item = item->next;
    }
    
    fclose(fp);
    printf("配置已保存到文件: %s\n", filename);
    return 0;
}

// 列出所有配置
void config_list_all(void) {
    if (!global_config_data) {
        printf("配置模块未初始化\n");
        return;
    }
    
    printf("\n=== 配置列表 ===\n");
    
    config_item_t *item = global_config_data->items;
    if (!item) {
        printf("  无配置项\n");
    } else {
        while (item) {
            printf("  %s = ", item->key);
            
            switch (item->type) {
                case CONFIG_TYPE_STRING:
                    printf("%s (string)\n", item->value.string_value ? item->value.string_value : "NULL");
                    break;
                case CONFIG_TYPE_INT:
                    printf("%d (int)\n", item->value.int_value);
                    break;
                case CONFIG_TYPE_FLOAT:
                    printf("%.6f (float)\n", item->value.float_value);
                    break;
                case CONFIG_TYPE_BOOL:
                    printf("%s (bool)\n", item->value.bool_value ? "true" : "false");
                    break;
            }
            
            item = item->next;
        }
    }
    
    printf("================\n\n");
}
