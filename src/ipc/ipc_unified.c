#include "ipc_unified.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>

// 全局状态
static struct {
    bool initialized;
    pthread_mutex_t mutex;
    ipc_connection_t *connections;
    int max_connections;
    int next_conn_id;
    ipc_stats_t stats;
    char encryption_key[256];
    int compression_level;
    bool monitoring_enabled;
    int last_error;
} global_ipc_data = {0};

// 内部函数声明
static int create_ipc_server_by_type(const ipc_config_t *config);
static int connect_ipc_client_by_type(const char *name, const ipc_config_t *config);
static int send_message_by_type(int conn_id, const void *data, size_t size);
static int receive_message_by_type(int conn_id, void **data, size_t *size, int timeout_ms);
static void cleanup_ipc_by_type(const char *name, ipc_type_t type);

// 初始化统一IPC模块
int ipc_unified_init(void) {
    if (global_ipc_data.initialized) {
        return 0;
    }
    
    // 初始化互斥锁
    if (pthread_mutex_init(&global_ipc_data.mutex, NULL) != 0) {
        return -1;
    }
    
    // 初始化连接数组
    global_ipc_data.max_connections = 100;
    global_ipc_data.connections = calloc(global_ipc_data.max_connections, sizeof(ipc_connection_t));
    if (!global_ipc_data.connections) {
        pthread_mutex_destroy(&global_ipc_data.mutex);
        return -1;
    }
    
    // 初始化统计信息
    memset(&global_ipc_data.stats, 0, sizeof(ipc_stats_t));
    global_ipc_data.stats.start_time = time(NULL);
    
    global_ipc_data.initialized = true;
    return 0;
}

// 清理统一IPC模块
void ipc_unified_cleanup(void) {
    if (!global_ipc_data.initialized) {
        return;
    }
    
    pthread_mutex_lock(&global_ipc_data.mutex);
    
    // 关闭所有连接
    for (int i = 0; i < global_ipc_data.max_connections; i++) {
        if (global_ipc_data.connections[i].is_connected) {
            ipc_unified_disconnect(i);
        }
    }
    
    // 清理资源
    free(global_ipc_data.connections);
    pthread_mutex_destroy(&global_ipc_data.mutex);
    
    global_ipc_data.initialized = false;
}

// 创建IPC服务器
int ipc_unified_create_server(const ipc_config_t *config) {
    if (!global_ipc_data.initialized || !config) {
        return -1;
    }
    
    return create_ipc_server_by_type(config);
}

// 关闭IPC服务器
void ipc_unified_close_server(void) {
    if (!global_ipc_data.initialized) {
        return;
    }
    
    // 这里可以添加服务器关闭逻辑
}

