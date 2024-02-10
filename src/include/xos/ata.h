#ifndef XOS_ATA_H
#define XOS_ATA_H

#include <xos/types.h>
#include <xos/mutex.h>
#include <xos/task.h>
#include <xos/device.h>

// 扇区大小 (512 Byte)
#define SECTOR_SIZE 512
// ATA 总线数量
#define ATA_BUS_NR 2
// 每条 ATA 总线可以挂载的磁盘数量
#define ATA_DISK_NR 2
// 每个磁盘的分区数量（目前只支持主分区，所以共 4 个）
#define ATA_PARTITION_NR 4

struct ata_bus_t;
struct ata_disk_t;

// 分区文件系统标识
typedef enum PARTITION_FS {
    PARTITION_FS_FAT12 = 1,     // FAT12
    PARTITION_FS_EXTENDED = 5,  // 扩展分区
    PARTITION_FS_MINIX = 0x80,  // MINIX until 1.4a
    PARTITION_FS_LINUX = 0x83,  // Linux native partition
} PARTITION_FS;

// ATA 分区
typedef struct ata_partition_t {
    char name[8];               // 分区名称
    struct ata_disk_t *disk;    // 分区所在的磁盘
    PARTITION_FS system;        // 分区类型 (表示文件系统)
    u32 start_lba;              // 分区起始扇区的 LBA
    size_t count;               // 分区占有的扇区数
} ata_partition_t;


// ATA 磁盘
typedef struct ata_disk_t {
    char name[8];           // 磁盘名称
    struct ata_bus_t *bus;  // 所在的 ATA 总线
    u8 selector;            // 磁盘选择信息
    bool master;            // 是否为主盘
    size_t total_lba;       // 可用扇区的数量
    ata_partition_t parts[ATA_PARTITION_NR]; // 硬盘分区
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

// 磁盘分区表项
typedef struct partition_entry_t {
    u8  bootable;               // 引导标志
    u8  start_head;             // 分区起始磁头号
    u8  start_sector : 6;       // 分区起始扇区号
    u16 start_cylinder : 10;    // 分区起始柱面号
    u8  system;                 // 分区类型字节 (表示文件系统)
    u8  end_head;               // 分区结束磁头号
    u8  end_sector : 6;         // 分区结束扇区号
    u16 end_cylinder : 10;      // 分区结束柱面号
    u32 start_lba;              // 分区起始扇区的 LBA
    u32 count;                  // 分区占有的扇区数
} _packed partition_entry_t;

// 主引导扇区结构
typedef struct mbr_t {
    u8  code[446];      // 引导代码
    partition_entry_t partition_table[4];   // 分区表
    u16 magic_number;   // 魔数
} _packed mbr_t;

// 发送磁盘控制命令，获取对应信息
i32 ata_pio_ioctl(ata_disk_t *disk, dev_cmd_t cmd, void *args, i32 flags);
// 从磁盘 disk 的第 lba 个扇区开始，读取连续 count 个扇区的数据到缓冲区 buf
i32 ata_pio_read(ata_disk_t *disk, void *buf, u8 count, size_t lba);
// 将缓冲区 buf 的数据写入磁盘 disk 的第 lba 个扇区开始的连续 count 个扇区
i32 ata_pio_write(ata_disk_t *disk, void *buf, u8 count, size_t lba);

// 发送分区控制命令，获取对应信息
i32 ata_pio_partition_ioctl(ata_partition_t part, dev_cmd_t cmd, void *args, i32 flags);
// 从分区 part 的第 lba 个扇区开始读取
i32 ata_pio_partition_read(ata_partition_t *part, void *buf, u8 count, size_t lba);
// 从分区 part 的第 lba 个扇区开始写入
i32 ata_pio_partition_write(ata_partition_t *part, void *buf, u8 count, size_t lba);

#endif