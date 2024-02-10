# 079 块设备请求

块设备（如硬盘，软盘）的读写以扇区 (512B) 为单位，操作比较耗时，需要寻道，寻道时需要旋转磁头臂，所以需要一种策略来完成磁盘的访问,目前主流的策略的“电梯调度算法”。这个算法我们留到下一节来实现，本节实现基本的块设备请求机制。

本节主要实现的功能：

```c
// 块设备请求
void dev_request(devid_t dev_id, void *buf, size_t count, size_t idx, i32 flags, req_type_t type);
```

## 1. 安装块设备

与上一节字符设备的安装类似，本节我们需要安装块设备，包括磁盘、分区。

增加设备具体类型、设备控制命令：

```c
//--> include/xos/device.h

// 设备具体类型 (例如控制台、键盘、硬盘等)
typedef enum dev_subtype_t {
    ...
    /* 块设备 */
    DEV_ATA_DISK,   // ATA 磁盘
    DEV_ATA_PART,   // ATA 磁盘分区
} dev_subtype_t;

// 设备控制命令
typedef enum dev_cmd_t {
    ...
    DEV_CMD_SECTOR_START,   // 获取设备扇区的起始 LBA
    DEV_CMD_SECTOR_COUNT,   // 获取设备扇区的数量
} dev_cmd_t;
```

块设备对于读、写、控制功能均有对应的实现，下面实现支持对应上面控制命令的块设备控制函数：

```c
//--> kernel/ata.c

// 发送磁盘控制命令，获取对应信息
i32 ata_pio_ioctl(ata_disk_t *disk, dev_cmd_t cmd, void *args, i32 flags) {
    switch (cmd) {
    case DEV_CMD_SECTOR_START:
        return 0;
    case DEV_CMD_SECTOR_COUNT:
        return disk->total_lba;
    default:
        panic("Unknown device command %d...", cmd);
        break;
    }
}

// 发送分区控制命令，获取对应信息
i32 ata_pio_partition_ioctl(ata_partition_t part, dev_cmd_t cmd, void *args, i32 flags) {
    switch (cmd) {
    case DEV_CMD_SECTOR_START:
        return part.start_lba;
    case DEV_CMD_SECTOR_COUNT:
        return part.count;
    default:
        panic("Unknown device command %d...", cmd);
        break;
    }
}
```

块设备的安装，为了解耦合，独立成一个函数：

```c
//--> kernel/ata.c

// 安装块设备
static void ata_device_install() {
    for (size_t bidx = 0; bidx < ATA_BUS_NR; bidx++) {
        ata_bus_t *bus = &buses[bidx];
        
        for (size_t didx = 0; didx < ATA_DISK_NR; didx++) {
            ata_disk_t *disk = &bus->disks[didx];

            // 硬盘不存在
            if (disk->total_lba == 0) continue;
            // 安装磁盘设备
            devid_t dev_id = dev_install(DEV_BLOCK, DEV_ATA_DISK, disk, disk->name, -1, 
                                         ata_pio_ioctl, ata_pio_read, ata_pio_write);

            for (size_t pidx = 0; pidx < ATA_PARTITION_NR; pidx++) {
                ata_partition_t *part = &disk->parts[pidx];

                // 分区不存在
                if (part->count == 0) continue;
                // 安装分区设备
                dev_install(DEV_BLOCK, DEV_ATA_PART, part, part->name, dev_id, 
                            ata_pio_partition_ioctl, ata_pio_partition_read, 
                            ata_pio_partition_write);
            }
        }
    }
}

// ATA 总线和磁盘初始化
void ata_init() {
    ...
    ata_device_install();
    ...
}
```

对于分区，需要设置其父设备号为其所在硬盘的设备号。

## 2. 块设备请求

### 2.1 请求列表

本节需要实现的块设备请求功能为：

```c
//--> include/xos/device.h

// 块设备请求
void dev_request(devid_t dev_id, void *buf, size_t count, size_t idx, i32 flags, req_type_t type);
```

