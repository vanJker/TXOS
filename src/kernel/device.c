#include <xos/device.h>
#include <xos/string.h>
#include <xos/arena.h>
#include <xos/assert.h>
#include <xos/debug.h>
#include <xos/interrupt.h>

#define DEV_NR 64 // 设备数量

// 设备数组
static dev_t devices[DEV_NR];

// 从设备数组获取一个空设备
static dev_t *get_null_dev() {
    for (size_t i = 0; i < DEV_NR; i++) {
        dev_t *dev = &devices[i];
        if (dev->type == DEV_NULL) {
            return dev;
        }
    }
    panic("no more devices!!!");
}

// 安装设备
devid_t dev_install(dev_type_t type, dev_subtype_t subtype, void *dev, char *name, 
                  devid_t parent, void *ioctl, void *read, void *write) {
    dev_t *vdev = get_null_dev();
    
    strncpy(vdev->name, name, DEV_NAMELEN);
    vdev->type = type;
    vdev->subtype = subtype;
    vdev->parent = -1;
    vdev->dev = dev;
    vdev->ioctl = ioctl;
    vdev->read = read;
    vdev->write = write;

    return vdev->dev_id;
}

// 根据设备具体类型查找该类型的第 idx 个设备
dev_t *dev_find(dev_subtype_t subtype, size_t idx) {
    size_t count = 0;
    for (size_t i = 0; i < DEV_NR; i++) {
        dev_t *dev = &devices[i];
        if (dev->subtype != subtype) continue;
        if (count++ == idx) {
            return dev;
        }
    }
    return NULL;
}

// 根据设备号查找设备
dev_t *dev_get(devid_t dev_id) {
    assert(dev_id >= 0 && dev_id < DEV_NR);
    dev_t *dev = &devices[dev_id];
    assert(dev->type != DEV_NULL);
    return dev;
}

// 控制设备
i32 dev_ioctl(devid_t dev_id, dev_cmd_t cmd, void *args, i32 flags) {
    ASSERT_IRQ_DISABLE();   // 保证中断禁止
    dev_t *dev = dev_get(dev_id);
    if (dev->ioctl == NULL) {
        LOGK("Device %d's ioctl is unimplement...\n", dev->dev_id);
        return EOF;
    }
    return dev->ioctl(dev->dev, cmd, args, flags);
}

// 读设备
i32 dev_read(devid_t dev_id, void *buf, size_t count, size_t idx, i32 flags) {
    ASSERT_IRQ_DISABLE();   // 保证中断禁止
    dev_t *dev = dev_get(dev_id);
    if (dev->read == NULL) {
        LOGK("Device %d's read is unimplement...\n", dev->dev_id);
        return EOF;
    }
    return dev->read(dev->dev, buf, count, idx, flags);
}

// 写设备
i32 dev_write(devid_t dev_id, void *buf, size_t count, size_t idx, i32 flags) {
    ASSERT_IRQ_DISABLE();   // 保证中断禁止
    dev_t *dev = dev_get(dev_id);
    if (dev->write == NULL) {
        LOGK("Device %d's write is unimplement...\n", dev->dev_id);
        return EOF;
    }
    return dev->write(dev->dev, buf, count, idx, flags);
}

// 使用电梯调度算法 (SCAN) 获取所给磁盘请求 req 的下一个请求
static request_t *next_request(dev_t *dev, request_t *req) {
    list_t *list = &dev->request_list;

    // 如果磁盘在当前寻道方向上没有待处理的请求，则变换寻道方向
    // 磁盘设备请求列表按照请求的 LBA 进行排序，即正向为向内寻道，反向为外里寻道
    if (dev->direction == DIRECT_OUT && list_istail(list, &req->node)) {
        dev->direction = DIRECT_IN;
    } else if (dev->direction ==  DIRECT_IN && list_ishead(list, &req->node)) {
        dev->direction = DIRECT_OUT;
    }

    // 根据寻道方向获取下一个请求
    list_node_t *next = NULL;
    switch (dev->direction) {
    case DIRECT_OUT:
        next = req->node.next;
        break;
    case DIRECT_IN:
        next = req->node.prev;
        break;
    default:
        panic("Unknown device direction...\n");
        break;
    }

    // 如果没有其它请求则返回 NULL
    if (list_singular(list)) {
        return NULL;
    }

    return element_entry(request_t, node, next);
}

// 块设备执行请求
static void do_dev_request(request_t *req) {
    LOGK("Device %d do request index %d\n", req->dev_id, req->idx);

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

// 块设备请求
void dev_request(devid_t dev_id, void *buf, size_t count, size_t idx, i32 flags, req_type_t type) {
    ASSERT_IRQ_DISABLE();   // 保证中断禁止

    dev_t *dev = dev_get(dev_id);   // 获取设备
    assert(dev->type == DEV_BLOCK); // 保证为块设备

    request_t *req = (request_t *)kmalloc(sizeof(request_t));

    req->dev_id = dev_id;
    req->type = type;
    req->idx = idx;
    req->count = count;
    req->flags = flags;
    req->buf = buf;
    req->task = current_task();

    LOGK("Device %d request index %d\n", req->dev_id, req->idx);

    // 将请求加入对应设备的请求列表，如果设备的请求列表有其它请求，则阻塞等待调度
    // 否则直接执行请求，无需阻塞
    // 使用插入排序算法，按照 LBA 的大小进行排序
    bool flag = list_empty(&dev->request_list);
    // list_push_back(&dev->request_list, &req->node);
    list_insert_sort(&dev->request_list, &req->node, list_node_offset(request_t, node, idx));
    if (!flag) {
        task_block(req->task, NULL, TASK_BLOCKED);
    }

    // 执行对应的请求操作，并移出请求列表
    do_dev_request(req);
    request_t *next_req = next_request(dev, req);
    list_remove(&req->node);
    kfree(req);

    // 电梯调度算法 (SCAN)
    if (next_req != NULL) {
        assert(next_req->task->magic == XOS_MAGIC); // 检测栈溢出
        task_unblock(next_req->task);
    }
}

// 初始化虚拟设备
void device_init() {
    for (size_t i = 0; i < DEV_NR; i++) {
        dev_t *dev = &devices[i];
        strncpy(dev->name, "null", DEV_NAMELEN);
        dev->type = DEV_NULL;
        dev->dev_id = i;
        dev->parent = 0;
        dev->dev = NULL;
        dev->ioctl = NULL;
        dev->read = NULL;
        dev->write = NULL;
        list_init(&dev->request_list);
        dev->direction = DIRECT_IN;
    }
}
