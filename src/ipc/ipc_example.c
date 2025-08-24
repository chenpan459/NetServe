#include "ipc_module.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>

// 事件回调函数
void ipc_event_handler(const ipc_event_t *event, void *user_data) {
    const char *event_names[] = {
        "CONNECT", "DISCONNECT", "DATA_RECEIVED", 
        "ERROR", "TIMEOUT", "HEARTBEAT"
    };
    
    printf("[事件] 类型: %s, 连接ID: %d, 时间: %lu\n", 
           event_names[event->type], 
           event->conn ? event->conn->id : -1, 
           event->timestamp);
    
    if (event->type == IPC_EVENT_DATA_RECEIVED && event->data) {
        printf("[事件] 接收到数据: %.*s\n", (int)event->data_len, (char*)event->data);
    }
}

// 服务器进程
void run_server() {
    printf("启动IPC服务器...\n");
    
    // 初始化IPC模块
    if (ipc_module_init() != 0) {
        printf("IPC模块初始化失败\n");
        return;
    }
    
    // 设置事件回调
    ipc_set_event_callback(ipc_event_handler, NULL);
    
    // 创建服务器
    ipc_config_t config = ipc_default_config;
    strcpy(config.name, "test_server");
    config.type = IPC_TYPE_SOCKET;
    config.max_connections = 5;
    
    if (ipc_create_server(&config) != 0) {
        printf("创建服务器失败\n");
        ipc_module_cleanup();
        return;
    }
    
    printf("服务器创建成功，等待客户端连接...\n");
    
    // 等待一段时间让客户端连接
    sleep(10);
    
    // 获取统计信息
    ipc_stats_t stats;
    if (ipc_get_statistics(&stats) == 0) {
        printf("统计信息:\n");
        printf("  发送消息: %lu\n", stats.messages_sent);
        printf("  接收消息: %lu\n", stats.messages_received);
        printf("  发送字节: %lu\n", stats.bytes_sent);
        printf("  接收字节: %lu\n", stats.bytes_received);
        printf("  当前连接: %lu\n", stats.connections);
    }
    
    // 清理
    ipc_module_cleanup();
    printf("服务器已关闭\n");
}

// 客户端进程
void run_client() {
    printf("启动IPC客户端...\n");
    
    // 初始化IPC模块
    if (ipc_module_init() != 0) {
        printf("IPC模块初始化失败\n");
        return;
    }
    
    // 设置事件回调
    ipc_set_event_callback(ipc_event_handler, NULL);
    
    // 连接到服务器
    ipc_config_t config = ipc_default_config;
    config.type = IPC_TYPE_SOCKET;
    config.timeout_ms = 3000;
    
    int conn_id = ipc_connect_to_server("test_server", &config);
    if (conn_id < 0) {
        printf("连接服务器失败\n");
        ipc_module_cleanup();
        return;
    }
    
    printf("成功连接到服务器，连接ID: %d\n", conn_id);
    
    // 发送一些测试消息
    const char *test_messages[] = {
        "Hello from client!",
        "This is a test message",
        "Testing IPC communication"
    };
    
    for (int i = 0; i < 3; i++) {
        if (ipc_send_notification(conn_id, test_messages[i], IPC_PRIORITY_NORMAL) == 0) {
            printf("发送消息: %s\n", test_messages[i]);
        } else {
            printf("发送消息失败: %s\n", test_messages[i]);
        }
        usleep(500000); // 500ms
    }
    
    // 发送大数据
    printf("发送大数据...\n");
    const size_t large_data_size = 1024 * 1024; // 1MB
    char *large_data = malloc(large_data_size);
    if (large_data) {
        // 填充测试数据
        for (size_t i = 0; i < large_data_size; i++) {
            large_data[i] = (char)(i % 256);
        }
        
        if (ipc_send_large_data(conn_id, large_data, large_data_size, "test_data.bin") == 0) {
            printf("大数据发送成功\n");
        } else {
            printf("大数据发送失败\n");
        }
        
        free(large_data);
    }
    
    // 等待一段时间
    sleep(2);
    
    // 断开连接
    ipc_disconnect(conn_id);
    printf("已断开连接\n");
    
    // 清理
    ipc_module_cleanup();
    printf("客户端已关闭\n");
}

