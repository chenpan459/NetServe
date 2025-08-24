#include "xml_parser_module.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

// 默认配置
const xml_parser_config_t xml_parser_default_config = {
    .preserve_whitespace = false,
    .preserve_comments = true,
    .preserve_cdata = true,
    .preserve_processing = true,
    .max_depth = 100,
    .max_node_count = 10000,
    .max_attribute_count = 1000,
    .max_text_length = 1024 * 1024  // 1MB
};

// 解析器状态
typedef struct {
    const char *input;
    size_t length;
    size_t position;
    int depth;
    int line_number;
    int column_number;
    size_t node_count;
    size_t attribute_count;
    xml_error_t last_error;
    xml_parser_config_t config;
} xml_parser_t;

// 全局错误状态
static xml_error_t global_last_error = XML_ERROR_NONE;

// 内部函数声明
static void skip_whitespace(xml_parser_t *parser);
static int parse_document(xml_parser_t *parser, xml_document_t **document);
static int parse_element(xml_parser_t *parser, xml_node_t **node);
static int parse_start_tag(xml_parser_t *parser, xml_node_t **node);
static int parse_end_tag(xml_parser_t *parser, const char *expected_name);
static int parse_attributes(xml_parser_t *parser, xml_node_t *node);
static int parse_attribute(xml_parser_t *parser, xml_attribute_t **attr);
static int parse_text(xml_parser_t *parser, xml_node_t **node);
static int parse_comment(xml_parser_t *parser, xml_node_t **node);
static int parse_cdata(xml_parser_t *parser, xml_node_t **node);
static int parse_processing_instruction(xml_parser_t *parser, xml_node_t **node);
static int parse_declaration(xml_parser_t *parser, xml_document_t *document);
static char* parse_quoted_string(xml_parser_t *parser);
static char* parse_name(xml_parser_t *parser);
static void set_parser_error(xml_parser_t *parser, xml_error_t error);
static void update_position(xml_parser_t *parser, char c);

// 内存管理函数
static xml_node_t* create_node(xml_node_type_t type, const char *name, const char *value);
static xml_attribute_t* create_attribute(const char *name, const char *value);
static void free_node_recursive(xml_node_t *node);
static void free_attributes(xml_attribute_t *attr);

// 字符串处理函数
static char* strdup_safe(const char *str);
static char* escape_xml_string(const char *str);
static char* unescape_xml_string(const char *str);
static bool is_valid_xml_name(const char *name);
static bool is_valid_attribute_value(const char *value);

// XML解析主函数
int xml_parse_file(const char *filename, xml_document_t **document) {
    if (!filename || !document) {
        global_last_error = XML_ERROR_FILE_IO;
        return -1;
    }
    
    FILE *file = fopen(filename, "r");
    if (!file) {
        global_last_error = XML_ERROR_FILE_IO;
        return -1;
    }
    
    // 获取文件大小
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size < 0) {
        fclose(file);
        global_last_error = XML_ERROR_FILE_IO;
        return -1;
    }
    
    // 读取文件内容
    char *buffer = malloc(file_size + 1);
    if (!buffer) {
        fclose(file);
        global_last_error = XML_ERROR_MEMORY_ALLOCATION;
        return -1;
    }
    
    size_t bytes_read = fread(buffer, 1, file_size, file);
    fclose(file);
    
    if (bytes_read != (size_t)file_size) {
        free(buffer);
        global_last_error = XML_ERROR_FILE_IO;
        return -1;
    }
    
    buffer[bytes_read] = '\0';
    
    // 解析XML
    int result_code = xml_parse_string(buffer, bytes_read, document);
    if (result_code == 0 && *document) {
        (*document)->filename = strdup_safe(filename);
    }
    
    free(buffer);
    return result_code;
}

int xml_parse_string(const char *xml_string, size_t length, xml_document_t **document) {
    if (!xml_string || !document) {
        global_last_error = XML_ERROR_INVALID_SYNTAX;
        return -1;
    }
    
    // 创建解析器
    xml_parser_t parser = {
        .input = xml_string,
        .length = length,
        .position = 0,
        .depth = 0,
        .line_number = 1,
        .column_number = 1,
        .node_count = 0,
        .attribute_count = 0,
        .last_error = XML_ERROR_NONE,
        .config = xml_parser_default_config
    };
    
    // 解析XML文档
    int parse_result = parse_document(&parser, document);
    if (parse_result != 0) {
        global_last_error = parser.last_error;
        return -1;
    }
    
    global_last_error = XML_ERROR_NONE;
    return 0;
}

