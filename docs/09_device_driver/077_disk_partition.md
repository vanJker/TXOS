# 077 硬盘分区

为了实现多个操作系统共享硬盘资源，硬盘可以在逻辑上分为 4 个主分区。每个分区之间的扇区号是邻接的。

分区表 (partition table) 由 4 个表项组成，每个表项由 16 字节组成，对应一个分区的信息，存放有分区的大小和起止的柱面号、磁道号和扇区号，所以分区表总共占据 $4 \times 16 = 64 Bytes$。

分区表存放在硬盘的 0 柱面第 0 磁头第 1 个扇区的 0x1BE ~ 0x1FD 处 (这部分总共有 $0x1FD - 0x1BE + 1 = 0x40 = 64\ Bytes$ 与前面一致)。

| 位置 | 大小  | 名称           | 说明                                    |
| ---- | ----- | -------------- | --------------------------------------- |
| 0x00 | 8 位  | bootable       | 引导标志 / 0 - 不可引导 / 0x80 - 可引导 |
| 0x01 | 8 位  | start_head     | 分区起始磁头号                          |
| 0x02 | 6 位  | start_sector   | 分区起始扇区号                          |
| 0x03 | 10 位 | start_cylinder | 分区起始柱面号                          |
| 0x04 | 8 位  | system         | 分区类型字节 / 用于表示文件系统         |
| 0x05 | 8 位  | end_head       | 分区的结束磁头号                        |
| 0x06 | 6 位  | end_sector     | 分区结束扇区号                          |
| 0x07 | 10 位 | end_cylinder   | 分区结束柱面号                          |
| 0x08 | 32 位 | start          | 分区起始物理扇区号 lba                  |
| 0x0c | 32 位 | count          | 分区占用的扇区数                        |

## 1. 主引导扇区的结构

主引导扇区 (MBR) 是硬盘的第 0 柱面第 0 磁头第 1 个扇区，所以在主引导扇区里有分区表 (partition table)

- 代码：446B
- 硬盘分区表：$64B = 4 \times 16B$
- 魔数：0xaa55 (0x55 0xaa)

## 2. 扩展分区

扩展分区是一种可以多加 4 个分区的方式。如果分区表中的 SystemID 字段的值位 0x5，表示该分区为扩展分区。

可以将扩展分区的所有扇区组合起来认为是一个新的磁盘，然后再对其进行分区，这种逻辑有点套娃，所以如果磁盘空间足够大，理论上可以分出无数个分区。

![](./images/partition.drawio.svg)

## 3. 创建硬盘分区

可以通过 `fdisk` 命令对磁盘进行分区，具体参考 <https://wiki.archlinux.org/title/Fdisk>

> 由于参考资料写的更加详细，如果看不懂下面的例子，请务必阅读上面的参考资料！

可以将分好区的分区信息备份：

```bash
$ sfdisk -d /dev/... > master.sfdisk
```

有分区信息可以直接对磁盘进行分区：

```bash
$ sfdisk /dev/... < master.sfdisk
```

