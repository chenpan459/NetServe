#include "json_parser_module.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

// 默认配置
const json_parser_config_t json_parser_default_config = {
    .enable_unicode_escape = 0,
    .enable_comments = 0,
    .strict_mode = 1,
    .max_depth = 100,
    .max_string_length = 1024 * 1024  // 1MB
};

// 解析器状态
typedef struct {
    const char *input;
    size_t length;
    size_t position;
    int depth;
    json_error_t last_error;
    json_parser_config_t config;
} json_parser_t;

// 内部函数声明
static void skip_whitespace(json_parser_t *parser);
static int parse_value(json_parser_t *parser, json_value_t **result);
static int parse_object(json_parser_t *parser, json_value_t **result);
static int parse_array(json_parser_t *parser, json_value_t **result);
static int parse_string(json_parser_t *parser, char **result);
static int parse_number(json_parser_t *parser, double *result);
static int parse_bool(json_parser_t *parser, int *result);
static int parse_null(json_parser_t *parser);
static char* unescape_string(const char *str);
static int expand_array_capacity(json_array_t *array);
static int expand_object_capacity(json_object_t *object);

// 全局错误状态
static json_error_t global_last_error = JSON_ERROR_NONE;

// JSON解析主函数
int json_parse(const char *json_string, size_t length, json_value_t **result) {
    if (!json_string || !result) {
        global_last_error = JSON_ERROR_INVALID_SYNTAX;
        return -1;
    }
    
    // 创建解析器
    json_parser_t parser = {
        .input = json_string,
        .length = length,
        .position = 0,
        .depth = 0,
        .last_error = JSON_ERROR_NONE,
        .config = json_parser_default_config
    };
    
    // 解析JSON
    int parse_result = parse_value(&parser, result);
    if (parse_result != 0) {
        global_last_error = parser.last_error;
        return -1;
    }
    
    // 跳过尾部空白字符
    skip_whitespace(&parser);
    
    // 检查是否还有剩余字符
    if (parser.position < parser.length) {
        global_last_error = JSON_ERROR_INVALID_SYNTAX;
        json_free(*result);
        *result = NULL;
        return -1;
    }
    
    global_last_error = JSON_ERROR_NONE;
    return 0;
}

// 从文件解析JSON
int json_parse_file(const char *filename, json_value_t **result) {
    if (!filename || !result) {
        global_last_error = JSON_ERROR_FILE_IO;
        return -1;
    }
    
    FILE *file = fopen(filename, "r");
    if (!file) {
        global_last_error = JSON_ERROR_FILE_IO;
        return -1;
    }
    
    // 获取文件大小
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size < 0) {
        fclose(file);
        global_last_error = JSON_ERROR_FILE_IO;
        return -1;
    }
    
    // 读取文件内容
    char *buffer = malloc(file_size + 1);
    if (!buffer) {
        fclose(file);
        global_last_error = JSON_ERROR_MEMORY_ALLOCATION;
        return -1;
    }
    
    size_t bytes_read = fread(buffer, 1, file_size, file);
    fclose(file);
    
    if (bytes_read != (size_t)file_size) {
        free(buffer);
        global_last_error = JSON_ERROR_FILE_IO;
        return -1;
    }
    
    buffer[bytes_read] = '\0';
    
    // 解析JSON
    int result_code = json_parse(buffer, bytes_read, result);
    free(buffer);
    
    return result_code;
}

