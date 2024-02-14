#ifndef XOS_DEVICE_H
#define XOS_DEVICE_H

#include <xos/types.h>
#include <xos/task.h>
#include <xos/list.h>

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
    /* 字符设备 */
    DEV_CONSOLE,    // 控制台
    DEV_KEYBOARD,   // 键盘
    /* 块设备 */
    DEV_ATA_DISK,   // ATA 磁盘
    DEV_ATA_PART,   // ATA 磁盘分区
} dev_subtype_t;

// 设备控制命令
typedef enum dev_cmd_t {
    DEV_CMD_NULL,           // 空命令
    DEV_CMD_SECTOR_START,   // 获取设备扇区的起始 LBA
    DEV_CMD_SECTOR_COUNT,   // 获取设备扇区的数量
} dev_cmd_t;

// 块设备请求类型
typedef enum req_type_t {
    REQ_READ,   // 块设备读
    REQ_WRITE,  // 块设备写
} req_type_t;

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

// 磁头寻道方向
typedef enum dev_direction_t {
    DIRECT_IN,  // 向内寻道
    DIRECT_OUT, // 向外寻道
} dev_direction_t;


// 虚拟设备
typedef struct dev_t {
    char name[DEV_NAMELEN];     // 设备名
    dev_type_t type;            // 设备类型
    dev_subtype_t subtype;      // 设备具体类型
    devid_t dev_id;             // 设备号
    devid_t parent;             // 父设备号
    void *dev;                  // 具体设备位置
    list_t request_list;        // 块设备请求列表
    dev_direction_t direction;  // 磁盘寻道方向

    // 控制设备
    i32 (*ioctl)(void *dev, dev_cmd_t cmd, void *args, i32 flags);
    // 读设备
    i32 (*read)(void *dev, void *buf, size_t count, size_t idx, i32 flags);
    // 写设备
    i32 (*write)(void *dev, void *buf, size_t count, size_t idx, i32 flags);
} dev_t;

// 安装设备
devid_t dev_install(dev_type_t type, dev_subtype_t subtype, void *dev, char *name, 
                  devid_t parent, void *ioctl, void *read, void *write);

// 根据设备具体类型查找该类型的第 idx 个设备
dev_t *dev_find(dev_subtype_t subtype, size_t idx);

// 根据设备号查找设备
dev_t *dev_get(devid_t dev_id);

// 控制设备
i32 dev_ioctl(devid_t dev_id, dev_cmd_t cmd, void *args, i32 flags);

// 读设备
i32 dev_read(devid_t dev_id, void *buf, size_t count, size_t idx, i32 flags);

// 写设备
i32 dev_write(devid_t dev_id, void *buf, size_t count, size_t idx, i32 flags);

// 块设备请求
void dev_request(devid_t dev_id, void *buf, size_t count, size_t idx, i32 flags, req_type_t type);

#endif