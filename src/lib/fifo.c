#include <xos/fifo.h>
#include <xos/assert.h>
#include <xos/debug.h>

// 返回在 FIFO 中 index 的下一个索引
static _inline size_t fifo_index_next(fifo_t *fifo, size_t index) {
    return (index + 1) % fifo->len;
}

// 初始化 FIFO
void fifo_init(fifo_t *fifo, u8 *buf, size_t len) {
    fifo->buf = buf;
    fifo->len = len;
    fifo->head = fifo->tail = 0;
}

// 判断 FIFO 是否为满
bool fifo_full(fifo_t *fifo) {
    return fifo_index_next(fifo, fifo->tail) == fifo->head;
}

// 判断 FIFO 是否为空
bool fifo_empty(fifo_t *fifo) {
    return fifo->head == fifo->tail;
}

// 在 FIFO 中加入字节 byte
void fifo_put(fifo_t *fifo, u8 byte) {
    // 如果缓冲区满了的话，就直接丢掉一些字节
    while (fifo_full(fifo)) {
        fifo_get(fifo);
    }
    fifo->buf[fifo->tail] = byte;
    fifo->tail = fifo_index_next(fifo, fifo->tail);
}

// 在 FIFO 中取出排队的第一个字节
u8 fifo_get(fifo_t *fifo) {
    assert(!fifo_empty(fifo));
    
    u8 byte = fifo->buf[fifo->head];
    fifo->head = fifo_index_next(fifo, fifo->head);

    return byte;
}
