# 061 数据结构 - 循环队列

本节实现一个循环队列，需要注意的是，这个循环队列的主要目的是支持键盘的环形缓冲区，所以它只支持一字节的数据。

键盘的环形缓冲区是一个典型的 **生产者 - 消费者** 模型，所以需要使用循环队列这种 FIFO 数据结构来实现。

## 1. 数据结构

> 以下代码位于 `include/xos/fifo.h`

```c
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
```

## 2. 初始化循环队列

> 以下代码位于 `kernel/fifo.c`

```c
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
```

因为是循环队列，所以使用求余算法来实现了获取下一个索引的功能。同时注意，在初始时，我们将循环队列的状态设置为头索引和尾索引指向同一个位置，这也是循环队列为空的状态。

## 2. 判断 FIFO 满 / 空

```c
// 判断 FIFO 是否为满
bool fifo_full(fifo_t *fifo) {
    return fifo_index_next(fifo, fifo->tail) == fifo->head;
}

// 判断 FIFO 是否为空
bool fifo_empty(fifo_t *fifo) {
    return fifo->head == fifo->tail;
}
```

由于初始时循环队列为空，且此时首尾索引相同，所以我们通过首尾索引是否相同，来判断循环队列是否为空。

而通过尾索引的下一个索引是否为头索引，来判断循环队列是否为满。但是这样会导致循环队列的缓冲区只能容纳 `LEN-1` 个字节（因为尾索引指向的是将要插入元素需要插入的位置）。

如果将循环队列的缓冲区装满的话，会无法分清缓冲区满还是空（都是首尾索引相同）。当然也可以通过记录循环队列中元素个数，来实现完全利用缓冲区，但是本节为了简单起见，并没有使用这种方法。

## 3. 入队 & 出队

理解了循环队列的 FIFO 机制后，实现入队、出队功能十分简单。无非就是放置 / 取出元素，更新首尾索引。

在入队操作中，如果缓冲区满了的话，我们采取的策略是直接丢掉队首的元素，以放入新元素。

```c
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
```

## 4. 测试循环队列

编写单元测试：

```c
/* kernel/fifo.c */
void fifo_test() {
    const size_t LEN = 5;
    u8 buf[LEN];
    fifo_t fifo;

    fifo_init(&fifo, buf, LEN);
    for (size_t i = 0; i < LEN + 3; i++) {
        fifo_put(&fifo, (u8)i);
    }
    assert(fifo_full(&fifo));   // shoube be passed

    for (size_t i = 0; i < LEN - 1; i++) {
        u8 byte = fifo_get(&fifo);
        LOGK("fifo byte: 0x%x\n", byte);
        // should be 0x4, 0x5, 0x6, 0x7
    }
    assert(fifo_empty(&fifo));  // should be passed
}
```

---

搭建测试框架：

```c
/* kernel/main.c */
void kernel_init() {
    console_init();
    gdt_init();
    memory_init();
    kernel_map_init();
    interrupt_init();
    clock_init();
    keyboard_init();
    fifo_test();

    hang();
    return;
}
```

---

使用调试模式来观察，测试过程当中变量的值变化，是否符合预期。

## 5. 环形缓冲区

实现了循环队列后，我们需要使用它来实现键盘的环形缓冲区，即如果一个任务在等待键盘输入时，而且此时键盘的环形缓冲区为空，则阻塞当前任务，等待键盘输入。这就是一个典型的 **生产者 - 消费者** 模型。

> 以下代码位于 `kernel/keyboard.c`

### 5.1 键盘管理器

在键盘管理器当中增加与环形缓冲区相关的变量：循环队列、临界区需要的互斥锁、等待键盘输入的任务（即消费者，键盘作为生产者）。

```c
#define BUFFER_SIZE 64 // 环形缓冲区大小

// 键盘管理器
typedef struct keyboard_t {
    ...
    /**** 环形缓冲区 ****/
    mutexlock_t lock;   // 环形缓冲区的锁
    fifo_t fifo;        // 循环队列
    u8 buf[BUFFER_SIZE];// 输入缓冲区
    task_t *waiter;     // 等待键盘输入的任务 
} keyboard_t;
static keyboard_t keyboard;
```

在键盘管理器初始化当中，加入初始化环形缓冲区相关变量的逻辑：

```c
// 初始化键盘管理器
void keyboard_new(keyboard_t *keyboard) {
    ...
    mutexlock_init(&keyboard->lock);
    fifo_init(&keyboard->fifo, keyboard->buf, BUFFER_SIZE);
    keyboard->waiter = NULL;
}
```

### 5.2 消费者

实现消费者会使用到的消费函数，即消费键盘环形缓冲区的字符，如果环形缓冲区为空，则等待直到键盘有新的输入。

因为涉及键盘环形缓冲区这个临界区，需要使用互斥锁来保证互斥性，即只能有一个任务作为环形缓冲区的消费者（生产者显然只有键盘这一个），保证了环形缓冲区的 **一对一** 的模型。

```c
// 从键盘的环形缓冲区读取字符到指定缓冲区，并返回读取字符的个数
size_t keyboard_read(char *buf, size_t count) {
    mutexlock_acquire(&keyboard.lock);
    int i = 0;
    while (i < count) {
        while (fifo_empty(&keyboard.fifo)) {
            // 如果当前键盘环形缓冲区为空，则阻塞当前任务为等待状态
            keyboard.waiter = current_task();
            task_block(keyboard.waiter, NULL, TASK_WAITING);
        }
        buf[i++] = fifo_get(&keyboard.fifo);
    }
    mutexlock_release(&keyboard.lock);
    return i;
}
```

### 5.3 键盘中断处理

将键盘中断处理最后打印可见字符的逻辑，修改为，将可见字符加入键盘的环形缓冲区（后续可能会为将所有输入的字符都加入环形缓冲区，无论可见字符，还是不可见字符），并且如果当前有任务在等待，则结束这个任务的等待状态。

```c
void keyboard_handler(int vector) {
    ...
    // 如果是不可见字符，则直接返回
    if (ch == INV) return;

    // 否则的话，就将按键组合对应的字符加入键盘的环形缓冲区
    fifo_put(&keyboard.fifo, ch);
    if (keyboard.waiter) {
        // 如果有任务在等待键盘输入
        task_unblock(keyboard.waiter);
        keyboard.waiter = NULL;
    }

    return;
}
```

### 5.4 测试环形缓冲区

修改初始化任务 `init_thread` 的逻辑如下：

```c
void init_thread() {
    irq_enable();
    u32 counter = 0;

    char ch;
    while (true) {
        u32 irq = irq_disable();
        keyboard_read(&ch, 1);
        printk("%c", ch);
        set_irq_state(irq);
    }
}
```

> 理论上是不需要关闭中断的，但由于在 `keyboard_read()` 中的阻塞任务 `task_block()` 还是采用关中断策略来保护临界区，所以这里需要关闭中断。
>
>（事实上，你也可以使用互斥锁来策略来替换 `task_block()` 的临界区保护策略，这样就不需要关闭中断了）

---

搭建测试框架：

```c
void kernel_init() {
    console_init();
    gdt_init();
    memory_init();
    kernel_map_init();
    interrupt_init();
    clock_init();
    keyboard_init();
    task_init();
    syscall_init();

    irq_enable(); // 打开外中断响应

    hang();
    return;
}
```

---

预期为，与在输入框进行键盘输入基本相同，支持换行、退格等等（但目前只支持在同一行进行退格，但这是 `printk()` 的限制）。
