#include "ipc_semaphore.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/types.h>

// Semaphore IPC通信实现
int ipc_semaphore_create_server(const char *name, int initial_value) {
    if (!name) {
        return -1;
    }
    
    // 生成信号量key
    key_t key = ftok(name, 'S');
    if (key == -1) {
        return -1;
    }
    
    // 创建System V信号量
    int sem_id = semget(key, 1, IPC_CREAT | 0666);
    if (sem_id == -1) {
        return -1;
    }
    
    // 初始化信号量值
    union semun {
        int val;
        struct semid_ds *buf;
        unsigned short *array;
    } arg;
    
    arg.val = initial_value;
    if (semctl(sem_id, 0, SETVAL, arg) == -1) {
        return -1;
    }
    
    return sem_id;
}

int ipc_semaphore_connect_client(const char *name) {
    if (!name) {
        return -1;
    }
    
    // 生成信号量key
    key_t key = ftok(name, 'S');
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

int ipc_semaphore_wait(int sem_id, int timeout_ms) {
    if (sem_id < 0) {
        return -1;
    }
    
    // 等待信号量（P操作）
    struct sembuf op = {0, -1, 0};
    
    if (timeout_ms > 0) {
        // 非阻塞模式
        if (semop(sem_id, &op, 1) == -1) {
            if (errno == EAGAIN) {
                return -1; // 超时
            }
            return -1;
        }
    } else {
        // 阻塞模式
        if (semop(sem_id, &op, 1) == -1) {
            return -1;
        }
    }
    
    return 0;
}

int ipc_semaphore_signal(int sem_id) {
    if (sem_id < 0) {
        return -1;
    }
    
    // 释放信号量（V操作）
    struct sembuf op = {0, 1, 0};
    if (semop(sem_id, &op, 1) == -1) {
        return -1;
    }
    
    return 0;
}

int ipc_semaphore_get_value(int sem_id) {
    if (sem_id < 0) {
        return -1;
    }
    
    // 获取信号量当前值
    int value = semctl(sem_id, 0, GETVAL);
    return value;
}

int ipc_semaphore_set_value(int sem_id, int value) {
    if (sem_id < 0 || value < 0) {
        return -1;
    }
    
    // 设置信号量值
    union semun {
        int val;
        struct semid_ds *buf;
        unsigned short *array;
    } arg;
    
    arg.val = value;
    if (semctl(sem_id, 0, SETVAL, arg) == -1) {
        return -1;
    }
    
    return 0;
}

int ipc_semaphore_try_wait(int sem_id) {
    if (sem_id < 0) {
        return -1;
    }
    
    // 尝试等待信号量（非阻塞）
    struct sembuf op = {0, -1, IPC_NOWAIT};
    if (semop(sem_id, &op, 1) == -1) {
        if (errno == EAGAIN) {
            return -1; // 信号量不可用
        }
        return -1;
    }
    
    return 0;
}

int ipc_semaphore_create_array(const char *name, int num_sems, int *initial_values) {
    if (!name || num_sems <= 0 || !initial_values) {
        return -1;
    }
    
    // 生成信号量key
    key_t key = ftok(name, 'S');
    if (key == -1) {
        return -1;
    }
    
    // 创建System V信号量数组
    int sem_id = semget(key, num_sems, IPC_CREAT | 0666);
    if (sem_id == -1) {
        return -1;
    }
    
    // 初始化每个信号量的值
    union semun {
        int val;
        struct semid_ds *buf;
        unsigned short *array;
    } arg;
    
    for (int i = 0; i < num_sems; i++) {
        arg.val = initial_values[i];
        if (semctl(sem_id, i, SETVAL, arg) == -1) {
            return -1;
        }
    }
    
    return sem_id;
}

int ipc_semaphore_wait_array(int sem_id, int num_sems, int *operations) {
    if (sem_id < 0 || num_sems <= 0 || !operations) {
        return -1;
    }
    
    // 构造操作数组
    struct sembuf *ops = malloc(num_sems * sizeof(struct sembuf));
    if (!ops) {
        return -1;
    }
    
    for (int i = 0; i < num_sems; i++) {
        ops[i].sem_num = i;
        ops[i].sem_op = operations[i];
        ops[i].sem_flg = 0;
    }
    
    // 执行信号量操作
    int result = semop(sem_id, ops, num_sems);
    free(ops);
    
    return result;
}

void ipc_semaphore_cleanup_server(const char *name) {
    if (name) {
        // 删除System V信号量
        key_t key = ftok(name, 'S');
        if (key != -1) {
            int sem_id = semget(key, 0, 0666);
            if (sem_id != -1) {
                semctl(sem_id, 0, IPC_RMID);
            }
        }
    }
}
