#include "ipc_msgqueue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>

// 消息结构定义
struct ipc_msg {
    long mtype;
    char mtext[IPC_MSGQUEUE_MAX_SIZE];
};

// Message Queue IPC通信实现
int ipc_msgqueue_create_server(const char *name) {
    if (!name) {
        return -1;
    }
    
    // 生成消息队列key
    key_t key = ftok(name, 'M');
    if (key == -1) {
        return -1;
    }
    
    // 创建System V消息队列
    int msgq_id = msgget(key, IPC_CREAT | 0666);
    if (msgq_id == -1) {
        return -1;
    }
    
    return msgq_id;
}

int ipc_msgqueue_connect_client(const char *name) {
    if (!name) {
        return -1;
    }
    
    // 生成消息队列key
    key_t key = ftok(name, 'M');
    if (key == -1) {
        return -1;
    }
    
    // 连接到System V消息队列
    int msgq_id = msgget(key, 0666);
    if (msgq_id == -1) {
        return -1;
    }
    
    return msgq_id;
}

int ipc_msgqueue_send_message(int msgq_id, const void *data, size_t size, long msg_type) {
    if (msgq_id < 0 || !data || size == 0 || size > IPC_MSGQUEUE_MAX_SIZE) {
        return -1;
    }
    
    // 构造消息
    struct ipc_msg msg;
    msg.mtype = msg_type;
    memcpy(msg.mtext, data, size);
    
    // 发送消息
    if (msgsnd(msgq_id, &msg, size, 0) == -1) {
        return -1;
    }
    
    return 0;
}

int ipc_msgqueue_receive_message(int msgq_id, void **data, size_t *size, long msg_type, int timeout_ms) {
    if (msgq_id < 0 || !data || !size) {
        return -1;
    }
    
    // 构造消息
    struct ipc_msg msg;
    
    if (timeout_ms > 0) {
        // 非阻塞接收
        ssize_t received = msgrcv(msgq_id, &msg, IPC_MSGQUEUE_MAX_SIZE, msg_type, IPC_NOWAIT);
        if (received == -1) {
            if (errno == ENOMSG) {
                return -1; // 没有消息
            }
            return -1;
        }
        *size = received;
    } else {
        // 阻塞接收
        ssize_t received = msgrcv(msgq_id, &msg, IPC_MSGQUEUE_MAX_SIZE, msg_type, 0);
        if (received == -1) {
            return -1;
        }
        *size = received;
    }
    
    // 分配接收缓冲区
    *data = malloc(*size);
    if (!*data) {
        return -1;
    }
    
    // 复制消息内容
    memcpy(*data, msg.mtext, *size);
    
    return 0;
}

int ipc_msgqueue_send_typed_message(int msgq_id, const void *data, size_t size, long msg_type) {
    return ipc_msgqueue_send_message(msgq_id, data, size, msg_type);
}

int ipc_msgqueue_receive_typed_message(int msgq_id, void **data, size_t *size, long msg_type, int timeout_ms) {
    return ipc_msgqueue_receive_message(msgq_id, data, size, msg_type, timeout_ms);
}

int ipc_msgqueue_get_message_count(int msgq_id) {
    if (msgq_id < 0) {
        return -1;
    }
    
    struct msqid_ds buf;
    if (msgctl(msgq_id, IPC_STAT, &buf) == -1) {
        return -1;
    }
    
    return (int)buf.msg_qnum;
}

int ipc_msgqueue_get_max_size(int msgq_id) {
    if (msgq_id < 0) {
        return -1;
    }
    
    struct msqid_ds buf;
    if (msgctl(msgq_id, IPC_STAT, &buf) == -1) {
        return -1;
    }
    
    return (int)buf.msg_qbytes;
}

int ipc_msgqueue_set_max_size(int msgq_id, int max_size) {
    if (msgq_id < 0 || max_size <= 0) {
        return -1;
    }
    
    struct msqid_ds buf;
    if (msgctl(msgq_id, IPC_STAT, &buf) == -1) {
        return -1;
    }
    
    buf.msg_qbytes = max_size;
    if (msgctl(msgq_id, IPC_SET, &buf) == -1) {
        return -1;
    }
    
    return 0;
}

void ipc_msgqueue_cleanup_server(const char *name) {
    if (name) {
        // 删除System V消息队列
        key_t key = ftok(name, 'M');
        if (key != -1) {
            int msgq_id = msgget(key, 0666);
            if (msgq_id != -1) {
                msgctl(msgq_id, IPC_RMID, NULL);
            }
        }
    }
}