// 生成JSON字符串
char* json_stringify(const json_value_t *value) {
    if (!value) {
        return strdup("null");
    }
    
    switch (value->type) {
        case JSON_TYPE_NULL:
            return strdup("null");
            
        case JSON_TYPE_BOOL:
            return strdup(value->data.bool_value ? "true" : "false");
            
        case JSON_TYPE_NUMBER: {
            char buffer[64];
            if (value->data.number_value == (int)value->data.number_value) {
                snprintf(buffer, sizeof(buffer), "%d", (int)value->data.number_value);
            } else {
                snprintf(buffer, sizeof(buffer), "%.6g", value->data.number_value);
            }
            return strdup(buffer);
        }
        
        case JSON_TYPE_STRING: {
            char *escaped = malloc(strlen(value->data.string_value) * 2 + 3);
            if (!escaped) return NULL;
            
            char *ptr = escaped;
            *ptr++ = '"';
            
            const char *src = value->data.string_value;
            while (*src) {
                switch (*src) {
                    case '"': strcpy(ptr, "\\\""); ptr += 2; break;
                    case '\\': strcpy(ptr, "\\\\"); ptr += 2; break;
                    case '\b': strcpy(ptr, "\\b"); ptr += 2; break;
                    case '\f': strcpy(ptr, "\\f"); ptr += 2; break;
                    case '\n': strcpy(ptr, "\\n"); ptr += 2; break;
                    case '\r': strcpy(ptr, "\\r"); ptr += 2; break;
                    case '\t': strcpy(ptr, "\\t"); ptr += 2; break;
                    default: *ptr++ = *src; break;
                }
                src++;
            }
            
            *ptr++ = '"';
            *ptr = '\0';
            
            return escaped;
        }
        
        case JSON_TYPE_ARRAY: {
            char *result = malloc(2);
            if (!result) return NULL;
            strcpy(result, "[");
            
            for (size_t i = 0; i < value->data.array_value->count; i++) {
                char *element_str = json_stringify(&value->data.array_value->values[i]);
                if (!element_str) continue;
                
                char *new_result = realloc(result, strlen(result) + strlen(element_str) + 2);
                if (!new_result) {
                    free(element_str);
                    continue;
                }
                
                result = new_result;
                if (i > 0) strcat(result, ",");
                strcat(result, element_str);
                free(element_str);
            }
            
            char *final_result = realloc(result, strlen(result) + 2);
            if (final_result) {
                result = final_result;
                strcat(result, "]");
            }
            
            return result;
        }
        
        case JSON_TYPE_OBJECT: {
            char *result = malloc(2);
            if (!result) return NULL;
            strcpy(result, "{");
            
            for (size_t i = 0; i < value->data.object_value->count; i++) {
                json_pair_t *pair = &value->data.object_value->pairs[i];
                char *key_str = json_stringify(json_create_string(pair->key));
                char *value_str = json_stringify(&pair->value);
                
                if (key_str && value_str) {
                    char *new_result = realloc(result, strlen(result) + strlen(key_str) + strlen(value_str) + 3);
                    if (new_result) {
                        result = new_result;
                        if (i > 0) strcat(result, ",");
                        strcat(result, key_str);
                        strcat(result, ":");
                        strcat(result, value_str);
                    }
                }
                
                if (key_str) free(key_str);
                if (value_str) free(value_str);
            }
            
            char *final_result = realloc(result, strlen(result) + 2);
            if (final_result) {
                result = final_result;
                strcat(result, "}");
            }
            
            return result;
        }
        
        default:
            return strdup("null");
    }
}

// 写入JSON到文件
int json_write_file(const char *filename, const json_value_t *value) {
    if (!filename || !value) {
        global_last_error = JSON_ERROR_FILE_IO;
        return -1;
    }
    
    char *json_string = json_stringify(value);
    if (!json_string) {
        global_last_error = JSON_ERROR_MEMORY_ALLOCATION;
        return -1;
    }
    
    FILE *file = fopen(filename, "w");
    if (!file) {
        free(json_string);
        global_last_error = JSON_ERROR_FILE_IO;
        return -1;
    }
    
    size_t written = fwrite(json_string, 1, strlen(json_string), file);
    fclose(file);
    free(json_string);
    
    if (written != strlen(json_string)) {
        global_last_error = JSON_ERROR_FILE_IO;
        return -1;
    }
    
    global_last_error = JSON_ERROR_NONE;
    return 0;
}

// JSON值创建函数

json_value_t* json_create_null(void) {
    json_value_t *value = malloc(sizeof(json_value_t));
    if (value) {
        value->type = JSON_TYPE_NULL;
    }
    return value;
}

json_value_t* json_create_bool(int bool_value) {
    json_value_t *value = malloc(sizeof(json_value_t));
    if (value) {
        value->type = JSON_TYPE_BOOL;
        value->data.bool_value = bool_value ? 1 : 0;
    }
    return value;
}

json_value_t* json_create_number(double number_value) {
    json_value_t *value = malloc(sizeof(json_value_t));
    if (value) {
        value->type = JSON_TYPE_NUMBER;
        value->data.number_value = number_value;
    }
    return value;
}

