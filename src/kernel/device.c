#include <xos/device.h>
#include <xos/string.h>
#include <xos/assert.h>
#include <xos/debug.h>

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
did_t dev_install(dev_type_t type, dev_subtype_t subtype, void *dev, char *name, 
                  did_t parent, void *ioctl, void *read, void *write) {
    dev_t *vdev = get_null_dev();
    
    strncpy(vdev->name, name, DEV_NAMELEN);
    vdev->type = type;
    vdev->subtype = subtype;
    vdev->parent = 0;
    vdev->dev = dev;
    vdev->ioctl = ioctl;
    vdev->read = read;
    vdev->write = write;

    return vdev->did;
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
dev_t *dev_get(did_t did) {
    assert(did >= 0 && did < DEV_NR);
    dev_t *dev = &devices[did];
    assert(dev->type != DEV_NULL);
    return dev;
}

// 控制设备
i32 dev_ioctl(did_t did, dev_cmd_t cmd, void *args, i32 flags) {
    dev_t *dev = dev_get(did);
    if (dev->ioctl == NULL) {
        LOGK("Device %d's ioctl is unimplement...\n", dev->did);
        return EOF;
    }
    return dev->ioctl(dev, cmd, args, flags);
}

// 读设备
i32 dev_read(did_t did, void *buf, size_t count, size_t idx, i32 flags) {
    dev_t *dev = dev_get(did);
    if (dev->read == NULL) {
        LOGK("Device %d's read is unimplement...\n", dev->did);
        return EOF;
    }
    return dev->read(dev, buf, count, idx, flags);
}

// 写设备
i32 dev_write(did_t did, void *buf, size_t count, size_t idx, i32 flags) {
    dev_t *dev = dev_get(did);
    if (dev->write == NULL) {
        LOGK("Device %d's write is unimplement...\n", dev->did);
        return EOF;
    }
    return dev->write(dev, buf, count, idx, flags);
}

// 初始化虚拟设备
void device_init() {
    for (size_t i = 0; i < DEV_NR; i++) {
        dev_t *dev = &devices[i];
        strncpy(dev->name, "null", DEV_NAMELEN);
        dev->type = DEV_NULL;
        dev->did = i;
        dev->parent = 0;
        dev->dev = NULL;
        dev->ioctl = NULL;
        dev->read = NULL;
        dev->write = NULL;
    }
}