int xml_parse_memory(const char *buffer, size_t size, xml_document_t **document) {
    return xml_parse_string(buffer, size, document);
}

// 保存函数实现
int xml_save_file(const char *filename, const xml_document_t *document) {
    if (!filename || !document) {
        global_last_error = XML_ERROR_FILE_IO;
        return -1;
    }
    
    char *xml_string = xml_pretty_print(document, 2);
    if (!xml_string) {
        global_last_error = XML_ERROR_MEMORY_ALLOCATION;
        return -1;
    }
    
    FILE *file = fopen(filename, "w");
    if (!file) {
        free(xml_string);
        global_last_error = XML_ERROR_FILE_IO;
        return -1;
    }
    
    size_t xml_length = strlen(xml_string);
    size_t written = fwrite(xml_string, 1, xml_length, file);
    fclose(file);
    free(xml_string);
    
    if (written != xml_length) {
        global_last_error = XML_ERROR_FILE_IO;
        return -1;
    }
    
    global_last_error = XML_ERROR_NONE;
    return 0;
}

int xml_save_string(const xml_document_t *document, char **output_string) {
    if (!document || !output_string) {
        global_last_error = XML_ERROR_INVALID_SYNTAX;
        return -1;
    }
    
    *output_string = xml_pretty_print(document, 2);
    if (!*output_string) {
        global_last_error = XML_ERROR_MEMORY_ALLOCATION;
        return -1;
    }
    
    global_last_error = XML_ERROR_NONE;
    return 0;
}

int xml_save_memory(const xml_document_t *document, char **buffer, size_t *size) {
    if (!document || !buffer || !size) {
        global_last_error = XML_ERROR_INVALID_SYNTAX;
        return -1;
    }
    
    *buffer = xml_pretty_print(document, 2);
    if (!*buffer) {
        global_last_error = XML_ERROR_MEMORY_ALLOCATION;
        return -1;
    }
    
    *size = strlen(*buffer);
    global_last_error = XML_ERROR_NONE;
    return 0;
}

// 节点创建函数
xml_node_t* xml_create_node(xml_node_type_t type, const char *name, const char *value) {
    return create_node(type, name, value);
}

xml_node_t* xml_create_element(const char *name) {
    return create_node(XML_TYPE_ELEMENT, name, NULL);
}

xml_node_t* xml_create_text(const char *text) {
    return create_node(XML_TYPE_TEXT, NULL, text);
}

xml_node_t* xml_create_attribute(const char *name, const char *value) {
    return create_node(XML_TYPE_ATTRIBUTE, name, value);
}

xml_node_t* xml_create_comment(const char *comment) {
    return create_node(XML_TYPE_COMMENT, NULL, comment);
}

xml_node_t* xml_create_cdata(const char *cdata) {
    return create_node(XML_TYPE_CDATA, NULL, cdata);
}

// 节点关系操作
int xml_add_child(xml_node_t *parent, xml_node_t *child) {
    if (!parent || !child || parent->type != XML_TYPE_ELEMENT) {
        return -1;
    }
    
    // 设置父子关系
    child->parent = parent;
    
    if (!parent->first_child) {
        parent->first_child = child;
        parent->last_child = child;
        child->prev_sibling = NULL;
        child->next_sibling = NULL;
    } else {
        child->prev_sibling = parent->last_child;
        child->next_sibling = NULL;
        parent->last_child->next_sibling = child;
        parent->last_child = child;
    }
    
    return 0;
}

int xml_remove_child(xml_node_t *parent, xml_node_t *child) {
    if (!parent || !child || child->parent != parent) {
        return -1;
    }
    
    // 更新兄弟节点关系
    if (child->prev_sibling) {
        child->prev_sibling->next_sibling = child->next_sibling;
    } else {
        parent->first_child = child->next_sibling;
    }
    
    if (child->next_sibling) {
        child->next_sibling->prev_sibling = child->prev_sibling;
    } else {
        parent->last_child = child->prev_sibling;
    }
    
    // 清除子节点关系
    child->parent = NULL;
    child->prev_sibling = NULL;
    child->next_sibling = NULL;
    
    return 0;
}