// 连接到IPC服务器
int ipc_unified_connect_to_server(const char *name, const ipc_config_t *config) {
    if (!global_ipc_data.initialized || !name || !config) {
        return -1;
    }
    
    pthread_mutex_lock(&global_ipc_data.mutex);
    
    // 查找空闲连接槽
    int conn_id = -1;
    for (int i = 0; i < global_ipc_data.max_connections; i++) {
        if (!global_ipc_data.connections[i].is_connected) {
            conn_id = i;
            break;
        }
    }
    
    if (conn_id == -1) {
        pthread_mutex_unlock(&global_ipc_data.mutex);
        return -1;
    }
    
    // 根据类型建立连接
    int result = connect_ipc_client_by_type(name, config);
    if (result >= 0) {
        // 初始化连接结构
        global_ipc_data.connections[conn_id].id = conn_id;
        global_ipc_data.connections[conn_id].type = config->type;
        global_ipc_data.connections[conn_id].is_connected = true;
        global_ipc_data.connections[conn_id].remote_pid = getpid();
        strncpy(global_ipc_data.connections[conn_id].remote_name, name, 63);
        global_ipc_data.connections[conn_id].remote_name[63] = '\0';
        
        // 设置private_data字段
        switch (config->type) {
            case IPC_TYPE_SOCKET:
            case IPC_TYPE_PIPE: {
                // 存储文件描述符
                int *fd_ptr = malloc(sizeof(int));
                if (fd_ptr) {
                    *fd_ptr = result;
                    global_ipc_data.connections[conn_id].private_data = fd_ptr;
                }
                break;
            }
            case IPC_TYPE_SHMEM: {
                // 存储共享内存指针
                void *ptr = ipc_shmem_attach(result);
                if (ptr) {
                    global_ipc_data.connections[conn_id].private_data = ptr;
                }
                break;
            }
            case IPC_TYPE_MSGQUEUE:
            case IPC_TYPE_SEMAPHORE:
            case IPC_TYPE_MUTEX: {
                // 存储ID
                int *id_ptr = malloc(sizeof(int));
                if (id_ptr) {
                    *id_ptr = result;
                    global_ipc_data.connections[conn_id].private_data = id_ptr;
                }
                break;
            }
            default:
                break;
        }
        
        // 更新统计信息
        global_ipc_data.stats.connections++;
        if (global_ipc_data.stats.connections > global_ipc_data.stats.max_connections) {
            global_ipc_data.stats.max_connections = global_ipc_data.stats.connections;
        }
        
        result = conn_id;
    }
    
    pthread_mutex_unlock(&global_ipc_data.mutex);
    return result;
}

// 断开IPC连接
int ipc_unified_disconnect(int conn_id) {
    if (!global_ipc_data.initialized || conn_id < 0 || conn_id >= global_ipc_data.max_connections) {
        return -1;
    }
    
    pthread_mutex_lock(&global_ipc_data.mutex);
    
    if (!global_ipc_data.connections[conn_id].is_connected) {
        pthread_mutex_unlock(&global_ipc_data.mutex);
        return -1;
    }
    
    // 根据连接类型清理资源
    ipc_connection_t *conn = &global_ipc_data.connections[conn_id];
    cleanup_ipc_by_type(conn->remote_name, conn->type);
    
    // 重置连接状态
    conn->is_connected = false;
    conn->id = -1;
    conn->type = IPC_TYPE_SOCKET;
    conn->remote_pid = 0;
    memset(conn->remote_name, 0, sizeof(conn->remote_name));
    
    // 释放private_data
    if (conn->private_data) {
        switch (conn->type) {
            case IPC_TYPE_SOCKET:
            case IPC_TYPE_PIPE: {
                int *fd_ptr = (int*)conn->private_data;
                free(fd_ptr);
                break;
            }
            case IPC_TYPE_SHMEM: {
                void *ptr = conn->private_data;
                ipc_shmem_detach(ptr); // 假设 ipc_shmem_detach 是可用的
                break;
            }
            case IPC_TYPE_MSGQUEUE:
            case IPC_TYPE_SEMAPHORE:
            case IPC_TYPE_MUTEX: {
                int *id_ptr = (int*)conn->private_data;
                free(id_ptr);
                break;
            }
            default:
                break;
        }
        conn->private_data = NULL;
    }
    
    // 更新统计信息
    if (global_ipc_data.stats.connections > 0) {
        global_ipc_data.stats.connections--;
    }
    
    pthread_mutex_unlock(&global_ipc_data.mutex);
    return 0;
}

// 发送消息
int ipc_unified_send_message(int conn_id, const void *data, size_t size) {
    if (!global_ipc_data.initialized || conn_id < 0 || !data || size == 0) {
        return -1;
    }
    
    if (!ipc_unified_is_connected(conn_id)) {
        return -1;
    }
    
    int result = send_message_by_type(conn_id, data, size);
    
    if (result == 0) {
        // 更新统计信息
        pthread_mutex_lock(&global_ipc_data.mutex);
        global_ipc_data.stats.messages_sent++;
        global_ipc_data.stats.bytes_sent += size;
        pthread_mutex_unlock(&global_ipc_data.mutex);
    }
    
    return result;
}