json_value_t* json_create_string(const char *string_value) {
    json_value_t *value = malloc(sizeof(json_value_t));
    if (value) {
        value->type = JSON_TYPE_STRING;
        value->data.string_value = string_value ? strdup(string_value) : NULL;
    }
    return value;
}

json_value_t* json_create_array(void) {
    json_value_t *value = malloc(sizeof(json_value_t));
    if (value) {
        value->type = JSON_TYPE_ARRAY;
        value->data.array_value = malloc(sizeof(json_array_t));
        if (value->data.array_value) {
            value->data.array_value->values = NULL;
            value->data.array_value->count = 0;
            value->data.array_value->capacity = 0;
        }
    }
    return value;
}

json_value_t* json_create_object(void) {
    json_value_t *value = malloc(sizeof(json_value_t));
    if (value) {
        value->type = JSON_TYPE_OBJECT;
        value->data.object_value = malloc(sizeof(json_object_t));
        if (value->data.object_value) {
            value->data.object_value->pairs = NULL;
            value->data.object_value->count = 0;
            value->data.object_value->capacity = 0;
        }
    }
    return value;
}

// JSON数组操作函数

int json_array_add(json_value_t *array, const json_value_t *value) {
    if (!array || array->type != JSON_TYPE_ARRAY || !value) {
        return -1;
    }
    
    json_array_t *arr = array->data.array_value;
    if (!arr) return -1;
    
    if (arr->count >= arr->capacity) {
        if (expand_array_capacity(arr) != 0) {
            return -1;
        }
    }
    
    // 克隆值
    json_value_t *cloned_value = json_clone(value);
    if (!cloned_value) return -1;
    
    arr->values[arr->count] = *cloned_value;
    arr->count++;
    
    return 0;
}

json_value_t* json_array_get(const json_value_t *array, size_t index) {
    if (!array || array->type != JSON_TYPE_ARRAY) {
        return NULL;
    }
    
    json_array_t *arr = array->data.array_value;
    if (!arr || index >= arr->count) {
        return NULL;
    }
    
    return &arr->values[index];
}

int json_array_set(json_value_t *array, size_t index, const json_value_t *value) {
    if (!array || array->type != JSON_TYPE_ARRAY || !value) {
        return -1;
    }
    
    json_array_t *arr = array->data.array_value;
    if (!arr || index >= arr->count) {
        return -1;
    }
    
    // 释放旧值
    json_free(&arr->values[index]);
    
    // 设置新值
    json_value_t *cloned_value = json_clone(value);
    if (!cloned_value) return -1;
    
    arr->values[index] = *cloned_value;
    
    return 0;
}

size_t json_array_size(const json_value_t *array) {
    if (!array || array->type != JSON_TYPE_ARRAY) {
        return 0;
    }
    
    json_array_t *arr = array->data.array_value;
    return arr ? arr->count : 0;
}

// JSON对象操作函数

int json_object_set(json_value_t *object, const char *key, const json_value_t *value) {
    if (!object || object->type != JSON_TYPE_OBJECT || !key || !value) {
        return -1;
    }
    
    json_object_t *obj = object->data.object_value;
    if (!obj) return -1;
    
    // 检查键是否已存在
    for (size_t i = 0; i < obj->count; i++) {
        if (strcmp(obj->pairs[i].key, key) == 0) {
            // 更新现有值
            json_free(&obj->pairs[i].value);
            json_value_t *cloned_value = json_clone(value);
            if (!cloned_value) return -1;
            obj->pairs[i].value = *cloned_value;
            return 0;
        }
    }
    
    // 添加新键值对
    if (obj->count >= obj->capacity) {
        if (expand_object_capacity(obj) != 0) {
            return -1;
        }
    }
    
    obj->pairs[obj->count].key = strdup(key);
    json_value_t *cloned_value = json_clone(value);
    if (!cloned_value) {
        free(obj->pairs[obj->count].key);
        return -1;
    }
    
    obj->pairs[obj->count].value = *cloned_value;
    obj->count++;
    
    return 0;
}

