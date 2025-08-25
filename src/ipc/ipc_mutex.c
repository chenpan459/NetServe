#include "ipc_mutex.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/types.h>

// Mutex IPC通信实现（基于System V信号量）
int ipc_mutex_create_server(const char *name) {
    if (!name) {
        return -1;
    }
    
    // 生成互斥锁key
    key_t key = ftok(name, 'X');
    if (key == -1) {
        return -1;
    }
    
    // 创建System V信号量作为互斥锁
    int sem_id = semget(key, 1, IPC_CREAT | 0666);
    if (sem_id == -1) {
        return -1;
    }
    
    // 初始化信号量为1（可用状态）
    union semun {
        int val;
        struct semid_ds *buf;
        unsigned short *array;
    } arg;
    
    arg.val = 1;
    if (semctl(sem_id, 0, SETVAL, arg) == -1) {
        return -1;
    }
    
    return sem_id;
}

int ipc_mutex_connect_client(const char *name) {
    if (!name) {
        return -1;
    }
    
    // 生成互斥锁key
    key_t key = ftok(name, 'X');
    if (key == -1) {
        return -1;
    }
    
    // 连接到System V信号量
    int sem_id = semget(key, 0, 0666);
    if (sem_id == -1) {
        return -1;
    }
    
    return sem_id;
}

int ipc_mutex_lock(int mutex_id, int timeout_ms) {
    if (mutex_id < 0) {
        return -1;
    }
    
    // 等待信号量（P操作）
    struct sembuf op = {0, -1, 0};
    
    if (timeout_ms > 0) {
        // 非阻塞模式
        if (semop(mutex_id, &op, 1) == -1) {
            if (errno == EAGAIN) {
                return -1; // 超时
            }
            return -1;
        }
    } else {
        // 阻塞模式
        if (semop(mutex_id, &op, 1) == -1) {
            return -1;
        }
    }
    
    return 0;
}

int ipc_mutex_unlock(int mutex_id) {
    if (mutex_id < 0) {
        return -1;
    }
    
    // 释放信号量（V操作）
    struct sembuf op = {0, 1, 0};
    if (semop(mutex_id, &op, 1) == -1) {
        return -1;
    }
    
    return 0;
}

int ipc_mutex_try_lock(int mutex_id) {
    if (mutex_id < 0) {
        return -1;
    }
    
    // 尝试锁定（非阻塞）
    struct sembuf op = {0, -1, IPC_NOWAIT};
    if (semop(mutex_id, &op, 1) == -1) {
        if (errno == EAGAIN) {
            return -1; // 互斥锁不可用
        }
        return -1;
    }
    
    return 0;
}

int ipc_mutex_is_locked(int mutex_id) {
    if (mutex_id < 0) {
        return -1;
    }
    
    // 获取信号量当前值
    int value = semctl(mutex_id, 0, GETVAL);
    if (value == -1) {
        return -1;
    }
    
    // 如果值为0，表示已锁定；如果值为1，表示未锁定
    return (value == 0) ? 1 : 0;
}

int ipc_mutex_get_owner(int mutex_id) {
    if (mutex_id < 0) {
        return -1;
    }
    
    // 获取信号量信息
    struct semid_ds buf;
    if (semctl(mutex_id, 0, IPC_STAT, &buf) == -1) {
        return -1;
    }
    
    // 返回最后操作的进程ID
    return (int)buf.sem_otime;
}

int ipc_mutex_create_named(const char *name) {
    return ipc_mutex_create_server(name);
}

int ipc_mutex_open_named(const char *name) {
    return ipc_mutex_connect_client(name);
}

int ipc_mutex_create_robust(const char *name) {
    if (!name) {
        return -1;
    }
    
    // 生成互斥锁key
    key_t key = ftok(name, 'X');
    if (key == -1) {
        return -1;
    }
    
    // 创建System V信号量作为互斥锁
    int sem_id = semget(key, 1, IPC_CREAT | 0666);
    if (sem_id == -1) {
        return -1;
    }
    
    // 初始化信号量为1（可用状态）
    union semun {
        int val;
        struct semid_ds *buf;
        unsigned short *array;
    } arg;
    
    arg.val = 1;
    if (semctl(sem_id, 0, SETVAL, arg) == -1) {
        return -1;
    }
    
    return sem_id;
}

int ipc_mutex_consistency_check(int mutex_id) {
    if (mutex_id < 0) {
        return -1;
    }
    
    // 检查信号量状态
    struct semid_ds buf;
    if (semctl(mutex_id, 0, IPC_STAT, &buf) == -1) {
        return -1;
    }
    
    // 获取当前值
    int value = semctl(mutex_id, 0, GETVAL);
    if (value == -1) {
        return -1;
    }
    
    // 检查一致性：值应该在0或1之间
    if (value < 0 || value > 1) {
        // 不一致，重置为1
        union semun {
            int val;
            struct semid_ds *buf;
            unsigned short *array;
        } arg;
        
        arg.val = 1;
        if (semctl(mutex_id, 0, SETVAL, arg) == -1) {
            return -1;
        }
        return 1; // 已修复
    }
    
    return 0; // 一致
}

void ipc_mutex_cleanup_server(const char *name) {
    if (name) {
        // 删除System V信号量
        key_t key = ftok(name, 'X');
        if (key != -1) {
            int sem_id = semget(key, 0, 0666);
            if (sem_id != -1) {
                semctl(sem_id, 0, IPC_RMID);
            }
        }
    }
}