这个功能的逻辑简单来说，是将之前的 `dev_read()` 和 `dev_write()` 通过请求类型参数 `type` 进行进一步抽象，通过请求类型 `type` 分发逻辑执行读或写。

但是除了这个分发逻辑功能之外，我们还需要一些额外的功能，因为块设备和字符设备不同，块设备有多个，并且不同的进程可以同时持有块设备（相同或不同都可以），然后通过持有的块设备发出操作请求。显然这样对于块设备操作的区域（硬盘或分区所对应的扇区），是一个临界区，会发生数据竞争。我们需要将这些请求排成列表，然后依次执行这些请求，这样既可以防止数据竞争，又可以保证完整地执行了全部的请求。另外，这种请求列表的形式还可以方便我们下一节对请求实现其它的调度算法，本节对请求采取的策略是依次执行，也即先来先服务 (FCFS)，下一节会实现比较高效的电梯调度算法。

为了能将请求保存到请求列表里，需要定义请求类型来保存相关信息：

```c
//--> include/xos/device.h

// 块设备请求
typedef struct request_t {
    devid_t dev_id;     // 请求设备号
    req_type_t type;    // 请求类型
    size_t idx;         // 请求扇区起始 LBA
    size_t count;       // 请求扇区数量
    i32 flags;          // 特殊标志
    void *buf;          // 缓冲区
    task_t *task;       // 请求进程
    list_node_t node;   // 请求列表节点
} request_t;

// 块设备请求类型
typedef enum req_type_t {
    REQ_READ,   // 块设备读
    REQ_WRITE,  // 块设备写
} req_type_t;
```

如上面请求的定义，对于请求列表我们使用链表来实现，所以对于设备还需要增加一个请求列表的字段，用于保存该设备所有待处理的请求：

```c
//--> include/xos/device.h

// 虚拟设备
typedef struct dev_t {
    ...
    list_t request_list;    // 块设备请求列表
    ...
} dev_t;

//--> kernel/device.c

// 初始化虚拟设备
void device_init() {
    for (size_t i = 0; i < DEV_NR; i++) {
        dev_t *dev = &devices[i];
        ...
        list_init(&dev->request_list);
    }
}
```

### 2.2 请求处理

请求执行的逻辑很简单，就是根据请求类型来进行分发：

```c
//--> kernel/device.c

static void do_dev_request(request_t *req) {
    switch (req->type) {
    case REQ_READ:
        dev_read(req->dev_id, req->buf, req->count, req->idx, req->flags);
        break;
    case REQ_WRITE:
        dev_write(req->dev_id, req->buf, req->count, req->idx, req->flags);
        break;
    default:
        panic("Unknown request type %d...");
        break;
    }
}
```

因为控制功能不涉及磁头在磁盘上的移动，所以不需要放在请求里执行（我们下一节需要根据磁头移动的方向来实现电梯调度算法）。

接下来需要实现请求的处理，主要逻辑为：

1. 根据参数构造对应的请求
2. 将请求加入对应设备的请求列表
3. 如果设备的请求列表没有未处理的请求，则直接执行该请求
4. 否则阻塞发出该请求的进程，直到其它进程的请求处理完毕唤醒该进程
5. 执行完请求后将其移除设备的请求列表
6. 如果设备的请求列表中还有未处理的请求，则唤醒下一个未处理请求的进程进行处理

> 因为设备的请求列表是一个临界区，所以执行该函数时需要保证中断禁止