json_value_t* json_object_get(const json_value_t *object, const char *key) {
    if (!object || object->type != JSON_TYPE_OBJECT || !key) {
        return NULL;
    }
    
    json_object_t *obj = object->data.object_value;
    if (!obj) return NULL;
    
    for (size_t i = 0; i < obj->count; i++) {
        if (strcmp(obj->pairs[i].key, key) == 0) {
            return &obj->pairs[i].value;
        }
    }
    
    return NULL;
}

int json_object_has(const json_value_t *object, const char *key) {
    return json_object_get(object, key) != NULL;
}

int json_object_remove(json_value_t *object, const char *key) {
    if (!object || object->type != JSON_TYPE_OBJECT || !key) {
        return -1;
    }
    
    json_object_t *obj = object->data.object_value;
    if (!obj) return -1;
    
    for (size_t i = 0; i < obj->count; i++) {
        if (strcmp(obj->pairs[i].key, key) == 0) {
            // 释放键值对
            free(obj->pairs[i].key);
            json_free(&obj->pairs[i].value);
            
            // 移动后面的元素
            for (size_t j = i; j < obj->count - 1; j++) {
                obj->pairs[j] = obj->pairs[j + 1];
            }
            
            obj->count--;
            return 0;
        }
    }
    
    return -1;
}

size_t json_object_size(const json_value_t *object) {
    if (!object || object->type != JSON_TYPE_OBJECT) {
        return 0;
    }
    
    json_object_t *obj = object->data.object_value;
    return obj ? obj->count : 0;
}

// JSON类型检查函数

int json_is_null(const json_value_t *value) {
    return value && value->type == JSON_TYPE_NULL;
}

int json_is_bool(const json_value_t *value) {
    return value && value->type == JSON_TYPE_BOOL;
}

int json_is_number(const json_value_t *value) {
    return value && value->type == JSON_TYPE_NUMBER;
}

int json_is_string(const json_value_t *value) {
    return value && value->type == JSON_TYPE_STRING;
}

int json_is_array(const json_value_t *value) {
    return value && value->type == JSON_TYPE_ARRAY;
}

int json_is_object(const json_value_t *value) {
    return value && value->type == JSON_TYPE_OBJECT;
}

// JSON值获取函数

int json_get_bool(const json_value_t *value) {
    if (json_is_bool(value)) {
        return value->data.bool_value;
    }
    return 0;
}

double json_get_number(const json_value_t *value) {
    if (json_is_number(value)) {
        return value->data.number_value;
    }
    return 0.0;
}

const char* json_get_string(const json_value_t *value) {
    if (json_is_string(value)) {
        return value->data.string_value;
    }
    return NULL;
}

// JSON内存管理函数

void json_free(json_value_t *value) {
    if (!value) return;
    
    switch (value->type) {
        case JSON_TYPE_STRING:
            if (value->data.string_value) {
                free(value->data.string_value);
            }
            break;
            
        case JSON_TYPE_ARRAY:
            if (value->data.array_value) {
                json_free_array(value->data.array_value);
            }
            break;
            
        case JSON_TYPE_OBJECT:
            if (value->data.object_value) {
                json_free_object(value->data.object_value);
            }
            break;
            
        default:
            break;
    }
    
    free(value);
}

void json_free_array(json_array_t *array) {
    if (!array) return;
    
    for (size_t i = 0; i < array->count; i++) {
        json_free(&array->values[i]);
    }
    
    if (array->values) {
        free(array->values);
    }
    
    free(array);
}

void json_free_object(json_object_t *object) {
    if (!object) return;
    
    for (size_t i = 0; i < object->count; i++) {
        free(object->pairs[i].key);
        json_free(&object->pairs[i].value);
    }
    
    if (object->pairs) {
        free(object->pairs);
    }
    
    free(object);
}

// JSON验证函数

int json_validate(const char *json_string, size_t length) {
    json_value_t *dummy;
    int result = json_parse(json_string, length, &dummy);
    if (result == 0) {
        json_free(dummy);
        return 1;
    }
    return 0;
}

int json_is_valid(const json_value_t *value) {
    return value != NULL;
}

// JSON工具函数