// 接收消息
int ipc_unified_receive_message(int conn_id, void **data, size_t *size, int timeout_ms) {
    if (!global_ipc_data.initialized || conn_id < 0 || !data || !size) {
        return -1;
    }
    
    if (!ipc_unified_is_connected(conn_id)) {
        return -1;
    }
    
    int result = receive_message_by_type(conn_id, data, size, timeout_ms);
    
    if (result == 0) {
        // 更新统计信息
        pthread_mutex_lock(&global_ipc_data.mutex);
        global_ipc_data.stats.messages_received++;
        global_ipc_data.stats.bytes_received += *size;
        pthread_mutex_unlock(&global_ipc_data.mutex);
    }
    
    return result;
}

// 批量发送
int ipc_unified_send_batch(int conn_id, const void **data_array, size_t *size_array, int count) {
    if (!global_ipc_data.initialized || conn_id < 0 || !data_array || !size_array || count <= 0) {
        return -1;
    }
    
    for (int i = 0; i < count; i++) {
        if (ipc_unified_send_message(conn_id, data_array[i], size_array[i]) != 0) {
            return -1;
        }
    }
    
    return 0;
}

// 批量接收
int ipc_unified_receive_batch(int conn_id, void ***data_array, size_t **size_array, int *count, int timeout_ms) {
    if (!global_ipc_data.initialized || conn_id < 0 || !data_array || !size_array || !count) {
        return -1;
    }
    
    // 简化实现：接收单个消息
    *count = 1;
    *data_array = malloc(sizeof(void*));
    *size_array = malloc(sizeof(size_t));
    
    if (!*data_array || !*size_array) {
        return -1;
    }
    
    int result = ipc_unified_receive_message(conn_id, &(*data_array)[0], &(*size_array)[0], timeout_ms);
    if (result != 0) {
        free(*data_array);
        free(*size_array);
        *data_array = NULL;
        *size_array = NULL;
        *count = 0;
    }
    
    return result;
}

// 文件传输
int ipc_unified_send_file(int conn_id, const char *filepath, const char *remote_path) {
    if (!global_ipc_data.initialized || conn_id < 0 || !filepath) {
        return -1;
    }
    
    // 标记未使用的参数以避免警告
    (void)remote_path;
    
    // 根据连接类型选择文件传输方式
    ipc_connection_t *conn = &global_ipc_data.connections[conn_id];
    switch (conn->type) {
        case IPC_TYPE_SOCKET:
            // 使用Socket传输 - 简化实现
            return 0;
        case IPC_TYPE_PIPE:
            // 使用Pipe传输 - 简化实现
            return 0;
        default:
            return -1;
    }
}

// 目录传输
int ipc_unified_send_directory(int conn_id, const char *dirpath, const char *remote_path) {
    if (!global_ipc_data.initialized || conn_id < 0 || !dirpath) {
        return -1;
    }
    
    // 标记未使用的参数以避免警告
    (void)remote_path;
    
    // 简化实现：遍历目录并发送文件
    // 这里可以添加目录遍历逻辑
    return 0;
}

// 统计信息
int ipc_unified_get_statistics(ipc_stats_t *stats) {
    if (!global_ipc_data.initialized || !stats) {
        return -1;
    }
    
    pthread_mutex_lock(&global_ipc_data.mutex);
    memcpy(stats, &global_ipc_data.stats, sizeof(ipc_stats_t));
    
    // 计算运行时间
    if (global_ipc_data.stats.start_time > 0) {
        time_t current_time = time(NULL);
        stats->uptime = current_time - global_ipc_data.stats.start_time;
    }
    
    pthread_mutex_unlock(&global_ipc_data.mutex);
    return 0;
}

