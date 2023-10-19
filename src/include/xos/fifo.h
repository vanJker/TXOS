#ifndef XOS_FIFO_H
#define XOS_FIFO_H

#include <xos/types.h>

// 以字节为单位的先进先出队列
typedef struct fifo_t {
    u8 *buf;     // 缓冲区
    size_t len;  // 长度
    size_t head; // 头索引
    size_t tail; // 尾索引
} fifo_t;

// 初始化 FIFO
void fifo_init(fifo_t *fifo, u8 *buf, size_t len);

// 判断 FIFO 是否为满
bool fifo_full(fifo_t *fifo);

// 判断 FIFO 是否为空
bool fifo_empty(fifo_t *fifo);

// 在 FIFO 中加入字节 byte
void fifo_put(fifo_t *fifo, u8 byte);

// 在 FIFO 中取出排队的第一个字节
u8 fifo_get(fifo_t *fifo);

#endif