json_value_t* json_clone(const json_value_t *value) {
    if (!value) return NULL;
    
    json_value_t *cloned = malloc(sizeof(json_value_t));
    if (!cloned) return NULL;
    
    cloned->type = value->type;
    
    switch (value->type) {
        case JSON_TYPE_NULL:
            break;
            
        case JSON_TYPE_BOOL:
            cloned->data.bool_value = value->data.bool_value;
            break;
            
        case JSON_TYPE_NUMBER:
            cloned->data.number_value = value->data.number_value;
            break;
            
        case JSON_TYPE_STRING:
            cloned->data.string_value = value->data.string_value ? strdup(value->data.string_value) : NULL;
            break;
            
        case JSON_TYPE_ARRAY:
            cloned->data.array_value = malloc(sizeof(json_array_t));
            if (cloned->data.array_value) {
                json_array_t *src_array = value->data.array_value;
                json_array_t *dst_array = cloned->data.array_value;
                
                dst_array->capacity = src_array->count;
                dst_array->count = src_array->count;
                dst_array->values = malloc(dst_array->count * sizeof(json_value_t));
                
                if (dst_array->values) {
                    for (size_t i = 0; i < dst_array->count; i++) {
                        dst_array->values[i] = *json_clone(&src_array->values[i]);
                    }
                }
            }
            break;
            
        case JSON_TYPE_OBJECT:
            cloned->data.object_value = malloc(sizeof(json_object_t));
            if (cloned->data.object_value) {
                json_object_t *src_obj = value->data.object_value;
                json_object_t *dst_obj = cloned->data.object_value;
                
                dst_obj->capacity = src_obj->count;
                dst_obj->count = src_obj->count;
                dst_obj->pairs = malloc(dst_obj->count * sizeof(json_pair_t));
                
                if (dst_obj->pairs) {
                    for (size_t i = 0; i < dst_obj->count; i++) {
                        dst_obj->pairs[i].key = strdup(src_obj->pairs[i].key);
                        dst_obj->pairs[i].value = *json_clone(&src_obj->pairs[i].value);
                    }
                }
            }
            break;
    }
    
    return cloned;
}

int json_merge(json_value_t *target, const json_value_t *source) {
    if (!target || !source || target->type != source->type) {
        return -1;
    }
    
    if (source->type == JSON_TYPE_OBJECT) {
        json_object_t *src_obj = source->data.object_value;
        if (src_obj) {
            for (size_t i = 0; i < src_obj->count; i++) {
                json_object_set(target, src_obj->pairs[i].key, &src_obj->pairs[i].value);
            }
        }
    }
    
    return 0;
}

char* json_pretty_print(const json_value_t *value, int indent) {
    (void)indent; // 避免未使用参数警告
    // 简化实现，返回格式化的JSON字符串
    return json_stringify(value);
}

// 错误处理函数

const char* json_error_string(json_error_t error) {
    switch (error) {
        case JSON_ERROR_NONE: return "No error";
        case JSON_ERROR_INVALID_SYNTAX: return "Invalid JSON syntax";
        case JSON_ERROR_UNEXPECTED_TOKEN: return "Unexpected token";
        case JSON_ERROR_UNTERMINATED_STRING: return "Unterminated string";
        case JSON_ERROR_INVALID_ESCAPE: return "Invalid escape sequence";
        case JSON_ERROR_NUMBER_TOO_LARGE: return "Number too large";
        case JSON_ERROR_STRING_TOO_LONG: return "String too long";
        case JSON_ERROR_DEPTH_EXCEEDED: return "Maximum depth exceeded";
        case JSON_ERROR_MEMORY_ALLOCATION: return "Memory allocation failed";
        case JSON_ERROR_FILE_IO: return "File I/O error";
        default: return "Unknown error";
    }
}

json_error_t json_get_last_error(void) {
    return global_last_error;
}

// 内部函数实现

static void skip_whitespace(json_parser_t *parser) {
    while (parser->position < parser->length && 
           isspace(parser->input[parser->position])) {
        parser->position++;
    }
}

