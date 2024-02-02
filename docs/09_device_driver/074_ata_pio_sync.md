# 074 硬盘同步 PIO

**Programmed Input/Output (PIO)** 可编程输入输出，PIO 模式使用了大量的 CPU 资源，因为磁盘和 CPU 之间传输的每个字节的数据都必须通过 CPU 的 IO 端口总线(而不是内存)传送。在某些 CPU 上，PIO 模式仍然可以实现每秒 16 MB 的实际传输速度，但是机器上的其他进程将得不到任何 CPU 时间片。

不过，当计算机刚开始启动时，没有其他进程。因此，在系统进入多任务模式之前，PIO 模式是一个在启动过程中使用的优秀而简单的接口。

> 本节与 [<006 硬盘读写>](../01_bootloader/006_disk_read_write.md) 相关，建议对照阅读。

## 1. 硬件 (Hardware)

> The ATA disk specification is built around an older specification called ST506. With ST506, each disk drive was connected to a controller board by two cables -- a data cable, and a command cable. The controller board was plugged into a motherboard bus. The CPU communicated with the controller board through the CPU's IO ports, which were directly connected to the motherboard bus.

> What the original IDE specification did was to detach the disk controller boards from the motherboard, and stick one controller onto each disk drive, permanently. When the CPU accessed a disk IO port, there was a chip that shorted the CPU's IO bus pins directly onto the IDE cable -- so the CPU could directly access the drive's controller board. The data transfer mechanism between the CPU and the controller board remained the same, and is now called PIO mode. (Nowadays, the disk controller chips just copy the electrical signals between the IO port bus and the IDE cable, until the drive goes into some other mode than PIO.)

## 2. 主从驱动器 (Master/Slave Drives)

只有一条线专用于选择每个总线上的哪个驱动器是活动的。它要么是 高电平 或 低电平，这意味着在任何 ATA 总线上永远不会有超过两个设备运行。它们被称为主设备和从设备，没有特别的原因。它们的功能几乎完全相同。有一个特殊的 IO 端口位允许驱动程序为每个命令字节选择一个驱动器作为目标驱动器。

## 3. 主从总线 (Primary/Secondary Bus)

当前的磁盘控制器芯片几乎总是支持每个芯片两个 ATA 总线。有一组标准化的 IO 端口来控制总线上的磁盘。前两个总线被称为 Primary 和 Secondary ATA 总线，并且几乎总是分别由 IO 端口 `0x1F0` ~ `0x1F7` 和 `0x170` ~ `0x177` 控制(除非您更改它)。相关的设备控制寄存器/备用状态端口分别为 IO 端口 `0x3F6` 和 `0x376`。主总线的标准 IRQ 是 IRQ14，从总线的为 IRQ15。

如果存在另外两个总线，它们通常分别由 IO 端口 `0x1E8` ~ `0x1EF` 和 `0x168` ~ `0x16F` 控制。相关的设备控制寄存器/备用状态端口为 IO 端口 `0x3E6` 和 `0x366`。

每个总线的实际控制寄存器和 IRQs 通常可以通过枚举 PCI 总线、找到所有磁盘控制器并从每个控制器的 PCI 配置空间读取信息来确定。因此，从技术上讲，在 ATA 设备检测之前，应先进行 PCI 枚举。然而，这种方法并不完全可靠。

当系统启动时，根据规格，PCI 磁盘控制器应该处于兼容模式。这意味着它应该使用标准化的 IO 端口设置。你可能别无选择，只能依靠这个事实。

![](./images/ata_pio.drawio.svg)

## 4. 400 纳秒延迟 (400ns delays)

<details>
ATA 规范中建议的发送 ATA 命令的方法告诉您在试图发送命令之前检查 BSY 和 DRQ 位。这意味着在发送下一个命令之前，您需要为正确的驱动器读取状态寄存器(备用状态是一个很好的选择)。这意味着您需要首先选择正确的设备，然后才能读取该状态(然后将所有其他值发送到其他 IO 端口)。这意味着驱动器选择可能总是发生在读取状态之前。这是不好的。

许多驱动器需要一点时间来响应选择命令，并将它们的状态推送到总线上。建议 **读取状态寄存器 15 次**，只关注最后一次返回的值；在选择新的主设备或从设备之后。关键是，你可以假设一个 IO 端口读至少需要 30ns，所以做前 14 个创建 420ns 延迟，这允许驱动时间将正确的电压推到总线上。