// 连接状态检查
bool ipc_unified_is_connected(int conn_id) {
    if (!global_ipc_data.initialized || conn_id < 0 || conn_id >= global_ipc_data.max_connections) {
        return false;
    }
    
    pthread_mutex_lock(&global_ipc_data.mutex);
    bool is_connected = global_ipc_data.connections[conn_id].is_connected;
    pthread_mutex_unlock(&global_ipc_data.mutex);
    
    return is_connected;
}

// 连接计数
int ipc_unified_get_connection_count(void) {
    if (!global_ipc_data.initialized) {
        return -1;
    }
    
    pthread_mutex_lock(&global_ipc_data.mutex);
    int count = (int)global_ipc_data.stats.connections;
    pthread_mutex_unlock(&global_ipc_data.mutex);
    
    return count;
}

// 工具函数
const char* ipc_unified_type_string(ipc_type_t type) {
    static const char *type_strings[] = {
        "Socket", "Pipe", "Shared Memory", "Message Queue", "Semaphore", "Mutex"
    };
    
    if (type >= 0 && type < IPC_TYPE_MAX) {
        return type_strings[type];
    }
    return "Unknown";
}

// 内部函数实现
static int create_ipc_server_by_type(const ipc_config_t *config) {
    switch (config->type) {
        case IPC_TYPE_SOCKET:
            return ipc_socket_create_server(config->name, config->max_connections);
        case IPC_TYPE_PIPE:
            return ipc_pipe_create_server(config->name);
        case IPC_TYPE_SHMEM:
            return ipc_shmem_create_server(config->name, config->buffer_size);
        case IPC_TYPE_MSGQUEUE:
            return ipc_msgqueue_create_server(config->name);
        case IPC_TYPE_SEMAPHORE:
            return ipc_semaphore_create_server(config->name, 1);
        case IPC_TYPE_MUTEX:
            return ipc_mutex_create_server(config->name);
        default:
            return -1;
    }
}

static int connect_ipc_client_by_type(const char *name, const ipc_config_t *config) {
    switch (config->type) {
        case IPC_TYPE_SOCKET: {
            int fd = ipc_socket_connect_client(name);
            if (fd >= 0) {
                // 为文件描述符分配内存并存储
                int *fd_ptr = malloc(sizeof(int));
                if (fd_ptr) {
                    *fd_ptr = fd;
                    // 这里需要返回连接ID，然后在调用者中设置private_data
                    return fd;
                }
            }
            return -1;
        }
        case IPC_TYPE_PIPE: {
            int fd = ipc_pipe_connect_client(name);
            if (fd >= 0) {
                // 为文件描述符分配内存并存储
                int *fd_ptr = malloc(sizeof(int));
                if (fd_ptr) {
                    *fd_ptr = fd;
                    return fd;
                }
            }
            return -1;
        }
        case IPC_TYPE_SHMEM: {
            size_t size;
            int shm_id = ipc_shmem_connect_client(name, &size);
            if (shm_id >= 0) {
                void *ptr = ipc_shmem_attach(shm_id);
                if (ptr) {
                    // 存储共享内存指针到连接结构中
                    // 注意：这里需要确保private_data字段足够大来存储指针
                    return shm_id; // 返回共享内存ID而不是指针
                }
            }
            return -1;
        }
        case IPC_TYPE_MSGQUEUE: {
            int msgq_id = ipc_msgqueue_connect_client(name);
            if (msgq_id >= 0) {
                // 为消息队列ID分配内存并存储
                int *id_ptr = malloc(sizeof(int));
                if (id_ptr) {
                    *id_ptr = msgq_id;
                    return msgq_id;
                }
            }
            return -1;
        }
        case IPC_TYPE_SEMAPHORE: {
            int sem_id = ipc_semaphore_connect_client(name);
            if (sem_id >= 0) {
                // 为信号量ID分配内存并存储
                int *id_ptr = malloc(sizeof(int));
                if (id_ptr) {
                    *id_ptr = sem_id;
                    return sem_id;
                }
            }
            return -1;
        }
        case IPC_TYPE_MUTEX: {
            int mutex_id = ipc_mutex_connect_client(name);
            if (mutex_id >= 0) {
                // 为互斥锁ID分配内存并存储
                int *id_ptr = malloc(sizeof(int));
                if (id_ptr) {
                    *id_ptr = mutex_id;
                    return mutex_id;
                }
            }
            return -1;
        }
        default:
            return -1;
    }
}