static int parse_value(json_parser_t *parser, json_value_t **result) {
    skip_whitespace(parser);
    
    if (parser->position >= parser->length) {
        parser->last_error = JSON_ERROR_UNEXPECTED_TOKEN;
        return -1;
    }
    
    char c = parser->input[parser->position];
    
    switch (c) {
        case '{':
            return parse_object(parser, result);
            
        case '[':
            return parse_array(parser, result);
            
        case '"': {
            char *str_result;
            int parse_result = parse_string(parser, &str_result);
            if (parse_result == 0) {
                *result = json_create_string(str_result);
                free(str_result);
                return *result ? 0 : -1;
            }
            return parse_result;
        }
            
        case 't':
        case 'f': {
            int bool_result;
            int parse_result = parse_bool(parser, &bool_result);
            if (parse_result == 0) {
                *result = json_create_bool(bool_result);
                return *result ? 0 : -1;
            }
            return parse_result;
        }
            
        case 'n':
            return parse_null(parser);
            
        case '-':
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9': {
            double num_result;
            int parse_result = parse_number(parser, &num_result);
            if (parse_result == 0) {
                *result = json_create_number(num_result);
                return *result ? 0 : -1;
            }
            return parse_result;
        }
            
        default:
            parser->last_error = JSON_ERROR_UNEXPECTED_TOKEN;
            return -1;
    }
}

static int parse_object(json_parser_t *parser, json_value_t **result) {
    if (parser->depth >= parser->config.max_depth) {
        parser->last_error = JSON_ERROR_DEPTH_EXCEEDED;
        return -1;
    }
    
    parser->depth++;
    parser->position++; // 跳过 '{'
    
    *result = json_create_object();
    if (!*result) {
        parser->last_error = JSON_ERROR_MEMORY_ALLOCATION;
        parser->depth--;
        return -1;
    }
    
    skip_whitespace(parser);
    
    if (parser->position < parser->length && parser->input[parser->position] == '}') {
        parser->position++;
        parser->depth--;
        return 0;
    }
    
    while (parser->position < parser->length) {
        skip_whitespace(parser);
        
        if (parser->position >= parser->length) {
            parser->last_error = JSON_ERROR_UNTERMINATED_STRING;
            parser->depth--;
            return -1;
        }
        
        if (parser->input[parser->position] != '"') {
            parser->last_error = JSON_ERROR_UNEXPECTED_TOKEN;
            parser->depth--;
            return -1;
        }
        
        char *key;
        if (parse_string(parser, &key) != 0) {
            parser->depth--;
            return -1;
        }
        
        skip_whitespace(parser);
        
        if (parser->position >= parser->length || parser->input[parser->position] != ':') {
            free(key);
            parser->last_error = JSON_ERROR_UNEXPECTED_TOKEN;
            parser->depth--;
            return -1;
        }
        
        parser->position++; // 跳过 ':'
        
        json_value_t *value;
        if (parse_value(parser, &value) != 0) {
            free(key);
            parser->depth--;
            return -1;
        }
        
        json_object_set(*result, key, value);
        free(key);
        json_free(value);
        
        skip_whitespace(parser);
        
        if (parser->position >= parser->length) {
            parser->last_error = JSON_ERROR_UNEXPECTED_TOKEN;
            parser->depth--;
            return -1;
        }
        
        if (parser->input[parser->position] == '}') {
            parser->position++;
            parser->depth--;
            return 0;
        }
        
        if (parser->input[parser->position] != ',') {
            parser->last_error = JSON_ERROR_UNEXPECTED_TOKEN;
            parser->depth--;
            return -1;
        }
        
        parser->position++; // 跳过 ','
    }
    
    parser->last_error = JSON_ERROR_UNTERMINATED_STRING;
    parser->depth--;
    return -1;
}

static int parse_array(json_parser_t *parser, json_value_t **result) {
    if (parser->depth >= parser->config.max_depth) {
        parser->last_error = JSON_ERROR_DEPTH_EXCEEDED;
        return -1;
    }
    
    parser->depth++;
    parser->position++; // 跳过 '['
    
    *result = json_create_array();
    if (!*result) {
        parser->last_error = JSON_ERROR_MEMORY_ALLOCATION;
        parser->depth--;
        return -1;
    }
    
    skip_whitespace(parser);
    
    if (parser->position < parser->length && parser->input[parser->position] == ']') {
        parser->position++;
        parser->depth--;
        return 0;
    }
    
    while (parser->position < parser->length) {
        json_value_t *value;
        if (parse_value(parser, &value) != 0) {
            parser->depth--;
            return -1;
        }
        
        json_array_add(*result, value);
        json_free(value);
        
        skip_whitespace(parser);
        
        if (parser->position >= parser->length) {
            parser->last_error = JSON_ERROR_UNEXPECTED_TOKEN;
            parser->depth--;
            return -1;
        }
        
        if (parser->input[parser->position] == ']') {
            parser->position++;
            parser->depth--;
            return 0;
        }
        
        if (parser->input[parser->position] != ',') {
            parser->last_error = JSON_ERROR_UNEXPECTED_TOKEN;
            parser->depth--;
            return -1;
        }
        
        parser->position++; // 跳过 ','
    }
    
    parser->last_error = JSON_ERROR_UNTERMINATED_STRING;
    parser->depth--;
    return -1;
}

