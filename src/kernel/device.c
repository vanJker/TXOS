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
devid_t dev_install(dev_type_t type, dev_subtype_t subtype, void *dev, char *name, 
                  devid_t parent, void *ioctl, void *read, void *write) {
    dev_t *vdev = get_null_dev();
    
    strncpy(vdev->name, name, DEV_NAMELEN);
    vdev->type = type;
    vdev->subtype = subtype;
    vdev->parent = 0;
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
    dev_t *dev = dev_get(dev_id);
    if (dev->ioctl == NULL) {
        LOGK("Device %d's ioctl is unimplement...\n", dev->dev_id);
        return EOF;
    }
    return dev->ioctl(dev, cmd, args, flags);
}

// 读设备
i32 dev_read(devid_t dev_id, void *buf, size_t count, size_t idx, i32 flags) {
    dev_t *dev = dev_get(dev_id);
    if (dev->read == NULL) {
        LOGK("Device %d's read is unimplement...\n", dev->dev_id);
        return EOF;
    }
    return dev->read(dev, buf, count, idx, flags);
}

// 写设备
i32 dev_write(devid_t dev_id, void *buf, size_t count, size_t idx, i32 flags) {
    dev_t *dev = dev_get(dev_id);
    if (dev->write == NULL) {
        LOGK("Device %d's write is unimplement...\n", dev->dev_id);
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
        dev->dev_id = i;
        dev->parent = 0;
        dev->dev = NULL;
        dev->ioctl = NULL;
        dev->read = NULL;
        dev->write = NULL;
    }
}
