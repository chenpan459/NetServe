#ifndef XML_PARSER_MODULE_H
#define XML_PARSER_MODULE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// XML节点类型枚举
typedef enum {
    XML_TYPE_ELEMENT = 0,      // 元素节点
    XML_TYPE_TEXT = 1,         // 文本节点
    XML_TYPE_ATTRIBUTE = 2,    // 属性节点
    XML_TYPE_COMMENT = 3,      // 注释节点
    XML_TYPE_CDATA = 4,        // CDATA节点
    XML_TYPE_PROCESSING = 5,   // 处理指令节点
    XML_TYPE_DOCUMENT = 6      // 文档根节点
} xml_node_type_t;

// XML属性结构
typedef struct xml_attribute {
    char *name;                 // 属性名
    char *value;                // 属性值
    struct xml_attribute *next; // 下一个属性
} xml_attribute_t;

// XML节点结构
typedef struct xml_node {
    xml_node_type_t type;      // 节点类型
    char *name;                 // 节点名称（元素名、属性名等）
    char *value;                // 节点值（文本内容、属性值等）
    
    // 节点关系
    struct xml_node *parent;    // 父节点
    struct xml_node *first_child; // 第一个子节点
    struct xml_node *last_child;  // 最后一个子节点
    struct xml_node *next_sibling; // 下一个兄弟节点
    struct xml_node *prev_sibling; // 上一个兄弟节点
    
    // 属性（仅对元素节点有效）
    xml_attribute_t *attributes;
    
    // 节点位置信息
    int line_number;            // 行号
    int column_number;          // 列号
    
    // 用户数据
    void *user_data;
} xml_node_t;

// XML文档结构
typedef struct {
    xml_node_t *root;          // 根节点
    char *version;              // XML版本
    char *encoding;             // 编码
    char *standalone;           // 独立声明
    char *doctype;              // 文档类型声明
    char *filename;             // 源文件名
} xml_document_t;

// XML解析配置
typedef struct {
    bool preserve_whitespace;   // 是否保留空白字符
    bool preserve_comments;     // 是否保留注释
    bool preserve_cdata;        // 是否保留CDATA
    bool preserve_processing;   // 是否保留处理指令
    int max_depth;              // 最大嵌套深度
    size_t max_node_count;      // 最大节点数量
    size_t max_attribute_count; // 最大属性数量
    size_t max_text_length;     // 最大文本长度
} xml_parser_config_t;

// 默认配置
extern const xml_parser_config_t xml_parser_default_config;

// 错误代码枚举
typedef enum {
    XML_ERROR_NONE = 0,
    XML_ERROR_INVALID_SYNTAX,
    XML_ERROR_UNEXPECTED_TOKEN,
    XML_ERROR_UNTERMINATED_TAG,
    XML_ERROR_UNTERMINATED_STRING,
    XML_ERROR_INVALID_ATTRIBUTE,
    XML_ERROR_DEPTH_EXCEEDED,
    XML_ERROR_NODE_LIMIT_EXCEEDED,
    XML_ERROR_MEMORY_ALLOCATION,
    XML_ERROR_FILE_IO,
    XML_ERROR_INVALID_ENCODING,
    XML_ERROR_UNKNOWN
} xml_error_t;

// 解析函数
int xml_parse_file(const char *filename, xml_document_t **document);
int xml_parse_string(const char *xml_string, size_t length, xml_document_t **document);
int xml_parse_memory(const char *buffer, size_t size, xml_document_t **document);

// 保存函数
int xml_save_file(const char *filename, const xml_document_t *document);
int xml_save_string(const xml_document_t *document, char **output_string);
int xml_save_memory(const xml_document_t *document, char **buffer, size_t *size);

// 节点操作函数
xml_node_t* xml_create_node(xml_node_type_t type, const char *name, const char *value);
xml_node_t* xml_create_element(const char *name);
xml_node_t* xml_create_text(const char *text);
xml_node_t* xml_create_attribute(const char *name, const char *value);
xml_node_t* xml_create_comment(const char *comment);
xml_node_t* xml_create_cdata(const char *cdata);

// 节点关系操作
int xml_add_child(xml_node_t *parent, xml_node_t *child);
int xml_remove_child(xml_node_t *parent, xml_node_t *child);
int xml_insert_before(xml_node_t *node, xml_node_t *new_node);
int xml_insert_after(xml_node_t *node, xml_node_t *new_node);

// 属性操作
int xml_set_attribute(xml_node_t *node, const char *name, const char *value);
int xml_get_attribute(const xml_node_t *node, const char *name, char **value);
int xml_remove_attribute(xml_node_t *node, const char *name);
int xml_has_attribute(const xml_node_t *node, const char *name);

// 节点查找函数
xml_node_t* xml_find_child(const xml_node_t *parent, const char *name);
xml_node_t* xml_find_child_by_attribute(const xml_node_t *parent, const char *name, 
                                       const char *attr_name, const char *attr_value);
xml_node_t* xml_find_node_by_path(const xml_document_t *document, const char *xpath);
xml_node_t** xml_find_all_nodes(const xml_document_t *document, const char *xpath, size_t *count);

// 节点值操作
int xml_set_text(xml_node_t *node, const char *text);
const char* xml_get_text(const xml_node_t *node);
int xml_set_name(xml_node_t *node, const char *name);
const char* xml_get_name(const xml_node_t *node);

// 节点遍历函数
xml_node_t* xml_first_child(const xml_node_t *parent);
xml_node_t* xml_last_child(const xml_node_t *parent);
xml_node_t* xml_next_sibling(const xml_node_t *node);
xml_node_t* xml_prev_sibling(const xml_node_t *node);
xml_node_t* xml_parent(const xml_node_t *node);

// 节点信息函数
int xml_get_child_count(const xml_node_t *parent);
int xml_get_attribute_count(const xml_node_t *node);
xml_node_type_t xml_get_node_type(const xml_node_t *node);
bool xml_is_element(const xml_node_t *node);
bool xml_is_text(const xml_node_t *node);
bool xml_is_attribute(const xml_node_t *node);

// 内存管理函数
void xml_free_node(xml_node_t *node);
void xml_free_document(xml_document_t *document);
void xml_free_attribute(xml_attribute_t *attr);

// 克隆函数
xml_node_t* xml_clone_node(const xml_node_t *node);
xml_document_t* xml_clone_document(const xml_document_t *document);

// 验证函数
int xml_validate_document(const xml_document_t *document);
int xml_validate_node(const xml_node_t *node);

// 格式化函数
char* xml_pretty_print(const xml_document_t *document, int indent);
char* xml_compact_print(const xml_document_t *document);

// 错误处理函数
xml_error_t xml_get_last_error(void);
const char* xml_error_string(xml_error_t error);
void xml_clear_error(void);

// 工具函数
char* xml_escape_string(const char *str);
char* xml_unescape_string(const char *str);
char* xml_normalize_whitespace(const char *str);
bool xml_is_valid_name(const char *name);
bool xml_is_valid_attribute_value(const char *value);

#ifdef __cplusplus
}
#endif

#endif // XML_PARSER_MODULE_H