static int parse_string(json_parser_t *parser, char **result) {
    parser->position++; // 跳过开始的引号
    
    size_t start = parser->position;
    
    while (parser->position < parser->length) {
        char c = parser->input[parser->position];
        
        if (c == '"') {
            // 找到字符串结束
            size_t length = parser->position - start;
            char *str = malloc(length + 1);
            if (!str) {
                parser->last_error = JSON_ERROR_MEMORY_ALLOCATION;
                return -1;
            }
            
            strncpy(str, parser->input + start, length);
            str[length] = '\0';
            
            *result = unescape_string(str);
            free(str);
            
            parser->position++; // 跳过结束引号
            return 0;
        }
        
        if (c == '\\') {
            parser->position++;
            if (parser->position >= parser->length) {
                parser->last_error = JSON_ERROR_INVALID_ESCAPE;
                return -1;
            }
        }
        
        parser->position++;
    }
    
    parser->last_error = JSON_ERROR_UNTERMINATED_STRING;
    return -1;
}

static int parse_number(json_parser_t *parser, double *result) {
    char *endptr;
    *result = strtod(parser->input + parser->position, &endptr);
    
    if (endptr == parser->input + parser->position) {
        parser->last_error = JSON_ERROR_UNEXPECTED_TOKEN;
        return -1;
    }
    
    parser->position += (endptr - (parser->input + parser->position));
    return 0;
}

static int parse_bool(json_parser_t *parser, int *result) {
    if (parser->position + 3 < parser->length && 
        strncmp(parser->input + parser->position, "true", 4) == 0) {
        *result = 1;
        parser->position += 4;
        return 0;
    }
    
    if (parser->position + 4 < parser->length && 
        strncmp(parser->input + parser->position, "false", 5) == 0) {
        *result = 0;
        parser->position += 5;
        return 0;
    }
    
    parser->last_error = JSON_ERROR_UNEXPECTED_TOKEN;
    return -1;
}

static int parse_null(json_parser_t *parser) {
    if (parser->position + 3 < parser->length && 
        strncmp(parser->input + parser->position, "null", 4) == 0) {
        parser->position += 4;
        return 0;
    }
    
    parser->last_error = JSON_ERROR_UNEXPECTED_TOKEN;
    return -1;
}

static char* unescape_string(const char *str) {
    char *result = malloc(strlen(str) + 1);
    if (!result) return NULL;
    
    char *dst = result;
    const char *src = str;
    
    while (*src) {
        if (*src == '\\' && src[1]) {
            src++;
            switch (*src) {
                case '"': *dst++ = '"'; break;
                case '\\': *dst++ = '\\'; break;
                case '/': *dst++ = '/'; break;
                case 'b': *dst++ = '\b'; break;
                case 'f': *dst++ = '\f'; break;
                case 'n': *dst++ = '\n'; break;
                case 'r': *dst++ = '\r'; break;
                case 't': *dst++ = '\t'; break;
                default: *dst++ = *src; break;
            }
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    
    *dst = '\0';
    return result;
}

static int expand_array_capacity(json_array_t *array) {
    size_t new_capacity = array->capacity == 0 ? 8 : array->capacity * 2;
    json_value_t *new_values = realloc(array->values, new_capacity * sizeof(json_value_t));
    
    if (!new_values) return -1;
    
    array->values = new_values;
    array->capacity = new_capacity;
    return 0;
}

static int expand_object_capacity(json_object_t *object) {
    size_t new_capacity = object->capacity == 0 ? 8 : object->capacity * 2;
    json_pair_t *new_pairs = realloc(object->pairs, new_capacity * sizeof(json_pair_t));
    
    if (!new_pairs) return -1;
    
    object->pairs = new_pairs;
    object->capacity = new_capacity;
    return 0;
}
