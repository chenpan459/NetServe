#ifndef DATABASE_MODULE_H
#define DATABASE_MODULE_H

#include <stdint.h>
#include <stdbool.h>
#include <uv.h>
#include "src/modules/module_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

// 数据库类型枚举
typedef enum {
    DB_TYPE_SQLITE = 0,
    DB_TYPE_MYSQL = 1,
    DB_TYPE_POSTGRESQL = 2,
    DB_TYPE_UNKNOWN = 255
} database_type_t;

// 数据库连接状态
typedef enum {
    DB_CONN_DISCONNECTED = 0,
    DB_CONN_CONNECTING = 1,
    DB_CONN_CONNECTED = 2,
    DB_CONN_ERROR = 3
} database_connection_status_t;

// 查询结果行结构
typedef struct db_row {
    char **columns;           // 列名数组
    char **values;            // 值数组
    int column_count;         // 列数
    struct db_row *next;      // 下一行
} db_row_t;

// 查询结果结构
typedef struct {
    db_row_t *rows;          // 行数组
    int row_count;            // 行数
    int affected_rows;        // 受影响的行数
    char *last_error;         // 最后的错误信息
} db_result_t;

// 数据库连接结构
typedef struct {
    void *connection;         // 数据库连接句柄
    database_type_t type;     // 数据库类型
    database_connection_status_t status; // 连接状态
    char *host;               // 主机地址
    int port;                 // 端口
    char *database;           // 数据库名
    char *username;           // 用户名
    char *password;           // 密码
    char *last_error;         // 最后的错误信息
    int timeout;              // 连接超时时间（秒）
} database_connection_t;

// 数据库配置结构
typedef struct {
    database_type_t type;     // 数据库类型
    char *host;               // 主机地址
    int port;                 // 端口
    char *database;           // 数据库名
    char *username;           // 用户名
    char *password;           // 密码
    int timeout;              // 连接超时时间（秒）
    int max_connections;      // 最大连接数
    bool enable_pooling;      // 是否启用连接池
} database_config_t;

// 数据库模块结构
typedef struct {
    // 继承自 module_interface_t
    const char *name;         // 模块名称
    const char *version;      // 模块版本
    
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
    
    // 数据库模块特有字段
    const char *description;  // 模块描述
    
    // 配置函数
    int (*set_config)(const void *config);
} database_module_t;

// 数据库操作函数声明

// 连接管理
database_connection_t* db_connect(const database_config_t *config);
int db_disconnect(database_connection_t *conn);
int db_ping(database_connection_t *conn);
bool db_is_connected(database_connection_t *conn);

// 查询执行
db_result_t* db_execute_query(database_connection_t *conn, const char *sql);
db_result_t* db_execute_prepared(database_connection_t *conn, const char *sql, ...);
int db_execute_update(database_connection_t *conn, const char *sql);
int db_execute_batch(database_connection_t *conn, const char **sql_array, int count);

// 事务管理
int db_begin_transaction(database_connection_t *conn);
int db_commit_transaction(database_connection_t *conn);
int db_rollback_transaction(database_connection_t *conn);

// 结果集操作
void db_free_result(db_result_t *result);
int db_get_row_count(db_result_t *result);
int db_get_column_count(db_result_t *result);
const char* db_get_column_name(db_result_t *result, int column_index);
const char* db_get_value(db_result_t *result, int row_index, int column_index);
const char* db_get_value_by_name(db_result_t *result, int row_index, const char *column_name);

// 连接池管理
int db_pool_init(const database_config_t *config, int initial_size, int max_size);
int db_pool_get_connection(database_connection_t **conn);
int db_pool_return_connection(database_connection_t *conn);
void db_pool_cleanup(void);

// 错误处理
const char* db_get_last_error(database_connection_t *conn);
int db_get_last_error_code(database_connection_t *conn);
void db_clear_error(database_connection_t *conn);

// 工具函数
char* db_escape_string(database_connection_t *conn, const char *str);
char* db_quote_identifier(database_connection_t *conn, const char *identifier);
bool db_table_exists(database_connection_t *conn, const char *table_name);
int db_get_table_count(database_connection_t *conn, const char *table_name);

// 模块实例声明
extern const database_module_t database_module;

// 兼容性声明
extern const struct module_interface database_module_interface;

#ifdef __cplusplus
}
#endif

#endif // DATABASE_MODULE_H