// 共享内存测试
void test_shared_memory() {
    printf("测试共享内存...\n");
    
    const char *shm_name = "test_shm";
    const size_t shm_size = 1024 * 1024; // 1MB
    
    // 创建共享内存
    int shm_id = ipc_create_shared_memory(shm_name, shm_size);
    if (shm_id < 0) {
        printf("创建共享内存失败\n");
        return;
    }
    
    printf("共享内存创建成功，ID: %d\n", shm_id);
    
    // 附加共享内存
    void *shm_ptr;
    size_t actual_size;
    int attach_id = ipc_attach_shared_memory(shm_name, &shm_ptr, &actual_size);
    if (attach_id < 0) {
        printf("附加共享内存失败\n");
        ipc_destroy_shared_memory(shm_name);
        return;
    }
    
    printf("共享内存附加成功，大小: %zu\n", actual_size);
    
    // 写入数据
    const char *test_data = "Hello Shared Memory!";
    strcpy((char*)shm_ptr, test_data);
    printf("写入数据: %s\n", test_data);
    
    // 分离共享内存
    ipc_detach_shared_memory(shm_ptr);
    
    // 销毁共享内存
    ipc_destroy_shared_memory(shm_name);
    printf("共享内存测试完成\n");
}

// 信号量测试
void test_semaphore() {
    printf("测试信号量...\n");
    
    const char *sem_name = "test_sem";
    
    // 创建信号量
    int sem_id = ipc_create_semaphore(sem_name, 1);
    if (sem_id < 0) {
        printf("创建信号量失败\n");
        return;
    }
    
    printf("信号量创建成功，ID: %d\n", sem_id);
    
    // 等待信号量
    if (ipc_wait_semaphore(sem_name, 1000) == 0) {
        printf("获取信号量成功\n");
        
        // 模拟一些工作
        usleep(100000); // 100ms
        
        // 释放信号量
        if (ipc_signal_semaphore(sem_name) == 0) {
            printf("释放信号量成功\n");
        }
    } else {
        printf("获取信号量失败\n");
    }
    
    // 销毁信号量
    ipc_destroy_semaphore(sem_name);
    printf("信号量测试完成\n");
}

// 主函数
int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("用法: %s [server|client|shm|sem]\n", argv[0]);
        printf("  server - 启动服务器\n");
        printf("  client - 启动客户端\n");
        printf("  shm    - 测试共享内存\n");
        printf("  sem    - 测试信号量\n");
        return 1;
    }
    
    if (strcmp(argv[1], "server") == 0) {
        run_server();
    } else if (strcmp(argv[1], "client") == 0) {
        run_client();
    } else if (strcmp(argv[1], "shm") == 0) {
        test_shared_memory();
    } else if (strcmp(argv[1], "sem") == 0) {
        test_semaphore();
    } else if (strcmp(argv[1], "test") == 0) {
        // 测试多进程通信
        printf("测试多进程IPC通信...\n");
        
        // 启动服务器进程
        pid_t server_pid = fork();
        if (server_pid == 0) {
            // 子进程 - 服务器
            run_server();
            exit(0);
        } else if (server_pid > 0) {
            // 父进程
            printf("服务器进程启动，PID: %d\n", server_pid);
            
            // 等待服务器启动
            sleep(2);
            
            // 启动多个客户端进程
            for (int i = 0; i < 3; i++) {
                pid_t client_pid = fork();
                if (client_pid == 0) {
                    // 子进程 - 客户端
                    printf("客户端 %d 启动\n", i + 1);
                    run_client();
                    exit(0);
                } else if (client_pid > 0) {
                    printf("客户端 %d 进程启动，PID: %d\n", i + 1, client_pid);
                }
            }
            
            // 等待所有子进程完成
            for (int i = 0; i < 4; i++) {
                wait(NULL);
            }
            
            printf("所有测试完成\n");
        } else {
            printf("创建进程失败\n");
            return 1;
        }
    } else {
        printf("未知参数: %s\n", argv[1]);
        return 1;
    }
    
    return 0;
}
