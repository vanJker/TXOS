#include <xos/ata.h>
#include <xos/stdio.h>
#include <xos/io.h>
#include <xos/interrupt.h>
#include <xos/memory.h>
#include <xos/string.h>
#include <xos/assert.h>
#include <xos/debug.h>

// ATA 总线寄存器基址
#define ATA_IOBASE_PRIMARY      0x1F0
#define ATA_IOBASE_SECONDARY    0x170
#define ATA_CTLBASE_PRIMARY     0x3F6
#define ATA_CTLBASE_SECONDARY   0x376

// I/O 寄存器偏移
#define ATA_IO_DATA         0       // 数据寄存器
#define ATA_IO_ERROR        1       // 错误寄存器
#define ATA_IO_FEATURE      1       // 功能寄存器
#define ATA_IO_SECNR        2       // 扇区数量
#define ATA_IO_LBA_LOW      3       // LBA 低字节   
#define ATA_IO_LBA_MID      4       // LBA 中字节
#define ATA_IO_LBA_HIGH     5       // LBA 高字节
#define ATA_IO_DEVICE       6       // 磁盘选择寄存器
#define ATA_IO_STATUS       7       // 状态寄存器
#define ATA_IO_COMMAND      7       // 命令寄存器

// Control 寄存器偏移
#define ATA_CTL_ALT_STATUS  0       // 备用状态寄存器
#define ATA_CTL_DEV_CONTROL 1       // 设备控制寄存器
#define ATA_CTL_DRV_ADDRESS 1       // 驱动地址寄存器

// ATA 总线状态寄存器
#define ATA_SR_NULL         0x00    // NULL
#define ATA_SR_ERR          0x01    // Error
#define ATA_SR_IDX          0x02    // Index
#define ATA_SR_CORR         0x04    // Corrected data
#define ATA_SR_DRQ          0x08    // Data request
#define ATA_SR_DSC          0x10    // Drive seek complete
#define ATA_SR_DWF          0x20    // Drive write fault
#define ATA_SR_DRDY         0x40    // Drive ready
#define ATA_SR_BSY          0x80    // Controller busy

// ATA 总线错误寄存器
#define ATA_ER_AMNF         0x01    // Address mark not found
#define ATA_ER_TK0NF        0x02    // Track 0 not found
#define ATA_ER_ABRT         0x04    // Abort
#define ATA_ER_MCR          0x08    // Media change requested
#define ATA_ER_IDNF         0x10    // Sector id not found
#define ATA_ER_MC           0x20    // Media change
#define ATA_ER_UNC          0x40    // Uncorrectable data error
#define ATA_ER_BBK          0x80    // Bad block

// ATA 命令
#define ATA_CMD_READ        0x20    // 读命令
#define ATA_CMD_WRITE       0x30    // 写命令
#define ATA_CMD_IDENTIFY    0xEC    // 识别命令

// ATA 总线设备控制寄存器命令
#define ATA_CTRL_HD15       0x00    // Use 4 bits for head (not used, was 0x08)
#define ATA_CTRL_SRST       0x04    // Soft reset
#define ATA_CTRL_NIEN       0x02    // Disable interrupts

#define ATA_MASTER_SELECTOR 0b11100000
#define ATA_SLAVE_SELECTOR  0b11110000

// 共两条总线
ata_bus_t buses[ATA_BUS_NR];

// 硬盘识别信息
typedef struct ata_identify_data_t
{
    u16 config;                 // 00 General configuration bits
    u16 cylinders;              // 01 obsolete
    u16 RESERVED;               // 02
    u16 heads;                  // 03 obsolete
    u16 RESERVED[5 - 3];        // 04 ~ 05
    u16 sectors;                // 06 obsolete
    u16 RESERVED[9 - 6];        // 07 ~ 09
    u8  serial[20];             // 10 ~ 19 序列号
    u16 RESERVED[22 - 19];      // 10 ~ 22
    u8  firmware[8];            // 23 ~ 26 固件版本
    u8  model[40];              // 27 ~ 46 模型数
    u8  drq_sectors;            // 47 可用扇区数量
    u8  RESERVED[3];            // 48
    u16 capabilities;           // 49 能力
    u16 RESERVED[59 - 49];      // 50 ~ 59
    u32 total_lba;              // 60 ~ 61
    u16 RESERVED;               // 62
    u16 mdma_mode;              // 63
    u8  RESERVED;               // 64
    u8  pio_mode;               // 64
    u16 RESERVED[79 - 64];      // 65 ~ 79 参见 ATA specification
    u16 major_version;          // 80 主版本
    u16 minor_version;          // 81 副版本
    u16 commmand_sets[87 - 81]; // 82 ~ 87 支持的命令集
    u16 RESERVED[118 - 87];     // 88 ~ 118
    u16 support_settings;       // 119
    u16 enable_settings;        // 120
    u16 RESERVED[221 - 120];    // 121 ~ 221
    u16 transport_major;        // 222
    u16 transport_minor;        // 223
    u16 RESERVED[254 - 223];    // 224 ~ 254
    u16 integrity;              // 255 校验和
} _packed ata_identify_data_t;

