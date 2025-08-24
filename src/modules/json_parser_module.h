#ifndef JSON_PARSER_MODULE_H
#define JSON_PARSER_MODULE_H

#include <stddef.h>

// JSON值类型
typedef enum {
    JSON_TYPE_NULL,
    JSON_TYPE_BOOL,
    JSON_TYPE_NUMBER,
    JSON_TYPE_STRING,
    JSON_TYPE_ARRAY,
    JSON_TYPE_OBJECT
} json_type_t;

// JSON值结构
typedef struct json_value {
    json_type_t type;
    union {
        int bool_value;
        double number_value;
        char *string_value;
        struct json_array *array_value;
        struct json_object *object_value;
    } data;
} json_value_t;

// JSON数组结构
typedef struct json_array {
    json_value_t *values;
    size_t count;
    size_t capacity;
} json_array_t;

// JSON对象键值对结构
typedef struct json_pair {
    char *key;
    json_value_t value;
} json_pair_t;

// JSON对象结构
typedef struct json_object {
    json_pair_t *pairs;
    size_t count;
    size_t capacity;
} json_object_t;

// JSON解析器配置
typedef struct {
    int enable_unicode_escape;
    int enable_comments;
    int strict_mode;
    int max_depth;
    size_t max_string_length;
} json_parser_config_t;

// 默认配置
extern const json_parser_config_t json_parser_default_config;

// JSON解析函数
int json_parse(const char *json_string, size_t length, json_value_t **result);
int json_parse_file(const char *filename, json_value_t **result);

// JSON生成函数
char* json_stringify(const json_value_t *value);
int json_write_file(const char *filename, const json_value_t *value);

// JSON值操作函数
json_value_t* json_create_null(void);
json_value_t* json_create_bool(int value);
json_value_t* json_create_number(double value);
json_value_t* json_create_string(const char *value);
json_value_t* json_create_array(void);
json_value_t* json_create_object(void);

// JSON数组操作函数
int json_array_add(json_value_t *array, const json_value_t *value);
json_value_t* json_array_get(const json_value_t *array, size_t index);
int json_array_set(json_value_t *array, size_t index, const json_value_t *value);
size_t json_array_size(const json_value_t *array);

// JSON对象操作函数
int json_object_set(json_value_t *object, const char *key, const json_value_t *value);
json_value_t* json_object_get(const json_value_t *object, const char *key);
int json_object_has(const json_value_t *object, const char *key);
int json_object_remove(json_value_t *object, const char *key);
size_t json_object_size(const json_value_t *object);

// JSON类型检查函数
int json_is_null(const json_value_t *value);
int json_is_bool(const json_value_t *value);
int json_is_number(const json_value_t *value);
int json_is_string(const json_value_t *value);
int json_is_array(const json_value_t *value);
int json_is_object(const json_value_t *value);

// JSON值获取函数
int json_get_bool(const json_value_t *value);
double json_get_number(const json_value_t *value);
const char* json_get_string(const json_value_t *value);

// JSON内存管理函数
void json_free(json_value_t *value);
void json_free_array(json_array_t *array);
void json_free_object(json_object_t *object);

// JSON验证函数
int json_validate(const char *json_string, size_t length);
int json_is_valid(const json_value_t *value);

// JSON工具函数
json_value_t* json_clone(const json_value_t *value);
int json_merge(json_value_t *target, const json_value_t *source);
char* json_pretty_print(const json_value_t *value, int indent);

// 错误处理
typedef enum {
    JSON_ERROR_NONE = 0,
    JSON_ERROR_INVALID_SYNTAX,
    JSON_ERROR_UNEXPECTED_TOKEN,
    JSON_ERROR_UNTERMINATED_STRING,
    JSON_ERROR_INVALID_ESCAPE,
    JSON_ERROR_NUMBER_TOO_LARGE,
    JSON_ERROR_STRING_TOO_LONG,
    JSON_ERROR_DEPTH_EXCEEDED,
    JSON_ERROR_MEMORY_ALLOCATION,
    JSON_ERROR_FILE_IO
} json_error_t;

// 获取错误信息
const char* json_error_string(json_error_t error);
json_error_t json_get_last_error(void);

#endif // JSON_PARSER_MODULE_H