// 属性操作
int xml_set_attribute(xml_node_t *node, const char *name, const char *value) {
    if (!node || !name || node->type != XML_TYPE_ELEMENT) {
        return -1;
    }
    
    // 检查属性是否已存在
    xml_attribute_t *attr = node->attributes;
    while (attr) {
        if (strcmp(attr->name, name) == 0) {
            // 更新现有属性值
            if (attr->value) free(attr->value);
            attr->value = strdup_safe(value);
            return 0;
        }
        attr = attr->next;
    }
    
    // 创建新属性
    xml_attribute_t *new_attr = create_attribute(name, value);
    if (!new_attr) return -1;
    
    // 添加到属性列表
    new_attr->next = node->attributes;
    node->attributes = new_attr;
    
    return 0;
}

int xml_get_attribute(const xml_node_t *node, const char *name, char **value) {
    if (!node || !name || !value || node->type != XML_TYPE_ELEMENT) {
        return -1;
    }
    
    xml_attribute_t *attr = node->attributes;
    while (attr) {
        if (strcmp(attr->name, name) == 0) {
            *value = strdup_safe(attr->value);
            return 0;
        }
        attr = attr->next;
    }
    
    *value = NULL;
    return -1;
}

// 节点查找函数
xml_node_t* xml_find_child(const xml_node_t *parent, const char *name) {
    if (!parent || !name) return NULL;
    
    xml_node_t *child = parent->first_child;
    while (child) {
        if (child->type == XML_TYPE_ELEMENT && strcmp(child->name, name) == 0) {
            return child;
        }
        child = child->next_sibling;
    }
    
    return NULL;
}

// 节点值操作
int xml_set_text(xml_node_t *node, const char *text) {
    if (!node) return -1;
    
    if (node->value) {
        free(node->value);
    }
    node->value = strdup_safe(text);
    return 0;
}

const char* xml_get_text(const xml_node_t *node) {
    if (!node) return NULL;
    
    if (node->type == XML_TYPE_TEXT || node->type == XML_TYPE_CDATA) {
        return node->value;
    }
    
    // 对于元素节点，返回第一个文本子节点的内容
    if (node->type == XML_TYPE_ELEMENT) {
        xml_node_t *child = node->first_child;
        while (child) {
            if (child->type == XML_TYPE_TEXT || child->type == XML_TYPE_CDATA) {
                return child->value;
            }
            child = child->next_sibling;
        }
    }
    
    return NULL;
}

// 内存管理函数
void xml_free_node(xml_node_t *node) {
    if (node) {
        free_node_recursive(node);
    }
}

void xml_free_document(xml_document_t *document) {
    if (!document) return;
    
    if (document->root) {
        xml_free_node(document->root);
    }
    
    if (document->version) free(document->version);
    if (document->encoding) free(document->encoding);
    if (document->standalone) free(document->standalone);
    if (document->doctype) free(document->doctype);
    if (document->filename) free(document->filename);
    
    free(document);
}