// 检测错误
static u32 ata_error(ata_bus_t *bus) {
    u8 error = inb(bus->iobase + ATA_IO_ERROR);
    if (error & ATA_ER_AMNF)
        LOGK("Address mark not found.\n");
    if (error & ATA_ER_TK0NF)
        LOGK("Track 0 not found.\n");
    if (error & ATA_ER_ABRT)
        LOGK("Abort.\n");
    if (error & ATA_ER_MCR) 
        LOGK("Media change requested.\n");
    if (error & ATA_ER_IDNF)
        LOGK("Sector id not found.\n");
    if (error & ATA_ER_MC)
        LOGK("Media change.\n");
    if (error & ATA_ER_UNC)
        LOGK("Uncorrectable data error.\n");
    if (error & ATA_ER_BBK)
        LOGK("Bad block.\n");
}

// 忙等待 (mask 用于指定等待的事件，为 ATA_SR_NULL (0) 则直到表示繁忙结束)
static i32 ata_busy_wait(ata_bus_t *bus, u8 mask) {
    // TODO: reset when controller detects error and timeout
    while (true) {
        // 从备用状态寄存器读取状态
        u8 state = inb(bus->ctlbase + ATA_CTL_ALT_STATUS);
        // 如果有错误，则进行错误检测
        if (state & ATA_SR_ERR) {
            ata_error(bus);
            return ATA_SR_ERR;
        }
        // 如果驱动器繁忙，则继续忙等待
        if (state & ATA_SR_BSY) {
            continue;
        }
        // 如果等待事件都触发，则返回
        if ((state & mask) == mask) {
            return 0;
        }
    }
}

// 选择磁盘
static void ata_select_disk(ata_disk_t *disk) {
    outb(disk->bus->iobase + ATA_IO_DEVICE, disk->selector);
    disk->bus->active = disk;
}

// 选择扇区级对应扇区数量
static void ata_select_sector(ata_disk_t *disk, size_t lba, u8 count) {
    outb(disk->bus->iobase + ATA_IO_FEATURE, 0);

    // 读/写扇区数量
    outb(disk->bus->iobase + ATA_IO_SECNR, count);

    // LBA
    outb(disk->bus->iobase + ATA_IO_LBA_LOW, lba & 0xff);
    outb(disk->bus->iobase + ATA_IO_LBA_MID, (lba >> 8) & 0xff);
    outb(disk->bus->iobase + ATA_IO_LBA_HIGH, (lba >> 16) & 0xff);

    // LBA 最高 4 位 + 选择磁盘
    outb(disk->bus->iobase + ATA_IO_DEVICE, disk->selector | ((lba >> 24) & 0xf));

    disk->bus->active = disk;
}

// 读取一个扇区的内容到缓冲区
static void ata_pio_read_sector(ata_disk_t *disk, u16 *buf) {
    for (size_t i = 0; i < (SECTOR_SIZE / sizeof(u16)); i++) {
        buf[i] = inw(disk->bus->iobase + ATA_IO_DATA);
    }
}

// 将缓冲区的内容写入一个扇区
static void ata_pio_write_sector(ata_disk_t *disk, u16 *buf) {
    for (size_t i = 0; i < (SECTOR_SIZE / sizeof(u16)); i++) {
        outw(disk->bus->iobase + ATA_IO_DATA, buf[i]);
    }
}

i32 ata_pio_read(ata_disk_t *disk, void *buf, u8 count, size_t lba) {
    assert(count > 0);          // 保证读取扇区数不为 0
    ASSERT_IRQ_DISABLE();       // 保证为外中断禁止

    ata_bus_t *bus = disk->bus; // 对应的 ATA 总线
    mutexlock_acquire(&bus->lock);  // 获取总线的锁
    
    // 选择磁盘
    ata_select_disk(disk);      

    // 等待就绪
    ata_busy_wait(bus, ATA_SR_DRDY);

    // 选择扇区级对应扇区数量
    ata_select_sector(disk, lba, count);

    // 发送读命令
    outb(bus->iobase + ATA_IO_COMMAND, ATA_CMD_READ);

    // 持续读取磁盘对应的扇区内容直至读取完成
    for (size_t i = 0; i < count; i++) {
        task_t *task = current_task();
        // 如果是初始化时调用硬盘读写功能，则使用同步方式
        if (task->state == TASK_RUNNING) {
            // 阻塞自己直到中断带来表示就绪
            bus->waiter = task;
            task_block(task, NULL, TASK_BLOCKED);
        }
        ata_busy_wait(bus, ATA_SR_DRQ); // 等待 PIO 数据准备完成
        
        u32 offset = ((u32)buf + i * SECTOR_SIZE);
        ata_pio_read_sector(disk, (u16 *)offset);
    }

    mutexlock_release(&bus->lock);
    return 0;
}