读取 IO 端口来创建延迟会浪费大量的 CPU 周期。因此，让您的驱动器记住发送到每个驱动器选择 IO 端口的最后一个值实际上是更聪明的，以避免做不必要的驱动器选择，如果值没有改变。如果您不发送驱动器选择，那么您只需要读取状态寄存器一次。

或者，无论如何，您永远都不希望向已经在为以前的命令提供服务的驱动器发送新命令。如果当前设备正在主动修改 BSY/DRQ/ERR，你的驱动程序总是需要阻塞，并且你的设备驱动程序总是知道设备在那个条件下(因为驱动程序刚刚发送命令给设备，它还没有被标记为“完成”)。一旦驱动器实际完成了一个命令，它将始终清除 BSY 和 DRQ。在下一个 驱动器选择命令之前，您可以简单地验证这一点，在命令完成时，先前选择的设备正确地清除了 BSY 和 DRQ。然后你将永远不需要检查它们是否在设备选择后清除，所以你将不需要在设备选择后读取状态寄存器。

使用 ERR/DF 位写入命令寄存器后也有类似的问题。它们是可以终止命令的两种稍微不同的错误。BSY 和 DRQ 将被清除，但 ERR 或 DF 将一直保持设置，直到您向命令寄存器写入新命令之后。如果您正在使用轮询(见下文)，您应该考虑到这样一个事实:在发送命令字节之后，您对状态寄存器的前四次读取可能仍然意外地设置了 ERR 或 DF 位。(如果你正在使用 IRQ，在 IRQ 服务时状态将总是正确的。)
</details>

## 5. 坏扇区 (Bad Sectors)

<details>
出于实际目的，ATA 磁盘上有三种不同类型的坏扇区：

- 不能写入的扇区(永久性)
- 无法读取的扇区(永久性)
- 无法读取的扇区(临时)

一些磁盘制造商有一个功能，允许磁盘上的少量 “备用” 扇区重新映射到永久坏扇区。然而，该功能是非标准的，完全是制造商特有的。通常，操作系统/文件系统需要为每个驱动器的每个分区保留一个 “坏扇区列表”，并围绕坏扇区进行工作。

如上所述，也有 “临时的坏扇区”。当你读取它们时，你会得到一个硬件错误，就像一个永久坏扇区一样。但是，如果您写入该扇区，则写入将完美地工作，扇区将返回到一个良好的扇区。由于未刷新写缓存、电源峰值或电源故障，可能会发生临时坏扇区。
</details>

## 6. 寻址模式 (Addressing Modes)

目前有三种寻址模式来选择对磁盘进行读写的特定扇区。它们分别是 28 位LBA、48 位LBA 和 CHS。CHS 模式已经过时，但将在下面快速讨论。LBA 模式中的比特数指的是扇区“地址”中的有效比特数，称为 LBA。28 位模式下，0 ~ 0x0FFFFFFF 范围内的 LBA 是合法的。这总共提供了 256M 扇区，或 128GB 可寻址空间。因此，28 位 LBA 模式对于许多当前驱动器来说也是过时的。但是，28 位 PIO 模式比 48 位寻址更快，因此对于不违反最大 LBA 值限制的驱动器或分区来说，它可能是更好的选择。

### 6.1 绝对/相对 LBA (Absolute/Relative LBA)

> All the ATA commands that use LBA addressing require "absolute" LBAs (ie. the sector offset from the very beginning of the disk -- completely ignoring partition boundaries). At first glance, it might seem most efficient to store the LBA values in this same format in your OS. However, this is not the case. It is always necessary to validate the LBAs that are passed into your driver, as truly belonging to the partition that is being accessed. It ends up being smartest to use partition-relative LBA addressing in your code, because you then never need to test if the LBA being accessed is "off the front" of your current partition. So you only need to do half as many tests. This makes up for the fact that you need to add the absolute LBA of the beginning of the current partition to every "relative" LBA value passed to the driver. At the same time, doing this can give you access to one additional LBA address bit. (See the "33 bit LBA" driver code below.)

### 6.2 寄存器 (Registers)

一个 ATA 总线通常有十个 I/O 端口来控制它的行为。

对于 Primary ATA 总线，这些 I/O 端口通常是 `0x1F0` (“I/O”端口基地址) 到 `0x1F7` 和 `0x3F6` (“控制”端口基地址) 到 `0x3F7`。对于 Secondary ATA 总线，它们通常是 `0x170` ~ `0x177` 和 `0x376` ~ `0x377`。