```c
//--> kernel/device.c

// 块设备请求
void dev_request(devid_t dev_id, void *buf, size_t count, size_t idx, i32 flags, req_type_t type) {
    ASSERT_IRQ_DISABLE();   // 保证中断禁止

    dev_t *dev = dev_get(dev_id);   // 获取设备
    assert(dev->type == DEV_BLOCK); // 保证为块设备

    // 构造请求
    request_t *req = (request_t *)kmalloc(sizeof(request_t));

    req->dev_id = dev_id;
    req->type = type;
    req->idx = idx;
    req->count = count;
    req->flags = flags;
    req->buf = buf;
    req->task = current_task();

    // 将请求加入对应设备的请求列表，如果设备的请求列表有其它请求，则阻塞等待调度
    // 否则直接执行请求，无需阻塞
    bool flag = list_empty(&dev->request_list);
    list_push_back(&dev->request_list, &req->node);
    if (!flag) {
        task_block(req->task, NULL, TASK_BLOCKED);
    }

    // 执行对应的请求操作，并移出请求列表
    do_dev_request(req);
    list_remove(&req->node);
    kfree(req);

    // 先来先服务 (FCFS)
    if (!list_empty(&dev->request_list)) {
        request_t *next_req = element_entry(request_t, node, dev->request_list.head.next);
        assert(next_req->task->magic == XOS_MAGIC); // 检测栈溢出
        task_unblock(next_req->task);
    }
}
```

## 3. 功能测试

### 3.1 测试框架

与上一节类似，我们在测试用的系统调用 `sys_test` 中测试相关功能：

```c
//--> kernel/syscall.c

static u32 sys_test() {
    dev_t *device;
    void *buf = (void *)kalloc_page(1);

    device = dev_find(DEV_ATA_PART, 0);
    assert(device);
    pid_t pid = current_task()->pid;
    memset(buf, pid, 512);
    dev_request(device->dev_id, buf, 1, pid, 0, REQ_WRITE);

    kfree_page((u32)buf, 1);
    return 255;
}
```

这个逻辑主要为：调用该系统调用的进程在第 0 个分区设备上的，第 `pid` (进程 ID) 个扇区上，每个字节都填入 `pid` 这个值。

在测试用的内核进程 `test_thread` 中调用系统调用 `sys_test` 来进行测试：

```c
//--> kernel/thread.c

void test_thread() {
    irq_enable();

    test();
    LOGK("test finished of task %d\n", getpid());
    while (true) {
        sleep(2000);
    }
}
```

因为 `sys_test` 中只发出了一次请求，我们需要观察多个请求时的处理流程，所以新增一些测试进程：

```c
//--> kernel/task.c

void task_init() {
    ...
    task_create((target_t)test_thread, "test", 5, KERNEL_TASK); // 进程 2
    task_create((target_t)test_thread, "test", 5, KERNEL_TASK); // 进程 3
    task_create((target_t)test_thread, "test", 5, KERNEL_TASK); // 进程 4
}
```

又为了保证能观察到，同一个设备在未处理完一个请求时，又接收到另一个请求的情况，我们将写磁盘操作进行延时（因为 `sys_test` 中只用到了写磁盘操作）：

```c
//--> kernel/ata.c

i32 ata_pio_write(ata_disk_t *disk, void *buf, u8 count, size_t lba) {
    ...
    for (size_t i = 0; i < count; i++) {
        ...
        task_sleep(100); // 延时
        ...
    }
}
```

### 3.2 预期结果

在请求处理处打断点，观察执行流程，预期为：

1. 进程 2: `sys_test()` -> `dev_request()` -> `do_request()`
2. 进程 3: `sys_test()` -> `dev_request()` -> `task_block()`
3. 进程 4: `sys_test()` -> `dev_request()` -> `task_block()`
4. 进程 2: `task_unblock()`
5. 进程 3: `do_request()` -> `task_unblock()`
6. 进程 4: `do_request()` -> `task_unblock()`

执行完成后，使用 Hex Editor 打开虚拟磁盘文件 `master.img`：

- 地址 $[00100400， 001006000)$ 全部填为 02
- 地址 $[00100600， 001008000)$ 全部填为 03
- 地址 $[00100800， 00100A000)$ 全部填为 04

> 可以根据分区表信息来计算该结果的准确性。可以使用 Hex Editor 的二进制搜索功能来搜索形如 `0202` 来加快查找速度。

## 4. 参考文献

1. [赵炯 / Linux内核完全注释 / 机械工业出版社 / 2005](https://book.douban.com/subject/1231236/)