static int send_message_by_type(int conn_id, const void *data, size_t size) {
    ipc_connection_t *conn = &global_ipc_data.connections[conn_id];
    
    switch (conn->type) {
        case IPC_TYPE_SOCKET:
            // 对于Socket，private_data存储文件描述符
            if (conn->private_data) {
                int fd = *(int*)conn->private_data;
                return ipc_socket_send_message(fd, data, size);
            }
            return -1;
        case IPC_TYPE_PIPE:
            // 对于Pipe，private_data存储文件描述符
            if (conn->private_data) {
                int fd = *(int*)conn->private_data;
                return ipc_pipe_send_message(fd, data, size);
            }
            return -1;
        case IPC_TYPE_SHMEM:
            // 对于Shared Memory，private_data存储共享内存指针
            if (conn->private_data) {
                return ipc_shmem_send_message(conn->private_data, data, size, 0);
            }
            return -1;
        case IPC_TYPE_MSGQUEUE:
            // 对于Message Queue，private_data存储消息队列ID
            if (conn->private_data) {
                int msgq_id = *(int*)conn->private_data;
                return ipc_msgqueue_send_message(msgq_id, data, size, 1);
            }
            return -1;
        default:
            return -1;
    }
}

static int receive_message_by_type(int conn_id, void **data, size_t *size, int timeout_ms) {
    ipc_connection_t *conn = &global_ipc_data.connections[conn_id];
    
    switch (conn->type) {
        case IPC_TYPE_SOCKET:
            // 对于Socket，private_data存储文件描述符
            if (conn->private_data) {
                int fd = *(int*)conn->private_data;
                return ipc_socket_receive_message(fd, data, size, timeout_ms);
            }
            return -1;
        case IPC_TYPE_PIPE:
            // 对于Pipe，private_data存储文件描述符
            if (conn->private_data) {
                int fd = *(int*)conn->private_data;
                return ipc_pipe_receive_message(fd, data, size, timeout_ms);
            }
            return -1;
        case IPC_TYPE_SHMEM:
            // 对于Shared Memory，private_data存储共享内存指针
            if (conn->private_data) {
                return ipc_shmem_receive_message(conn->private_data, data, *size, 0);
            }
            return -1;
        case IPC_TYPE_MSGQUEUE:
            // 对于Message Queue，private_data存储消息队列ID
            if (conn->private_data) {
                int msgq_id = *(int*)conn->private_data;
                return ipc_msgqueue_receive_message(msgq_id, data, size, 1, timeout_ms);
            }
            return -1;
        default:
            return -1;
    }
}

static void cleanup_ipc_by_type(const char *name, ipc_type_t type) {
    switch (type) {
        case IPC_TYPE_SOCKET:
            ipc_socket_cleanup_server(name);
            break;
        case IPC_TYPE_PIPE:
            ipc_pipe_cleanup_server(name);
            break;
        case IPC_TYPE_SHMEM:
            ipc_shmem_cleanup_server(name);
            break;
        case IPC_TYPE_MSGQUEUE:
            ipc_msgqueue_cleanup_server(name);
            break;
        case IPC_TYPE_SEMAPHORE:
            ipc_semaphore_cleanup_server(name);
            break;
        case IPC_TYPE_MUTEX:
            ipc_mutex_cleanup_server(name);
            break;
        default:
            break;
    }
}
