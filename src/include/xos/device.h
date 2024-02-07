#ifndef XOS_DEVICE_H
#define XOS_DEVICE_H

#include <xos/types.h>

// 设备名称长度
#define DEV_NAMELEN 16

// 设备类型 (例如字符设备、块设备等)
typedef enum dev_type_t {
    DEV_NULL,   // 空设备
    DEV_CHAR,   // 字符设备
    DEV_BLOCK,  // 块设备
} dev_type_t;

// 设备具体类型 (例如控制台、键盘、硬盘等)
typedef enum dev_subtype_t {
    DEV_CONSOLE,    // 控制台
    DEV_KEYBOARD,   // 键盘
} dev_subtype_t;

// 设备命令 (例如读、写等)
typedef enum dev_cmd_t {
    DEV_CMD_NULL,   // 空命令
} dev_cmd_t;

// 虚拟设备
typedef struct dev_t {
    char name[DEV_NAMELEN]; // 设备名
    dev_type_t type;        // 设备类型
    dev_subtype_t subtype;  // 设备具体类型
    did_t did;              // 设备号
    did_t parent;           // 父设备号
    void *dev;              // 具体设备位置

    // 控制设备
    i32 (*ioctl)(void *dev, dev_cmd_t cmd, void *args, i32 flags);
    // 读设备
    i32 (*read)(void *dev, void *buf, size_t count, size_t idx, i32 flags);
    // 写设备
    i32 (*write)(void *dev, void *buf, size_t count, size_t idx, i32 flags);
} dev_t;

// 安装设备
did_t dev_install(dev_type_t type, dev_subtype_t subtype, void *dev, char *name, 
                  did_t parent, void *ioctl, void *read, void *write);

// 根据设备具体类型查找该类型的第 idx 个设备
dev_t *dev_find(dev_subtype_t subtype, size_t idx);

// 根据设备号查找设备
dev_t *dev_get(did_t did);

// 控制设备
i32 dev_ioctl(did_t did, dev_cmd_t cmd, void *args, i32 flags);

// 读设备
i32 dev_read(did_t did, void *buf, size_t count, size_t idx, i32 flags);

// 写设备
i32 dev_write(did_t did, void *buf, size_t count, size_t idx, i32 flags);

#endif