一些系统可能有非标准的 ATA 总线端口位置，在这种情况下，它可能是有帮助的咨询 PCI 部分，以确定如何检索端口地址为系统中的各种设备。

距离 I/O 端口基地址偏移 1 的端口偏移实际上指的是端口 `0x1F1` ($0x1F0 + 1 = 0x1F1$)。这样做是因为端口的基地址可能会根据任何给定系统中的硬件而变化，这也可以保持兼容。另外，这些 I/O 端口中的一些根据它们是被读还是被写映射到不同的寄存器。

> **I/O**

| Primary 通道 | Secondary 通道 | in 操作      | out 操作     | 寄存器大小 (LBA28 / LBA48) |
| ------------ | -------------- | ------------ | ------------ | --------------- |
| 0x1F0        | 0x170          | Data         | Data         | 16-bit / 16-bit |
| 0x1F1        | 0x171          | Error        | Features     | 8-bit / 16-bit |
| 0x1F2        | 0x172          | Sector count | Sector count | 8-bit / 16-bit |
| 0x1F3        | 0x173          | LBA low      | LBA low      | 8-bit / 16-bit |
| 0x1F4        | 0x174          | LBA mid      | LBA mid      | 8-bit / 16-bit |
| 0x1F5        | 0x175          | LBA high     | LBA high     | 8-bit / 16-bit |
| 0x1F6        | 0x176          | Device       | Device       | 8-bit / 8-bit |
| 0x1F7        | 0x177          | Status       | Command      | 8-bit / 8-bit |

- 0x1F0：16bit 端口，用于读写数据
- 0x1F1：检测前一个指令的错误
- 0x1F2：读写扇区的数量
- 0x1F3：起始扇区的 0 ~ 7 位
- 0x1F4：起始扇区的 8 ~ 15 位
- 0x1F5：起始扇区的 16 ~ 23 位
- 0x1F6:
    - 0 ~ 3：起始扇区的 24 ~ 27 位
    - 4: 0 主盘, 1 从盘
    - 6: 0 CHS, 1 LBA
    - 5 和 7：固定为1
- 0x1F7: **out**
    - 0xEC: 识别硬盘
    - 0x20: 读硬盘
    - 0x30: 写硬盘
- 0x1F7: **in**
    - 0 ERR
    - 3 DRQ 数据准备完毕
    - 7 BSY 硬盘繁忙

> **Control**

| Primary 通道 | Secondary 通道 | in 操作      | out 操作     | 寄存器大小 (LBA28 / LBA48) |
| ------------ | -------------- | ------------ | ------------ | --------------- |
| 0x3F6        | 0x376          | 状态寄存器的副本   | 重置总线或开关中断 | 8-bit / 8-bit |
| 0x3F7        | 0x377          | 驱动器地址信息 | 驱动器地址信息 | 8-bit / 8-bit |