// 格式化函数
char* xml_pretty_print(const xml_document_t *document, int indent) {
    if (!document || !document->root) return NULL;
    
    // 这里实现格式化输出，简化版本
    char *result = malloc(1024);
    if (!result) return NULL;
    
    snprintf(result, 1024, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    
    // 递归格式化节点
    char *node_str = format_node_pretty(document->root, indent, 0);
    if (node_str) {
        size_t len = strlen(result) + strlen(node_str) + 1;
        char *new_result = realloc(result, len);
        if (new_result) {
            result = new_result;
            strcat(result, node_str);
            free(node_str);
        }
    }
    
    return result;
}

// 错误处理函数
xml_error_t xml_get_last_error(void) {
    return global_last_error;
}

const char* xml_error_string(xml_error_t error) {
    switch (error) {
        case XML_ERROR_NONE: return "No error";
        case XML_ERROR_INVALID_SYNTAX: return "Invalid XML syntax";
        case XML_ERROR_UNEXPECTED_TOKEN: return "Unexpected token";
        case XML_ERROR_UNTERMINATED_TAG: return "Unterminated tag";
        case XML_ERROR_UNTERMINATED_STRING: return "Unterminated string";
        case XML_ERROR_INVALID_ATTRIBUTE: return "Invalid attribute";
        case XML_ERROR_DEPTH_EXCEEDED: return "Maximum depth exceeded";
        case XML_ERROR_NODE_LIMIT_EXCEEDED: return "Maximum node count exceeded";
        case XML_ERROR_MEMORY_ALLOCATION: return "Memory allocation failed";
        case XML_ERROR_FILE_IO: return "File I/O error";
        case XML_ERROR_INVALID_ENCODING: return "Invalid encoding";
        default: return "Unknown error";
    }
}

void xml_clear_error(void) {
    global_last_error = XML_ERROR_NONE;
}

// 工具函数
char* xml_escape_string(const char *str) {
    if (!str) return NULL;
    return escape_xml_string(str);
}

char* xml_unescape_string(const char *str) {
    if (!str) return NULL;
    return unescape_xml_string(str);
}

bool xml_is_valid_name(const char *name) {
    return is_valid_xml_name(name);
}

bool xml_is_valid_attribute_value(const char *value) {
    return is_valid_attribute_value(value);
}

// 内部函数实现
static void skip_whitespace(xml_parser_t *parser) {
    while (parser->position < parser->length && 
           isspace(parser->input[parser->position])) {
        update_position(parser, parser->input[parser->position]);
        parser->position++;
    }
}

static int parse_document(xml_parser_t *parser, xml_document_t **document) {
    // 创建文档结构
    *document = calloc(1, sizeof(xml_document_t));
    if (!*document) {
        set_parser_error(parser, XML_ERROR_MEMORY_ALLOCATION);
        return -1;
    }
    
    // 跳过BOM和空白
    skip_whitespace(parser);
    
    // 解析XML声明
    if (parser->position < parser->length && 
        parser->input[parser->position] == '<' &&
        parser->position + 1 < parser->length &&
        parser->input[parser->position + 1] == '?') {
        if (parse_declaration(parser, *document) != 0) {
            return -1;
        }
    }
    
    // 跳过空白
    skip_whitespace(parser);
    
    // 解析根元素
    xml_node_t *root_node;
    if (parse_element(parser, &root_node) != 0) {
        return -1;
    }
    
    (*document)->root = root_node;
    
    // 跳过尾部空白
    skip_whitespace(parser);
    
    // 检查是否还有剩余内容
    if (parser->position < parser->length) {
        set_parser_error(parser, XML_ERROR_INVALID_SYNTAX);
        return -1;
    }
    
    return 0;
}

static int parse_element(xml_parser_t *parser, xml_node_t **node) {
    if (parser->depth >= parser->config.max_depth) {
        set_parser_error(parser, XML_ERROR_DEPTH_EXCEEDED);
        return -1;
    }
    
    parser->depth++;
    
    // 解析开始标签
    if (parse_start_tag(parser, node) != 0) {
        parser->depth--;
        return -1;
    }
    
    // 检查自闭合标签
    if (parser->position < parser->length && 
        parser->input[parser->position - 1] == '/') {
        parser->depth--;
        return 0;
    }
    
    // 解析子节点
    while (parser->position < parser->length) {
        skip_whitespace(parser);
        
        if (parser->position >= parser->length) {
            set_parser_error(parser, XML_ERROR_UNTERMINATED_TAG);
            parser->depth--;
            return -1;
        }
        
        char c = parser->input[parser->position];
        
        if (c == '<') {
            if (parser->position + 1 < parser->length) {
                char next_c = parser->input[parser->position + 1];
                
                if (next_c == '/') {
                    // 结束标签
                    if (parse_end_tag(parser, (*node)->name) != 0) {
                        parser->depth--;
                        return -1;
                    }
                    break;
                } else if (next_c == '!') {
                    // 注释或CDATA
                    if (parser->position + 2 < parser->length &&
                        parser->input[parser->position + 2] == '-') {
                        xml_node_t *comment_node;
                        if (parse_comment(parser, &comment_node) == 0) {
                            xml_add_child(*node, comment_node);
                        }
                    } else if (parser->position + 7 < parser->length &&
                               strncmp(parser->input + parser->position + 1, "![CDATA[", 8) == 0) {
                        xml_node_t *cdata_node;
                        if (parse_cdata(parser, &cdata_node) == 0) {
                            xml_add_child(*node, cdata_node);
                        }
                    }
                } else if (next_c == '?') {
                    // 处理指令
                    xml_node_t *pi_node;
                    if (parse_processing_instruction(parser, &pi_node) == 0) {
                        xml_add_child(*node, pi_node);
                    }
                } else {
                    // 子元素
                    xml_node_t *child_node;
                    if (parse_element(parser, &child_node) == 0) {
                        xml_add_child(*node, child_node);
                    }
                }
            }
        } else {
            // 文本内容
            xml_node_t *text_node;
            if (parse_text(parser, &text_node) == 0) {
                xml_add_child(*node, text_node);
            }
        }
    }
    
    parser->depth--;
    return 0;
}

static int parse_start_tag(xml_parser_t *parser, xml_node_t **node) {
    parser->position++; // 跳过 '<'
    
    // 解析元素名
    char *element_name = parse_name(parser);
    if (!element_name) {
        set_parser_error(parser, XML_ERROR_INVALID_SYNTAX);
        return -1;
    }
    
    // 创建元素节点
    *node = create_node(XML_TYPE_ELEMENT, element_name, NULL);
    if (!*node) {
        free(element_name);
        set_parser_error(parser, XML_ERROR_MEMORY_ALLOCATION);
        return -1;
    }
    
    free(element_name);
    
    // 设置位置信息
    (*node)->line_number = parser->line_number;
    (*node)->column_number = parser->column_number;
    
    // 解析属性
    if (parse_attributes(parser, *node) != 0) {
        return -1;
    }
    
    // 查找结束字符
    while (parser->position < parser->length) {
        char c = parser->input[parser->position];
        if (c == '>' || c == '/') {
            parser->position++;
            break;
        }
        parser->position++;
    }
    
    return 0;
}

static int parse_attributes(xml_parser_t *parser, xml_node_t *node) {
    while (parser->position < parser->length) {
        skip_whitespace(parser);
        
        if (parser->position >= parser->length) break;
        
        char c = parser->input[parser->position];
        if (c == '>' || c == '/') break;
        
        // 解析属性
        xml_attribute_t *attr;
        if (parse_attribute(parser, &attr) == 0) {
            // 添加到节点
            attr->next = node->attributes;
            node->attributes = attr;
        }
    }
    
    return 0;
}

static int parse_attribute(xml_parser_t *parser, xml_attribute_t **attr) {
    // 解析属性名
    char *attr_name = parse_name(parser);
    if (!attr_name) return -1;
    
    skip_whitespace(parser);
    
    // 查找等号
    if (parser->position >= parser->length || 
        parser->input[parser->position] != '=') {
        free(attr_name);
        return -1;
    }
    parser->position++;
    
    skip_whitespace(parser);
    
    // 解析属性值
    char *attr_value = parse_quoted_string(parser);
    if (!attr_value) {
        free(attr_name);
        return -1;
    }
    
    // 创建属性
    *attr = create_attribute(attr_name, attr_value);
    free(attr_name);
    free(attr_value);
    
    return *attr ? 0 : -1;
}

static char* parse_quoted_string(xml_parser_t *parser) {
    if (parser->position >= parser->length) return NULL;
    
    char quote = parser->input[parser->position];
    if (quote != '"' && quote != '\'') return NULL;
    
    parser->position++;
    size_t start = parser->position;
    
    while (parser->position < parser->length && 
           parser->input[parser->position] != quote) {
        parser->position++;
    }
    
    if (parser->position >= parser->length) return NULL;
    
    size_t length = parser->position - start;
    char *str = malloc(length + 1);
    if (!str) return NULL;
    
    strncpy(str, parser->input + start, length);
    str[length] = '\0';
    
    parser->position++; // 跳过结束引号
    return str;
}

static char* parse_name(xml_parser_t *parser) {
    size_t start = parser->position;
    
    while (parser->position < parser->length) {
        char c = parser->input[parser->position];
        if (!isalnum(c) && c != '_' && c != '-' && c != ':' && c != '.') {
            break;
        }
        parser->position++;
    }
    
    size_t length = parser->position - start;
    if (length == 0) return NULL;
    
    char *name = malloc(length + 1);
    if (!name) return NULL;
    
    strncpy(name, parser->input + start, length);
    name[length] = '\0';
    
    return name;
}

static int parse_end_tag(xml_parser_t *parser, const char *expected_name) {
    parser->position += 2; // 跳过 '</'
    
    char *tag_name = parse_name(parser);
    if (!tag_name) {
        set_parser_error(parser, XML_ERROR_INVALID_SYNTAX);
        return -1;
    }
    
    // 检查标签名是否匹配
    if (strcmp(tag_name, expected_name) != 0) {
        free(tag_name);
        set_parser_error(parser, XML_ERROR_UNTERMINATED_TAG);
        return -1;
    }
    
    free(tag_name);
    
    // 查找结束字符
    while (parser->position < parser->length && 
           parser->input[parser->position] != '>') {
        parser->position++;
    }
    
    if (parser->position < parser->length) {
        parser->position++; // 跳过 '>'
    }
    
    return 0;
}

static int parse_text(xml_parser_t *parser, xml_node_t **node) {
    size_t start = parser->position;
    
    while (parser->position < parser->length) {
        char c = parser->input[parser->position];
        if (c == '<') break;
        parser->position++;
    }
    
    size_t length = parser->position - start;
    if (length == 0) return -1;
    
    char *text = malloc(length + 1);
    if (!text) return -1;
    
    strncpy(text, parser->input + start, length);
    text[length] = '\0';
    
    // 创建文本节点
    *node = create_node(XML_TYPE_TEXT, NULL, text);
    free(text);
    
    return *node ? 0 : -1;
}

static int parse_comment(xml_parser_t *parser, xml_node_t **node) {
    parser->position += 4; // 跳过 '<!--'
    
    size_t start = parser->position;
    
    while (parser->position < parser->length) {
        if (parser->position + 2 < parser->length &&
            parser->input[parser->position] == '-' &&
            parser->input[parser->position + 1] == '-' &&
            parser->input[parser->position + 2] == '>') {
            break;
        }
        parser->position++;
    }
    
    size_t length = parser->position - start;
    char *comment = malloc(length + 1);
    if (!comment) return -1;
    
    strncpy(comment, parser->input + start, length);
    comment[length] = '\0';
    
    *node = create_node(XML_TYPE_COMMENT, NULL, comment);
    free(comment);
    
    parser->position += 3; // 跳过 '-->'
    
    return *node ? 0 : -1;
}

static int parse_cdata(xml_parser_t *parser, xml_node_t **node) {
    parser->position += 9; // 跳过 '<![CDATA['
    
    size_t start = parser->position;
    
    while (parser->position < parser->length) {
        if (parser->position + 2 < parser->length &&
            parser->input[parser->position] == ']' &&
            parser->input[parser->position + 1] == ']' &&
            parser->input[parser->position + 2] == '>') {
            break;
        }
        parser->position++;
    }
    
    size_t length = parser->position - start;
    char *cdata = malloc(length + 1);
    if (!cdata) return -1;
    
    strncpy(cdata, parser->input + start, length);
    cdata[length] = '\0';
    
    *node = create_node(XML_TYPE_CDATA, NULL, cdata);
    free(cdata);
    
    parser->position += 3; // 跳过 ']]>'
    
    return *node ? 0 : -1;
}

static int parse_processing_instruction(xml_parser_t *parser, xml_node_t **node) {
    parser->position += 2; // 跳过 '<?'
    
    char *target = parse_name(parser);
    if (!target) return -1;
    
    skip_whitespace(parser);
    
    size_t start = parser->position;
    
    while (parser->position < parser->length) {
        if (parser->position + 1 < parser->length &&
            parser->input[parser->position] == '?' &&
            parser->input[parser->position + 1] == '>') {
            break;
        }
        parser->position++;
    }
    
    size_t length = parser->position - start;
    char *data = malloc(length + 1);
    if (!data) {
        free(target);
        return -1;
    }
    
    strncpy(data, parser->input + start, length);
    data[length] = '\0';
    
    *node = create_node(XML_TYPE_PROCESSING, target, data);
    free(target);
    free(data);
    
    parser->position += 2; // 跳过 '?>'
    
    return *node ? 0 : -1;
}

static int parse_declaration(xml_parser_t *parser, xml_document_t *document) {
    parser->position += 2; // 跳过 '<?'
    
    // 跳过 'xml'
    if (parser->position + 2 < parser->length &&
        strncmp(parser->input + parser->position, "xml", 3) == 0) {
        parser->position += 3;
    }
    
    skip_whitespace(parser);
    
    // 解析版本
    if (parser->position + 6 < parser->length &&
        strncmp(parser->input + parser->position, "version", 7) == 0) {
        parser->position += 7;
        skip_whitespace(parser);
        
        if (parser->position < parser->length && 
            parser->input[parser->position] == '=') {
            parser->position++;
            skip_whitespace(parser);
            
            char *version = parse_quoted_string(parser);
            if (version) {
                document->version = version;
            }
        }
    }
    
    // 解析编码
    skip_whitespace(parser);
    if (parser->position + 7 < parser->length &&
        strncmp(parser->input + parser->position, "encoding", 8) == 0) {
        parser->position += 8;
        skip_whitespace(parser);
        
        if (parser->position < parser->length && 
            parser->input[parser->position] == '=') {
            parser->position++;
            skip_whitespace(parser);
            
            char *encoding = parse_quoted_string(parser);
            if (encoding) {
                document->encoding = encoding;
            }
        }
    }
    
    // 解析独立声明
    skip_whitespace(parser);
    if (parser->position + 9 < parser->length &&
        strncmp(parser->input + parser->position, "standalone", 10) == 0) {
        parser->position += 10;
        skip_whitespace(parser);
        
        if (parser->position < parser->length && 
            parser->input[parser->position] == '=') {
            parser->position++;
            skip_whitespace(parser);
            
            char *standalone = parse_quoted_string(parser);
            if (standalone) {
                document->standalone = standalone;
            }
        }
    }
    
    // 查找结束
    while (parser->position < parser->length) {
        if (parser->position + 1 < parser->length &&
            parser->input[parser->position] == '?' &&
            parser->input[parser->position + 1] == '>') {
            parser->position += 2;
            break;
        }
        parser->position++;
    }
    
    return 0;
}

// 内存管理函数
static xml_node_t* create_node(xml_node_type_t type, const char *name, const char *value) {
    xml_node_t *node = calloc(1, sizeof(xml_node_t));
    if (!node) return NULL;
    
    node->type = type;
    node->name = strdup_safe(name);
    node->value = strdup_safe(value);
    
    return node;
}

static xml_attribute_t* create_attribute(const char *name, const char *value) {
    xml_attribute_t *attr = calloc(1, sizeof(xml_attribute_t));
    if (!attr) return NULL;
    
    attr->name = strdup_safe(name);
    attr->value = strdup_safe(value);
    
    return attr;
}

static void free_node_recursive(xml_node_t *node) {
    if (!node) return;
    
    // 释放子节点
    xml_node_t *child = node->first_child;
    while (child) {
        xml_node_t *next = child->next_sibling;
        free_node_recursive(child);
        child = next;
    }
    
    // 释放属性
    free_attributes(node->attributes);
    
    // 释放节点本身
    if (node->name) free(node->name);
    if (node->value) free(node->value);
    free(node);
}

static void free_attributes(xml_attribute_t *attr) {
    while (attr) {
        xml_attribute_t *next = attr->next;
        if (attr->name) free(attr->name);
        if (attr->value) free(attr->value);
        free(attr);
        attr = next;
    }
}

// 字符串处理函数
static char* strdup_safe(const char *str) {
    return str ? strdup(str) : NULL;
}

static char* escape_xml_string(const char *str) {
    if (!str) return NULL;
    
    size_t len = strlen(str);
    size_t escaped_len = len;
    
    // 计算转义后的长度
    for (size_t i = 0; i < len; i++) {
        switch (str[i]) {
            case '&': escaped_len += 4; break;  // &amp;
            case '<': escaped_len += 3; break;  // &lt;
            case '>': escaped_len += 3; break;  // &gt;
            case '"': escaped_len += 5; break;  // &quot;
            case '\'': escaped_len += 5; break; // &apos;
        }
    }
    
    char *escaped = malloc(escaped_len + 1);
    if (!escaped) return NULL;
    
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        switch (str[i]) {
            case '&': strcpy(escaped + j, "&amp;"); j += 5; break;
            case '<': strcpy(escaped + j, "&lt;"); j += 4; break;
            case '>': strcpy(escaped + j, "&gt;"); j += 4; break;
            case '"': strcpy(escaped + j, "&quot;"); j += 6; break;
            case '\'': strcpy(escaped + j, "&apos;"); j += 6; break;
            default: escaped[j++] = str[i]; break;
        }
    }
    
    escaped[j] = '\0';
    return escaped;
}

