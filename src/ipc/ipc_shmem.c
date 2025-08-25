#include "ipc_shmem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>

// Shared Memory IPC通信实现
int ipc_shmem_create_server(const char *name, size_t size) {
    if (!name || size == 0) {
        return -1;
    }
    
    // 生成共享内存key
    key_t key = ftok(name, 'I');
    if (key == -1) {
        return -1;
    }
    
    // 创建共享内存段
    int shm_id = shmget(key, size, IPC_CREAT | 0666);
    if (shm_id == -1) {
        return -1;
    }
    
    return shm_id;
}

int ipc_shmem_connect_client(const char *name, size_t *size) {
    if (!name || !size) {
        return -1;
    }
    
    // 生成共享内存key
    key_t key = ftok(name, 'I');
    if (key == -1) {
        return -1;
    }
    
    // 获取共享内存段
    int shm_id = shmget(key, 0, 0666);
    if (shm_id == -1) {
        return -1;
    }
    
    // 获取共享内存信息
    struct shmid_ds shm_info;
    if (shmctl(shm_id, IPC_STAT, &shm_info) == -1) {
        return -1;
    }
    
    *size = shm_info.shm_segsz;
    return shm_id;
}

void* ipc_shmem_attach(int shm_id) {
    if (shm_id < 0) {
        return NULL;
    }
    
    // 附加到共享内存段
    void *ptr = shmat(shm_id, NULL, 0);
    if (ptr == (void*)-1) {
        return NULL;
    }
    
    return ptr;
}

int ipc_shmem_detach(void *ptr) {
    if (!ptr) {
        return -1;
    }
    
    return shmdt(ptr);
}

int ipc_shmem_send_message(void *ptr, const void *data, size_t size, size_t offset) {
    if (!ptr || !data || size == 0) {
        return -1;
    }
    
    // 写入共享内存
    memcpy((char*)ptr + offset, data, size);
    
    return 0;
}

int ipc_shmem_receive_message(void *ptr, void **data, size_t size, size_t offset) {
    if (!ptr || !data || size == 0) {
        return -1;
    }
    
    // 分配接收缓冲区
    *data = malloc(size);
    if (!*data) {
        return -1;
    }
    
    // 从共享内存读取
    memcpy(*data, (char*)ptr + offset, size);
    
    return 0;
}

int ipc_shmem_create_file_mapping(const char *name, size_t size) {
    if (!name || size == 0) {
        return -1;
    }
    
    char shm_name[256];
    snprintf(shm_name, sizeof(shm_name), "/tmp/ipc_shm_%s", name);
    
    // 创建共享内存文件
    int fd = open(shm_name, O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        return -1;
    }
    
    // 设置文件大小
    if (ftruncate(fd, size) == -1) {
        close(fd);
        return -1;
    }
    
    close(fd);
    return 0;
}

void* ipc_shmem_map_file(const char *name, size_t size, int *fd) {
    if (!name || size == 0 || !fd) {
        return NULL;
    }
    
    char shm_name[256];
    snprintf(shm_name, sizeof(shm_name), "/tmp/ipc_shm_%s", name);
    
    // 打开共享内存文件
    *fd = open(shm_name, O_RDWR);
    if (*fd == -1) {
        return NULL;
    }
    
    // 映射到内存
    void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, *fd, 0);
    if (ptr == MAP_FAILED) {
        close(*fd);
        return NULL;
    }
    
    return ptr;
}

int ipc_shmem_unmap_file(void *ptr, size_t size) {
    if (!ptr || size == 0) {
        return -1;
    }
    
    return munmap(ptr, size);
}

void ipc_shmem_cleanup_server(const char *name) {
    if (name) {
        char shm_path[256];
        snprintf(shm_path, sizeof(shm_path), "/tmp/ipc_shm_%s", name);
        unlink(shm_path);
        
        // 删除System V共享内存段
        key_t key = ftok(name, 'I');
        if (key != -1) {
            int shm_id = shmget(key, 0, 0666);
            if (shm_id != -1) {
                shmctl(shm_id, IPC_RMID, NULL);
            }
        }
    }
}