详情见 [6.2.4 设备控制寄存器](#624-设备控制寄存器-device-control-register) 和 [6.2.5 驱动器地址寄存器](#625-驱动器地址寄存器-drive-address-register)。

#### 6.2.1 错误寄存器 (Error Register)

> **I/O base + 1**

| 位  | 缩写  | 描述                      |
| --- | ----- | ------------------------- |
| 0   | AMNF  | Address mark not found.   |
| 1   | TKZNF | Track zero not found.     |
| 2   | ABRT  | Aborted command.          |
| 3   | MCR   | Media change request.     |
| 4   | IDNF  | ID not found.             |
| 5   | MC    | Media changed.            |
| 6   | UNC   | Uncorrectable data error. |
| 7   | BBK   | Bad Block detected.       |

#### 6.2.2 状态寄存器 (Status Register)

> **I/O base + 7**

| 位  | 缩写 | 描述                                             |
| --- | ---- | ------------------------------------------------ |
| 0   | ERR  | 标志错误发生                                     |
| 1   | IDX  | 索引，总为 0                                     |
| 2   | CORR | 纠正数据，总为 0                                 |
| 3   | DRQ  | PIO 数据准备完毕                                 |
| 4   | SRV  | 重叠模式服务请求                                 |
| 5   | DF   | 驱动器故障错误                                   |
| 6   | RDY  | 当驱动器停机或发生错误后，位是清除的。否则置位。 |
| 7   | BSY  | 硬盘繁忙                                         |

从技术上讲，当设置了 BSY 时，状态字节中的其他位都是无意义的。测试 “Seek Complete”(DSC) 位通常也是一个坏主意，因为它已被弃用，并被较新的 SRV 位取代。

#### 6.2.3 备用状态寄存器 (Alternate Status Register)

> **Control base + 0** when in / read

读取设备控制寄存器端口会得到备用状态寄存器的值。Alternate Status 的值总是与 Regular Status 端口(在 Primary 总线上是 0x1F7)相同，但是 ***读取 Alternate Status 端口不影响中断***。

#### 6.2.4 设备控制寄存器 (Device Control Register)

> **Control base + 0** when out / write

还有一个额外的 IO 端口改变每个 ATA 总线的行为，称为设备控制寄存器(在主总线上，端口 0x3F6)。每个 ATA 总线都有自己的控制寄存器。


| 位    | 缩写 | 描述                                                               |
| ----- | ---- | ------------------------------------------------------------------ |
| 0     | -    | 总为 0                                                             |
| 1     | nIEN | 将此设置为停止当前设备发送中断                                     |
| 2     | SRST | 设置，然后清除(5us 后)，这对总线上的所有 ATA 驱动器做一个 “软重置” |
| 3 ~ 6 | -    | 保留                                                               |
| 7     | HOB  | 设置为回读发送到 IO 端口的最后一个 LBA48 值的高阶字节              |

所有其他的位都是保留的，并且应该总是清除的。一般来说，您会希望清除 HOB、SRST 和 nIEN，在启动期间，将每个设备控制寄存器设置为 0。

#### 6.2.5 驱动器地址寄存器 (Drive Address Register)

> **Control base + 1**

| 位    | 缩写      | 描述                                     |
| ----- | --------- | ---------------------------------------- |
| 0     | DS0       | 选择驱动器 1，选择驱动器 0 时清除        |
| 1     | DS1       | 选择驱动器 1，选择驱动器 1 时清除        |
| 2 ~ 5 | HS0 - HS3 | 合起来代表当前选定的磁头                 |
| 6     | WTG       | 写门，在写驱动器的过程中降低             |
| 7     | n/a       | 保留与软驱控制器的兼容性，可能使用这个位 |

## 7. 复位驱动器/软件复位 (Resetting a drive / Software Reset)

<details>
对于非 ATAPI 驱动器，驱动器在发生重大错误后复位驱动器的唯一方法是在总线上做“软件复位”。

在适当的控制寄存器中为总线设置位 2 (SRST，值= 4)。这将重置总线上的两个 ATA 设备，然后，你得自己再清除一遍。总线上的主驱动器被自动选择

ATAPI 驱动器在它们的 `LBA_LOW` 和 `LBA_HIGH` I/O 端口上设置值，但不应该重置或甚至终止它们的当前命令。
</details>

## 8. x86 的选择 (x86 Directions)

### 8.1 28 位 PIO

假设您有一个扇区计数字节和一个 28 位 LBA 值。当扇区计数字节值为 0 时，它表示表示拥有 256 个扇区（因为一个字节最大的数为 $2^8 - 1 = 255$）。256 个扇区则表示 $128KB$ 的大小（$256 \times 512Byte = 2^{8+9} = 128KB$）。

注意:当发送一个命令字节时，状态寄存器的 RDY 位是清除的，在 DRQ 设置之前，您可能需要等待(技术上最多 30 秒)驱动器旋转起来。如果是轮询，在读取 Status 的前四次，可能还需要忽略 ERR 和 DF。

<details>
<summary>一个在主总线上读取 28 位 LBA PIO 模式的示例</summary>

1. 发送“主”的 0xE0 或“从”的 0xF0，或 LBA 的最高 4 位到端口 0x1F6
2. 发送 NULL 字节到 0x1F1 端口，如果你喜欢 (它被忽略和浪费大量的 CPU 时间):
3. 发送扇区数量到端口 0x1F2
4. 发送 LBA 的低 8 位到端口 0x1F3
5. 将 LBA 的下一个 8 位发送到端口 0x1F4
6. 发送 LBA 的下一个 8 位到端口 0x1F5
7. 将 READ SECTORS 命令 (0x20) 发送到端口0x1F7
8. 等待 IRQ 或轮询
9. 从 I/O 端口 0x1F0 传输 256 个 16 位值，每次一个 uint16_t。(在汇编中，REP INSW 更加高效)
10. 然后循环返回，等待每个连续扇区的下一个 IRQ (或再次轮询，参见下一条说明)
11. 轮询 PIO 驱动器注意: 将 PIO 数据块的最后一个 uint16_t 转移到数据 IO 端口后，给驱动器一个 400ns 的延迟来重置其 DRQ 位(并可能再次设置 BSY，同时清空/填充驱动器的缓冲区)

注意发送到端口 0x1f6 的 “魔法位”:位 6(值= 0x40)是 LBA 位。这必须为 LBA28 或 LBA48 传输设置。CHS 传输时必须清除。

位 7 和 5 对于当前的 ATA 驱动器是过时的，但必须设置为向后兼容非常老的(ATA1)驱动器。
</details>

#### 8.1.1 写 28 位 LBA

如果使用 28 位 PIO 模式写扇区，需要向命令端口发送 write sectors (`0x30`) 命令。

请勿使用REP OUTSW传输数据，每个 OUTSW 输出 uint16_t 之间必须有微小的延迟，一个 jmp $+2 大小的延迟

确保在每个写命令完成后进行 Cache Flush (ATA 命令 `0xE7`)

### 8.2 48 位 PIO

使用 48 位 PIO 读取扇区与 28 位方法非常相似。它使用 16-bit 来表示扇区计数，类似的，当扇区计数值为 0 时表示拥有 65536 个扇区（因为 16-bit 最大表示的数为 $2^{16} = 65535$）。65536 个扇区则表示 $32MB$ 的大小（$65536 \times 512Byte = 2^{16+9} = 32MB$）。尽量不要连续两次向同一个 IO 端口发送字节。这样做要比对不同的 IO 端口执行两个 outb() 命令慢得多。

重要的是扇区计数、特性和 LBA 字节 4、5 和 6 的高字节在低字节之前进入各自的端口

假设你有一个扇区计数 uint16_t 和一个 6 字节的 LBA 值。脑海中从低到高为 LBA 字节编号为 1 到 6。将 2 字节扇区计数发送到端口 0x1F2 (高字节优先)，并以适当的顺序将 6 个 LBA 字节对发送到端口 0x1F3 到 0x1F5。

<details>
<summary>一个例子</summary>

- 发送 0x40 为“主” 端口或 0x50 为“从”端口 0x1F6: outb(0x1F6, 0x40 | (slavebit << 4))

- outb (0x1F2, sectorcount高字节)

- outb (0 x1f3 LBA4)

- outb (0 x1f4 LBA5)

- outb (0 x1f5 LBA6)

- outb (0x1F2, sectorcount 低字节)

- outb (0 x1f3 LBA1)

- outb (0 x1f4 LBA2)

- outb (0 x1f5 LBA3)

发送“READ SECTORS EXT” 命令 (0x24) 到端口0x1F7 : outb(0x1F7, 0x24)

注意发送到端口 0x1f6 的“魔法位”:位 6(值= 0x40)是 LBA 位。对于 LBA48 命令，任何支持 LBA48 的驱动器将忽略该端口上的所有其他位。如果这将使您的代码更干净，您可以设置它们(使用与 LBA28 相同的魔法位)。

如果要在 48 位 PIO 模式下写扇区，则发送命令 “write sectors EXT”(0x34)。(和以前一样，在编写时不要使用 REP OUTSW) 记住在每个写命令完成后进行缓存刷新

命令字节发送后，以与 28 位 PIO 读写命令完全相同的方式传输每个扇区的数据
</details>

## 9. CHS 模式 (CHS Mode)

<details>
柱面 (Cylinder)，磁头(Head)，扇区(Sector) 模式已经完全过时了，但由于遗留原因，有一些关于它的事情需要了解。

最古老的驱动器有许多玻璃“盘片”，每个盘片有两个读/写“磁头”。头总是垂直排成一行。其中一个盘子的一个头通常用于“计时”。当所有的盘面旋转时，每个盘头勾画出一个圆圈，所有的盘头一起勾画出一个“圆柱体”。每个头划出的圆圈又细分为若干个“扇区”。每个扇区可用于存储 512 字节的数据。选择圆柱、磁头和扇区成为一种寻址模式。

更换柱面意味着移动整个头部总成，这是要避免的，如果可能的话

但重要的是，这些信息在过去 20 年里都不是真的——除了计算机一直通过人工 CHS 寻址访问数据

在 CHS 模式中，每个驱动器都有一个“几何形状”，CHS值的合法范围。典型的最大合法值是 Cyl = 0 ~ 1023, Head = 0 ~ 15, Sector = 1 ~ 63。

请注意，扇区= 0 总是非法的，这是导致错误的常见原因。(也有可能一些硬件/驱动器将接受高达 65537 的 Cylinder 值)

将 CHS 地址转换为 LBA 是很简单的:(Cylinder * TotalHeads + SelectedHead) * SectorsPerTrack + (SectorNum - 1)。有时程序会要求一个 CHS 地址，你需要手工进行计算。

在 CHS 模式下访问扇区基本上与进行 28 位 LBA 读写相同，除了在写入 bit Flag s端口时保持 LBA 位(值= 0x40)关闭，并且向IO 端口发送各种 CHS 字节而不是 LBA 字节。
</details>

## 10. 代码分析

和 [<006 硬盘读写>](../01_bootloader/006_disk_read_write.md) 一致，本节采用的也是 **28 位 LBA** 的寻址方式。

再贴一下设备模型：

![](./images/ata_pio.drawio.svg)

事实上我们也可以从 bochsrc 中找到对应的逻辑：

```bash
ata0: enabled=true, ioaddr1=0x1f0, ioaddr2=0x3f0, irq=14
ata0-master: type=disk, path="target/master.img", mode=flat
ata0-slave: type=none
ata1: enabled=true, ioaddr1=0x170, ioaddr2=0x370, irq=15
ata1-master: type=none
ata1-slave: type=none
ata2: enabled=false
ata3: enabled=false
```

可以看到也是可以使能了两条 ATA 总线：ata0 和 ata1，每条总线都有 master 和 slave 两个磁盘，对应的 I/O 端口基址也与原理一致，还指定了 irq 号，这个我们会在下一节使用到。bochs 还额外支持 2 条总线：ata3 和 ata4，但目前未使能。

### 10.1 磁盘和总线

按照 ATA PIO 对应的设备模型，定义相关常量：

> 以下代码位于 `include/xos/ata.h`

```c
// 扇区大小 (512 Byte)
#define SECTOR_SIZE 512
// ATA 总线数量
#define ATA_BUS_NR 2
// 每条 ATA 总线可以挂载的磁盘数量
#define ATA_DISK_NR 2
```

从 CPU 的角度来看，操作 I/O 设备都是通过 I/O 端口来实现的；但从内核角度来看，将这些设备抽象成“对象”，使用面向对象的写法来实现比较直观和具有扩展性，例如将总线与对应的端口、磁盘关联起来。

磁盘对应的结构体：

```c
// ATA 磁盘
typedef struct ata_disk_t {
    char name[8];           // 磁盘名称
    struct ata_bus_t *bus;  // 所在的 ATA 总线
    u8 selector;            // 磁盘选择信息
    bool master;            // 是否为主盘
} ata_disk_t;
```

总线对应的结构体，一条总线挂载两个磁盘，并拥有一组寄存器：

```c
// ATA 总线
typedef struct ata_bus_t {
    char name[8];                   // 总线名称
    mutexlock_t lock;               // 总线互斥锁
    u16 iobase;                     // 总线 I/O 寄存器基址
    u16 ctlbase;                   // 总线控制寄存器基址
    ata_disk_t disks[ATA_DISK_NR];  // 挂载的磁盘
    ata_disk_t *active;             // 当前选择的磁盘
} ata_bus_t;
```

这里解释一下 `ata_disk_t` 中的 `selector` 字段，这个字段是用于向总线的 Devide 寄存器 (I/O base + 6) 发送信息来选择磁盘。根据说明，Device 寄存器的Bit 5 和 Bit 7 固定为 1，Bit 6 由于采用的是 LBA 所以也为 1，而 Bit 4 根据是否选择主盘而为 0 或 1，而剩下的位在选择磁盘时并不需要用到（当然进行 LBA 寻址还是需要用到）。所以对于任意总线，如果选择主盘需要向 Device 寄存器发送 `0b11100000`，选择从盘则发送 `0b11110000`。我们将这些位元序列，在初始化时保存在磁盘结构体，可以节省我们在操作时的一些额外判断，可以直接提取对应磁盘的 `selector` 字段对该磁盘进行选择。

> 为什么需要进行磁盘选择？
> ---
> 因为总线上的两个磁盘共用一组寄存器，而对磁盘进行操作都需要通过这一组寄存器。如果不进行磁盘选择就对寄存器进行操作，结果是不确定的，因为这些操作都会作用于上一次磁盘选择时所选择的磁盘，而上一次选择的磁盘不一定就是我们这一次想操作的磁盘。

另外解释一下 `ata_bus_t` 中的互斥锁 `lock`，为什么需要锁机制？因为对于磁盘或者设备，多个线程都可能想曲操作，这样的话磁盘或设备就是一个临界区，会产生数据竞争，所以需要锁机制。

其它的就很简单了，像 `name` 字段，对于磁盘可能是 `master` 或 `slave`，对于总线可能是 `primary` 或 `secondary`。

> 对于磁盘和总线这种互相关联的结构体，源代码中使用到了 **forward declaration** 这一技术来实现，可以阅读一下 😎

声明一下本节需要实现的功能：

```c
// 从磁盘 disk 的第 lba 个扇区开始，读取连续 count 个扇区的数据到缓冲区 buf
i32 ata_pio_read(ata_disk_t *disk, void *buf, u8 count, size_t lba);

// 将缓冲区 buf 的数据写入磁盘 disk 的第 lba 个扇区开始的连续 count 个扇区
i32 ata_pio_write(ata_disk_t *disk, void *buf, u8 count, size_t lba);
```

注意到，这里的参数扇区数量 `count` 是用 `u8` 来表示的，这是没问题的，因为 Count 寄存器 (I/O base 2) 在 28 位 LBA 下也是 8-bit 的。至于参数 `lba` 为 `size_t` 也是没问题的，因为 28 位的 LBA 显然是可以被包含在 `size_t` (32-bit) 中的。

因为 CPU 操纵 I/O 设备是通过 I/O 端口实现的，所以需要根据原理说明定义一些 I/O 端口相关的常量，由于这部分篇幅过长且比较直观，这里就不贴出来了，相关定义位于 `kernel/ata.c` 的开始处。

> 以下代码均位于 `kernel/ata.c`

### 10.2 读取磁盘

读取磁盘的逻辑很简单：
1. 选择对应的磁盘
2. 等待总线就绪
3. 选择数据对应的扇区以及扇区数量
4. 发送读命令
5. 持续读取磁盘对应的扇区内容直至读取完成
6. 每次读取一个扇区之前都必须保证对应 PIO 数据准备完成

```c
i32 ata_pio_read(ata_disk_t *disk, void *buf, u8 count, size_t lba) {
    assert(count > 0);          // 保证读取扇区数不为 0
    ata_bus_t *bus = disk->bus; // 对应的 ATA 总线
    mutex_acquire(&bus->lock);  // 获取总线的锁
    
    // 选择磁盘
    ata_select_disk(disk);      

    // 等待就绪
    ata_busy_wait(bus, 0);

    // 选择扇区级对应扇区数量
    ata_select_sector(disk, lba, count);

    // 发送读命令
    outb(bus->iobase + ATA_IO_COMMAND, ATA_CMD_READ);

    // 持续读取磁盘对应的扇区内容直至读取完成
    for (size_t i = 0; i < count; i++) {
        ata_busy_wait(bus, ATA_SR_DRQ); // 等待 PIO 数据准备完成
        u32 offset = ((u32)buf + i * SECTOR_SIZE);
        ata_pio_read_sector(disk, (u16 *)offset);
    }

    mutex_release(&bus->lock);
    return 0;
}
```

这里用到了一个函数 `ata_pio_read_sector()`，它的作用是通过设备的 DATA 端口，每次 2 Byte 的传送扇区的内容到指定的缓冲区。

```c
static void ata_pio_read_sector(ata_disk_t *disk, u16 *buf) {
    for (size_t i = 0; i < (SECTOR_SIZE / sizeof(u16)); i++) {
        buf[i] = inw(disk->bus->iobase + ATA_IO_DATA);
    }
}
```

### 10.3 写入磁盘

写入磁盘逻辑和读取磁盘差不多，主要区别在于忙等待的时机。
1. 选择对应的磁盘
2. 等待总线就绪
3. 选择数据对应的扇区以及扇区数量
4. 发送写命令
5. 持续写入磁盘对应的扇区直至写入完成
6. 每次写入一个扇区之后都必须保证不繁忙再开始写入下一个扇区

```c
i32 ata_pio_write(ata_disk_t *disk, void *buf, u8 count, size_t lba) {
    assert(count > 0);          // 保证读取扇区数不为 0
    ata_bus_t *bus = disk->bus; // 对应的 ATA 总线
    mutexlock_acquire(&bus->lock);  // 获取总线的锁
    
    // 选择磁盘
    ata_select_disk(disk);      

    // 等待就绪
    ata_busy_wait(bus, 0);

    // 选择扇区级对应扇区数量
    ata_select_sector(disk, lba, count);

    // 发送读命令
    outb(bus->iobase + ATA_IO_COMMAND, ATA_CMD_WRITE);

    // 持续读取磁盘对应的扇区内容直至读取完成
    for (size_t i = 0; i < count; i++) {
        u32 offset = ((u32)buf + i * SECTOR_SIZE);
        ata_pio_write_sector(disk, (u16 *)offset);
        ata_busy_wait(bus, 0); // 等待写入数据完成
    }

    mutexlock_release(&bus->lock);
    return 0;
}
```

```c
static void ata_pio_write_sector(ata_disk_t *disk, u16 *buf) {
    for (size_t i = 0; i < (SECTOR_SIZE / sizeof(u16)); i++) {
        outw(disk->bus->iobase + ATA_IO_DATA, buf[i]);
    }
}
```

### 10.4 辅助功能

这些功能都比较直观，对照原理说明理解即可。代码均位于 `kernel/ata.c` 处。

#### 10.4.1 检测错误

```c
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
```

#### 10.4.2 忙等待

```c
// 忙等待 (mask 用于指定等待的事件，为 0 则直到表示繁忙结束)
static i32 ata_busy_wait(ata_bus_t *bus, u8 mask) {
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
```

这里解释一下为什么需要读取 **备用状态寄存器**。因为直接读取状态寄存器会消除设备的中断状态，一般用读取状态寄存器来告知设备中断已经处理完成。而我们下一节需要使用中断方式来实现 PIO 的异步方式，所以为了不影响后续的中断，就这么处理。（而且如果这里直接读取状态寄存器好像会出 bug）

注意检测驱动器繁忙逻辑必须在等待事件之前（考虑一下传入的 mask 为 0 的情况）

#### 10.4.3 选择磁盘、扇区

选择磁盘：
```c
static void ata_select_disk(ata_disk_t *disk) {
    outb(disk->bus->iobase + ATA_IO_DEVICE, disk->selector);
    disk->bus->active = disk;
}
```

选择扇区：
```c
static void ata_select_sector(ata_disk_t *disk, size_t lba, u8 count) {
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
```

### 10.5 初始化

初始化的逻辑也很简单：对每条 ATA 总线进行遍历，设置总线对应的端口等信息，然后对总线挂载的磁盘进行初始化；磁盘初始化逻辑也类似，设置一些相关信息。

```c
static void ata_bus_init() {
    for (size_t bidx = 0; bidx < ATA_BUS_NR; bidx++) {
        ata_bus_t *bus = &buses[bidx];
        sprintf(bus->name, "ata%u", bidx);
        mutex_init(&bus->lock);
        bus->active = NULL;

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
}
```

在 `kernel/main.c` 中加入相关逻辑：

```c
void kernel_init() {
    ...
    ata_init();
    ...
}
```

## 11. 功能测试

为了测试方便，我们直接在 `ata_init()` 中进行测试（同时将进程运行信息注释掉，避免干扰）：

```c
void ata_init() {
    LOGK("ata init...\n");
    ata_bus_init();

    void *buf = (void *)kalloc_page(1);
    // BP 0
    LOGK("read buffer 0x%p\n", buf);
    ata_pio_read(&buses[0].disks[0], buf, 1, 0);
    // BP 1    
    memset(buf, 0x5a, SECTOR_SIZE);
    // BP 2
    ata_pio_write(&buses[0].disks[0], buf, 1, 1);
    LOGK("write buffer 0x%p\n", buf);
    // BP 3
    kfree_page((u32)buf, 1);
}
```

预期为在断点：
- BP1 处：通过 VS Code 调试控制台输入指令 `-exec x/512xb buf`，可以查看到 `buf` 已经存储第 0 个扇区（即 MBR 扇区）的内容（最简单的判断方式为最后 2 个字节为 0xaa55）。
- BP2 处：通过 Hex Editor 插件查看 master.img 镜像（位于 target/ 目录下），可以查看到第 1 个扇区的内容已经全部变为 0x5a（第 1 个扇区起始于 Hex Editor 标注 0x00000200 地址，因为一个扇区为 512 字节）。


> 直接修改第 1 个扇区的数据没有问题吗？
> ---
> 没有问题的，因为第 0 个扇区是 MBR，loader 则存放于第 2 个扇区开始的连续 4 个扇区，所以第 1 个扇区是空闲的，修改它是没有问题的。

## 12. 参考文献

- <https://wiki.osdev.org/ATA_PIO_Mode>
- <https://wiki.osdev.org/PCI_IDE_Controller>
- Information Technology - AT Attachment - 8 ATA/ATAPI Command Set (ATA8-ACS)
- <http://www.bswd.com/pciide.pdf>
- <http://bswd.com/idems100.pdf>