i32 ata_pio_write(ata_disk_t *disk, void *buf, u8 count, size_t lba) {
    assert(count > 0);          // 保证读取扇区数不为 0
    ASSERT_IRQ_DISABLE();       // 保证为外中断禁止

    ata_bus_t *bus = disk->bus; // 对应的 ATA 总线
    mutexlock_acquire(&bus->lock);  // 获取总线的锁
    
    // 选择磁盘
    ata_select_disk(disk);      

    // 等待就绪
    ata_busy_wait(bus, ATA_SR_DRDY);

    // 选择扇区级对应扇区数量
    ata_select_sector(disk, lba, count);

    // 发送读命令
    outb(bus->iobase + ATA_IO_COMMAND, ATA_CMD_WRITE);

    // 持续读取磁盘对应的扇区内容直至读取完成
    for (size_t i = 0; i < count; i++) {
        u32 offset = ((u32)buf + i * SECTOR_SIZE);
        ata_pio_write_sector(disk, (u16 *)offset);

        task_t *task = current_task();
        if (task->state == TASK_RUNNING) {
            bus->waiter = task;
            task_block(task, NULL, TASK_BLOCKED);
        }
        ata_busy_wait(bus, ATA_SR_NULL); // 等待写入数据完成
    }

    mutexlock_release(&bus->lock);
    return 0;
}

// 从分区 part 的第 lba 个扇区开始读取
i32 ata_pio_partition_read(ata_partition_t *part, void *buf, u8 count, size_t lba) {
    return ata_pio_read(part->disk, buf, count, part->start_lba + lba);
}

// 从分区 part 的第 lba 个扇区开始写入
i32 ata_pio_partition_write(ata_partition_t *part, void *buf, u8 count, size_t lba) {
    return ata_pio_write(part->disk, buf, count, part->start_lba + lba);
}

// 软件方法重置驱动器
static void ata_reset_derive(ata_bus_t *bus) {
    outb(bus->ctlbase + ATA_CTL_DEV_CONTROL, ATA_CTRL_SRST);
    ata_busy_wait(bus, ATA_SR_NULL);
    outb(bus->ctlbase + ATA_CTL_DEV_CONTROL, 0);
    ata_busy_wait(bus, ATA_SR_NULL);
}

// 字节序由 ATA string 转 C string
static void ata_swap_words(u8 *buf, size_t len) {
    for (size_t i = 0; i < len; i += 2) {
        u8 temp = buf[i];
        buf[i] = buf[i + 1];
        buf[i + 1] = temp;
    }
    buf[len - 1] = EOS;
}

// 识别硬盘
static i32 ata_identify(ata_disk_t *disk, u16 *buf) {
    LOGK("identifing disk %s...\n", disk->name);

    i32 ret;
    mutexlock_acquire(&disk->bus->lock);

    // 选择磁盘
    // ata_select_disk(disk);
    outb(disk->bus->iobase + ATA_IO_DEVICE, disk->selector & (~0x40));

    // 设置 LBA 端口
    ata_select_sector(disk, 0, 0);

    // 硬盘识别命令
    outb(disk->bus->iobase + ATA_IO_COMMAND, ATA_CMD_IDENTIFY);

    // 检测设备是否存在，并等待识别数据准备就绪
    if (inb(disk->bus->ctlbase + ATA_CTL_ALT_STATUS) == 0 ||
        ata_busy_wait(disk->bus, ATA_SR_DRQ) == ATA_SR_ERR
    ) {
        LOGK("disk %s does not exist...\n", disk->name);
        disk->total_lba = 0;
        ret = EOF;
        goto rollback;
    }

    // 获取硬盘识别信息
    ata_pio_read_sector(disk, buf);
    ata_identify_data_t *data = (ata_identify_data_t *)buf;

    // total number of user addressable sectors
    disk->total_lba = data->total_lba;
    LOGK("disk %s total sectors %d\n", disk->name, data->total_lba);

    // serial number
    ata_swap_words(data->serial, sizeof(data->serial));
    LOGK("disk %s serial number %s\n", disk->name, data->serial);

    // firmware revision
    ata_swap_words(data->firmware, sizeof(data->firmware));
    LOGK("disk %s firmware version %s\n", disk->name, data->firmware);

    // model number
    ata_swap_words(data->model, sizeof(data->model));
    LOGK("disk %s model number %s\n", disk->name, data->model);

    ret = 0;

rollback:
    mutexlock_release(&disk->bus->lock);
    return ret;
}

