#include <xos/ata.h>
#include <xos/stdio.h>
#include <xos/io.h>
#include <xos/interrupt.h>
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

// 忙等待 (mask 用于指定等待的事件，为 0 则直到表示繁忙结束)
static i32 ata_busy_wait(ata_bus_t *bus, u8 mask) {
    while (true) {
        // 从备用状态寄存器读取状态
        u8 state = inb(bus->ctlbase + ATA_CTL_ALT_STATUS);
        // 如果有错误，则进行错误检测
        if (state & ATA_SR_ERR) {
            ata_error(bus);
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

// 磁盘中断处理
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
        }
    }
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