手动对磁盘进行分区请参考 [4 Create a partition table and partitions](https://wiki.archlinux.org/title/Fdisk#Create_a_partition_table_and_partitions)：

> **善用 `m` 调出帮助手册**

```bash
$ fdisk /dev/...
```

4.2 Create partitions
> Create a new partition with the n command. You must enter a MBR partition type, partition number, starting sector, and an ending sector.

4.2.1 Partition type
> When using MBR, fdisk will ask for the MBR partition type. Specify it, type p to create a primary partition or e to create an extended one. There may be up to four primary partitions.
>
> fdisk does not ask for the partition type ID and uses 'Linux filesystem' by default; you can change it later.

4.2.3 First and last sector
> The first sector must be specified in absolute terms using sector numbers. The last sector can be specified using the absolute position in sector numbers or as positions measured in kibibytes (K), mebibytes (M), gibibytes (G), tebibytes (T), or pebibytes (P);

可以将磁盘挂载到系统：
    
```bash
$ sudo losetup /dev/loop0 --partscan master.img
```

- `/dev/loop0` 表示将设备 `master.img` 挂载到系统设备路径 `dev/loop0`
- `--partscan` 表示扫描分区表，即挂载磁盘同时加载分区到系统

或者取消挂载：

```bash
$ sudo losetup -d /dev/loop0
```

可以列出已挂载的设备信息：

```bash
$ lsblk
```

man 8 lsblk
> lsblk lists information about all available or the specified block devices.

## 4. GPT

GPT是一个更先进的分区标准，在 UEFI 下被作为推荐的分区机制。它不包含 MBR 分区表的人为 24 位或 32 位限制。它还增强了分区表的概念，通常比 MBR 方案复杂得多。

## 5. master.img 分区

根据上面说明的 `fdisk` 等命令，我们对系统镜像 `master.img` 进行一下手动分区：

```bash
$ fdisk ./target/master.img
...
# 创建一个主分区
Command (m for help): n
Partition type: p
...
Last sector: +8M

# 创建一个扩展分区
Command (m for help): n
Partition type: e
...
Last sector:

# 在扩展分区里创建一个主分区（因为之前创建扩展分区已经用完主分区的空间了，只能在扩展分区里创建主分区了）
Command (m for help): n
Partition type: p
...
Last sector:

# 打印一下当前的分区表
Command (m for help): p
Disk target/master.img: 15.75 MiB, 16515072 bytes, 32256 sectors
Units: sectors of 1 * 512 = 512 bytes
Sector size (logical/physical): 512 bytes / 512 bytes
I/O size (minimum/optimal): 512 bytes / 512 bytes
Disklabel type: dos
Disk identifier: 0x00000000

Device             Boot Start   End Sectors  Size Id Type
target/master.img1       2048 18431   16384    8M 83 Linux
target/master.img2      18432 32255   13824  6.8M  5 Extended
target/master.img5      20480 32255   11776  5.8M 83 Linux
```

然后我们可用通过 `losetup` 命令将 master.img 挂载到主机 Arch 系统上的 loop0 设备上：

```bash
# 挂载设备
$ sudo losetup /dev/loop0 --partscan master.img
# 查看已挂载的设备信息
$ lsblk
NAME      MAJ:MIN RM   SIZE RO TYPE MOUNTPOINTS
loop0       7:0    0  15.8M  0 loop 
├─loop0p1 259:0    0     8M  0 part 
├─loop0p2 259:1    0     1K  0 part 
└─loop0p5 259:2    0   5.8M  0 part 
sda
├─sda1    ...
....
# 取消挂载
$ sudo losetup -d /dev/loop0
```

本节的内容就是对上面创建分区结构进行测试，测试系统是否能准确获取分区表信息。但是每次都需要手动创建这样分区结构实在是过于繁琐，幸好上面提到可用通过 `sfdisk` 命令来对已有的分区信息进行导入导出，非常方便。

导出 / 备份刚创建的分区结构：

```bash
$ sfdisk -d master.img > src/utils/master.sfdisk
```

在 Makefile 中加入对 master.img 导入分区信息逻辑：

```makefile
$(TARGET)/master.img: ... $(SRC)/utils/master.sfdisk
    ...
# 对硬盘进行分区
	sfdisk $@ < $(SRC)/utils/master.sfdisk
```

## 6. 代码分析

### 6.1 分区信息

根据分区表的结构，定义相关的结构体：

```c
//--> include/xos/ata.h

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
```

通过上面两个结构体，我们可以解析硬盘的主引导扇区以及其中的分区表，但是对于解析得到的分区表，我们并不需要其所有的信息，仅仅需要一部分信息即可。所以定义一个结构体 `ata_partition_t` 用于给系统持有其所需要的分区信息，例如分区的文件系统，分区起始扇区的 LBA 和分区占有的扇区数等，同时加入一些系统管理驱动的相关信息，例如分区所在的磁盘、分区名称等。

> 这是驱动开发的一个常用技巧，将硬件部分的数据（例如这里的磁盘分区表）与系统在内存持有的设备信息（例如这里的 `ata_partition_t` 结构体）进行关联，系统在运行时只需使用其拥有的设备信息（例如 `ata_partition_t` 即可）。

```c
//--> include/xos/ata.h

// 每个磁盘的分区数量（目前只支持主分区，所以共 4 个）
#define ATA_PARTITION_NR 4

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
```

上面的文件系统标识可以在 [这里](https://www.win.tue.nl/~aeb/partitions/partition_types-1.html) 获取到。

由于我们设定每个磁盘拥有 4 个分区，所以在系统的磁盘设备结构体 `ata_disk_t` 中加入相关字段：

```c
//--> include/xos/ata.h

// ATA 磁盘
typedef struct ata_disk_t {
    ...
    ata_partition_t parts[ATA_PARTITION_NR]; // 硬盘分区
} ata_disk_t;
```

### 6.2 分区读写

系统在拥有磁盘的分区信息之后，很容易就能实现对指定分区进行读写：

```c
//--> kernel/ata.c

// 从分区 part 的第 lba 个扇区开始读取
i32 ata_pio_partition_read(ata_partition_t *part, void *buf, u8 count, size_t lba) {
    return ata_pio_read(part->disk, buf, count, part->start_lba + lba);
}

// 从分区 part 的第 lba 个扇区开始写入
i32 ata_pio_partition_write(ata_partition_t *part, void *buf, u8 count, size_t lba) {
    return ata_pio_write(part->disk, buf, count, part->start_lba + lba);
}
```

函数参数的 LBA 是相对于该分区起始扇区的 LBA，所以需要通过 `part->start_lba + lba` 转换成磁盘的绝对 LBA。

### 6.3 磁盘分区

为了将硬盘的分区数据与系统内存的设备信息进行关联，我们需要从硬盘读取、解析分区数据，然后将其保存到系统内存对于的分区信息里。这样系统下次就可以直接使用内存中保存的分区信息来操作相应的分区，而无需再进行低效的硬盘读写来获取分区数据，然后操作分区。

这样的操作类似于上一节的硬盘读写，我们这里实现一个硬盘分区功能。系统对一个指定的磁盘关联分区信息的逻辑如下：

- 读取该磁盘的主引导扇区
- 扫描主引导扇区的分区表
- 对分区表的 4 个表项在内存中进行相应地记录、处理

```c
//--> kernel/ata.c

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
    }
}
```

接下来需要对扩展分区进行特殊处理，我们的系统暂时不支持扩展分区，所以对扩展分区的处理仅仅是：将扩展分区视为主引导扇区，并输出一下其内部的分区信息。

```c
//--> kernel/ata.c

static void ata_partition(ata_disk_t *disk, u16 *buf) {
    ...
    for (size_t i = 0; i < ATA_PARTITION_NR; i++) {
        ...
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
```

这里我们将扩展分区内的主分区的起始扇区 LBA，通过 `eentry->start_lba + entry->start_lba` 转换成磁盘的绝对 LBA（这样可以方便后面我们进行调试测试）。

这里需要注意，由于我们在处理完扩展分区后，还需要使用原先主引导扇区的分区表来处理磁盘的下一个分区，所以不能直接覆盖 `buf` 的原有内容，这里使用 `u8 *ebuf = (u8 *)buf + SECTOR_SIZE;` 来存放扩展分区的内存。由于一个主引导扇区是 512 字节，所以请保证传入参数 `buf` 至少拥有 $512 + 512 = 1024\ Bytes$ 的有效内存。

与硬盘识别类似，我们在磁盘初始化时，加入硬盘分区的相应处理逻辑：

```c
//--> kernel/ata.c

static void ata_bus_init() {
    u16 *buf = (u16 *)kalloc_page(1);

    for (size_t bidx = 0; bidx < ATA_BUS_NR; bidx++) {
        ...
        for (size_t didx = 0; didx < ATA_DISK_NR; didx++) {
            ...
            ata_partition(disk, buf);
            memset((void *)buf, 0, PAGE_SIZE);
        }
    }

    kfree_page((u32)buf, 1);
}
```

## 7. 功能测试

在 `ata_bus_init` 的 `ata_partition` 处进行断点，观察内核输出的硬盘分区信息，并与 `src/utils/master.sfdisk` 中的分区信息进行对比，确保系统正确处理硬盘分区信息。

## 8. 参考文献

- <https://wiki.osdev.org/Partition_table>
- <https://wiki.osdev.org/MBR_(x86)>
- <https://www.win.tue.nl/~aeb/partitions/partition_types-1.html>
- [郑刚 / 操作系统真象还原 / 人民邮电出版社 / 2016](https://book.douban.com/subject/26745156/)
- [赵炯 / Linux内核完全注释 / 机械工业出版社 / 2005](https://book.douban.com/subject/1231236/)
- <https://wiki.archlinux.org/title/Fdisk>
- <https://www.win.tue.nl/~aeb/partitions/partition_types-1.html>