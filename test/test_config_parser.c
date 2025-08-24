#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/modules/config_module.h"

int main() {
    printf("=== 配置解析器注释功能测试 ===\n\n");
    
    // 初始化配置模块
    module_interface_t config_mod = config_module;
    if (config_module_init(&config_mod, NULL) != 0) {
        printf("配置模块初始化失败\n");
        return 1;
    }
    
    // 启动配置模块
    if (config_module_start(&config_mod) != 0) {
        printf("配置模块启动失败\n");
        return 1;
    }
    
    // 测试加载配置文件
    printf("加载测试配置文件...\n");
    if (config_load_from_file("config/test_config.ini") != 0) {
        printf("加载配置文件失败\n");
        return 1;
    }
    
    printf("\n=== 配置项测试 ===\n");
    
    // 测试基本配置项
    int network_port = config_get_int("network_port", -1);
    printf("network_port: %d\n", network_port);
    
    int http_port = config_get_int("http_port", -1);
    printf("http_port: %d\n", http_port);
    
    int enhanced_port = config_get_int("enhanced_network_port", -1);
    printf("enhanced_network_port: %d\n", enhanced_port);
    
    // 测试被注释掉的配置项
    int disabled_setting = config_get_int("disabled_setting", -999);
    printf("disabled_setting (应该返回默认值): %d\n", disabled_setting);
    
    // 测试行内注释的配置项
    int test_value = config_get_int("test_value", -1);
    printf("test_value: %d\n", test_value);
    
    int another_test = config_get_int("another_test", -1);
    printf("another_test: %d\n", another_test);
    
    // 测试字符串配置项
    const char* server_name = config_get_string("server_name", "default");
    printf("server_name: %s\n", server_name);
    
    // 测试布尔配置项
    int debug_mode = config_get_bool("server_debug_mode", -1);
    printf("server_debug_mode: %d\n", debug_mode);
    
    printf("\n=== 所有配置项列表 ===\n");
    config_list_all();
    
    // 清理配置模块
    config_module_cleanup(&config_mod);
    
    printf("\n测试完成！\n");
    return 0;
}