// 磁盘分区
static void ata_partition(ata_disk_t *disk, u16 *buf) {
    // 如果磁盘不可用
    if (disk->total_lba == 0) {
        return;
    }

    // 读取主引导扇区
    ata_pio_read(disk, buf, 1, 0);
    mbr_t *mbr = (mbr_t *)buf;

    // 扫描分区表信息
    for (size_t i = 0; i < ATA_PARTITION_NR; i++) {
        partition_entry_t *entry = &mbr->partition_table[i];
        ata_partition_t *part = &disk->parts[i];

        if (entry->count == 0) continue;

        sprintf(part->name, "%s%d", disk->name, i + 1);
        LOGK("partition %s\n", part->name);
        LOGK("  |__bootable %d\n", entry->bootable);
        LOGK("  |__start %d\n", entry->start_lba);
        LOGK("  |__count %d\n", entry->count);
        LOGK("  |__system 0x%x\n", entry->system);

        // 配置系统分区信息
        part->disk = disk;
        part->system = entry->system;
        part->start_lba = entry->start_lba;
        part->count = entry->count;

        // 扫描扩展分区
        if (entry->system == PARTITION_FS_EXTENDED) {
            LOGK("Unsupport extended partition...\n");
            
            u8 *ebuf = (u8 *)buf + SECTOR_SIZE;
            ata_pio_read(disk, ebuf, 1, entry->start_lba);
            mbr_t *embr = (mbr_t *)ebuf;

            for (size_t j = 0; j < ATA_PARTITION_NR; j++) {
                partition_entry_t *eentry = &embr->partition_table[j];
                if (eentry->count == 0) continue;
                LOGK("partition %d extend %d\n", i, j);
                LOGK("  |__bootable %d\n", eentry->bootable);
                LOGK("  |__start %d\n", eentry->start_lba + entry->start_lba);
                LOGK("  |__count %d\n", eentry->count);
                LOGK("  |__system 0x%x\n", eentry->system);
            }
        }
    }
}

// 硬盘中断处理
void ata_handler(int vector) {
    assert(vector == 0x2e || vector == 0x2f);

    // 向中断控制器发送中断处理结束信号
    send_eoi(vector);

    // 获取中断向量对应的 ATA 总线
    ata_bus_t *bus = &buses[vector - IRQ_MASTER_NR - IRQ_HARDDISK_1];

    // 读取常规状态寄存器，表示中断处理结束
    u8 state = inb(bus->iobase + ATA_IO_STATUS);
    LOGK("hard disk interrupt vector %d state 0x%x\n", vector, state);

    // 如果有等待中断的进程，则取消它的阻塞
    if (bus->waiter) {
        task_unblock(bus->waiter);
        bus->waiter = NULL;
    }
}

static void ata_bus_init() {
    u16 *buf = (u16 *)kalloc_page(1);

    for (size_t bidx = 0; bidx < ATA_BUS_NR; bidx++) {
        ata_bus_t *bus = &buses[bidx];
        sprintf(bus->name, "ata%u", bidx);
        mutexlock_init(&bus->lock);
        bus->active = NULL;
        bus->waiter = NULL;

        if (bidx == 0) {
            // Primary bus
            bus->iobase  = ATA_IOBASE_PRIMARY;
            bus->ctlbase = ATA_CTLBASE_PRIMARY;
        } else {
            // Secondary bus
            bus->iobase  = ATA_IOBASE_SECONDARY;
            bus->ctlbase = ATA_CTLBASE_SECONDARY;
        }

        for (size_t didx = 0; didx < ATA_DISK_NR; didx++) {
            ata_disk_t *disk = &bus->disks[didx];
            sprintf(disk->name, "hdd%c", 'a' + bidx*ATA_BUS_NR + didx);
            disk->bus = bus;

            if (didx == 0) {
                // Master disk
                disk->master = true;
                disk->selector = ATA_MASTER_SELECTOR;
            } else {
                // Slave disk
                disk->master = false;
                disk->selector = ATA_SLAVE_SELECTOR;
            }

            ata_identify(disk, buf);
            memset((void *)buf, 0, PAGE_SIZE);
            ata_partition(disk, buf);
            memset((void *)buf, 0, PAGE_SIZE);
        }
    }

    kfree_page((u32)buf, 1);
}

// ATA 总线和磁盘初始化
void ata_init() {
    LOGK("ata init...\n");
    ata_bus_init();

    // 注册硬盘中断，并取消对应的屏蔽字
    set_interrupt_handler(IRQ_HARDDISK_1, ata_handler);
    set_interrupt_handler(IRQ_HARDDISK_2, ata_handler);
    set_interrupt_mask(IRQ_HARDDISK_1, true);
    set_interrupt_mask(IRQ_HARDDISK_2, true);
    set_interrupt_mask(IRQ_CASCADE, true);
}