static char* unescape_xml_string(const char *str) {
    if (!str) return NULL;
    
    size_t len = strlen(str);
    char *unescaped = malloc(len + 1);
    if (!unescaped) return NULL;
    
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if (str[i] == '&' && i + 1 < len) {
            if (strncmp(str + i, "&amp;", 5) == 0) {
                unescaped[j++] = '&';
                i += 4;
            } else if (strncmp(str + i, "&lt;", 4) == 0) {
                unescaped[j++] = '<';
                i += 3;
            } else if (strncmp(str + i, "&gt;", 4) == 0) {
                unescaped[j++] = '>';
                i += 3;
            } else if (strncmp(str + i, "&quot;", 6) == 0) {
                unescaped[j++] = '"';
                i += 5;
            } else if (strncmp(str + i, "&apos;", 6) == 0) {
                unescaped[j++] = '\'';
                i += 5;
            } else {
                unescaped[j++] = str[i];
            }
        } else {
            unescaped[j++] = str[i];
        }
    }
    
    unescaped[j] = '\0';
    return unescaped;
}

static bool is_valid_xml_name(const char *name) {
    if (!name || !*name) return false;
    
    // 第一个字符必须是字母或下划线
    if (!isalpha(name[0]) && name[0] != '_') return false;
    
    // 后续字符可以是字母、数字、下划线、连字符、冒号、点
    for (size_t i = 1; i < strlen(name); i++) {
        if (!isalnum(name[i]) && name[i] != '_' && name[i] != '-' && 
            name[i] != ':' && name[i] != '.') {
            return false;
        }
    }
    
    return true;
}

