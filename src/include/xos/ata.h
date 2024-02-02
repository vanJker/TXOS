#ifndef XOS_ATA_H
#define XOS_ATA_H

#include <xos/types.h>
#include <xos/mutex.h>
#include <xos/task.h>

// 扇区大小 (512 Byte)
#define SECTOR_SIZE 512
// ATA 总线数量
#define ATA_BUS_NR 2
// 每条 ATA 总线可以挂载的磁盘数量
#define ATA_DISK_NR 2

struct ata_bus_t;

// ATA 磁盘
typedef struct ata_disk_t {
    char name[8];           // 磁盘名称
    struct ata_bus_t *bus;  // 所在的 ATA 总线
    u8 selector;            // 磁盘选择信息
    bool master;            // 是否为主盘
    size_t total_lba;       // 可用扇区的数量
} ata_disk_t;

// ATA 总线
typedef struct ata_bus_t {
    char name[8];                   // 总线名称
    mutexlock_t lock;               // 总线互斥锁
    u16 iobase;                     // 总线 I/O 寄存器基址
    u16 ctlbase;                    // 总线控制寄存器基址
    ata_disk_t disks[ATA_DISK_NR];  // 挂载的磁盘
    ata_disk_t *active;             // 当前选择的磁盘
    task_t *waiter;                 // 等待总线忙碌结束的进程
} ata_bus_t;

// 从磁盘 disk 的第 lba 个扇区开始，读取连续 count 个扇区的数据到缓冲区 buf
i32 ata_pio_read(ata_disk_t *disk, void *buf, u8 count, size_t lba);
// 将缓冲区 buf 的数据写入磁盘 disk 的第 lba 个扇区开始的连续 count 个扇区
i32 ata_pio_write(ata_disk_t *disk, void *buf, u8 count, size_t lba);

#endif