static bool is_valid_attribute_value(const char *value) {
    if (!value) return true; // 空值是有效的
    
    // 检查是否包含未转义的字符
    for (size_t i = 0; i < strlen(value); i++) {
        if (value[i] == '<' || value[i] == '&') {
            return false;
        }
    }
    
    return true;
}

// 解析器辅助函数
static void set_parser_error(xml_parser_t *parser, xml_error_t error) {
    parser->last_error = error;
}

static void update_position(xml_parser_t *parser, char c) {
    if (c == '\n') {
        parser->line_number++;
        parser->column_number = 1;
    } else {
        parser->column_number++;
    }
}

// 格式化辅助函数（简化实现）
static char* format_node_pretty(const xml_node_t *node, int indent, int level) {
    if (!node) return NULL;
    
    char *result = malloc(1024);
    if (!result) return NULL;
    
    char indent_str[256] = "";
    for (int i = 0; i < level * indent; i++) {
        indent_str[i] = ' ';
    }
    indent_str[level * indent] = '\0';
    
    switch (node->type) {
        case XML_TYPE_ELEMENT: {
            snprintf(result, 1024, "%s<%s", indent_str, node->name);
            
            // 添加属性
            xml_attribute_t *attr = node->attributes;
            while (attr) {
                char *escaped_value = escape_xml_string(attr->value);
                char *new_result = realloc(result, strlen(result) + strlen(attr->name) + strlen(escaped_value) + 10);
                if (new_result) {
                    result = new_result;
                    strcat(result, " ");
                    strcat(result, attr->name);
                    strcat(result, "=\"");
                    strcat(result, escaped_value);
                    strcat(result, "\"");
                }
                if (escaped_value) free(escaped_value);
                attr = attr->next;
            }
            
            if (node->first_child) {
                strcat(result, ">\n");
                
                // 添加子节点
                xml_node_t *child = node->first_child;
                while (child) {
                    char *child_str = format_node_pretty(child, indent, level + 1);
                    if (child_str) {
                        char *new_result = realloc(result, strlen(result) + strlen(child_str) + 1);
                        if (new_result) {
                            result = new_result;
                            strcat(result, child_str);
                        }
                        free(child_str);
                    }
                    child = child->next_sibling;
                }
                
                strcat(result, indent_str);
                strcat(result, "</");
                strcat(result, node->name);
                strcat(result, ">\n");
            } else {
                strcat(result, "/>\n");
            }
            break;
        }
        
        case XML_TYPE_TEXT: {
            char *escaped_text = escape_xml_string(node->value);
            snprintf(result, 1024, "%s%s", indent_str, escaped_text);
            if (escaped_text) free(escaped_text);
            break;
        }
        
        case XML_TYPE_COMMENT: {
            snprintf(result, 1024, "%s<!--%s-->\n", indent_str, node->value);
            break;
        }
        
        case XML_TYPE_CDATA: {
            snprintf(result, 1024, "%s<![CDATA[%s]]>\n", indent_str, node->value);
            break;
        }
        
        default:
            snprintf(result, 1024, "%s<!-- unknown node type -->\n", indent_str);
            break;
    }
    
    return result;
}
