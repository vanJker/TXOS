# 075 硬盘异步 PIO

由于同步状态检测，消耗了大量的 CPU 资源，所以可以使用异步的方式来等待硬盘驱动器。给驱动器发送完读写命令后，进程可以进入阻塞态，当驱动器完成一个扇区的操作 (读/写) 时，会发送中断，可以在中断中恢复进程到就绪态，继续执行。

## 1. 中断

注意：当命令以错误结束时，它不会生成 IRQ。每秒检查几次备用状态寄存器是明智的，以查看是否设置了 ERR 位。否则，直到命令超时您才会知道。

## 2. 处理中断

在早期，IRQ 的唯一目的是通知 IRQ 处理程序驱动器已经准备好发送或接受数据。我们的期望是 IRQ 处理程序本身将立即对下一个数据块执行基于 PIO 的数据传输。现在事情没那么简单了。总线上的一个或两个驱动器可能处于 DMA 模式，或者数据块大小不是 256 个 16 位值。此外，现在更强调以尽可能快的速度从 IRQ 处理程序例程中返回。所以问题是：IRQ处理程序需要做的最小操作集是什么?

如果您正在使用 IRQ 共享，您将需要检查 PCI 总线主状态字节，以验证 IRQ 来自磁盘。如果是这样，就需要读取常规状态寄存器一次，以使磁盘清除其中断标志。如果状态寄存器中的 ERR 位被设置(位 0，值 = 1)，您可能想从错误 IO 端口(主总线上 0x1F1)读取并保存 **错误详细信息** 值。

如果传输是一个 READ DMA 操作，则必须从总线主状态寄存器读取值。因为 IRQ 处理程序可能不知道操作是否是 DMA 操作，所以在所有 IRQ 之后 (如果总线是由 PCI 控制器控制的，几乎肯定是这样)，您可能会检查 Busmaster Status 字节。如果该字节设置了 ERR 位(位 1，值 = 2)，您可能希望将当前值保存在磁盘的 LBA IO 端口中，它们可以告诉您驱动器上哪个扇区生成了错误。您还需要清除错误位，方法是向其写入一个 2。

您还需要向两个 PIC 发送 EOI (0x20)，以清除它们的中断标志。然后，您需要设置一个标志来“解除”驱动程序，并让它知道发生了另一个 IRQ ——这样驱动程序就可以进行任何必要的数据传输。

注意：如果你仍然处于单请求模式，并且只在 PIO 模式下轮询常规状态寄存器，那么 IRQ 处理器需要做的唯一一件事就是向 PCI 发送 EOI。您甚至可能想要设置控制寄存器的 nIEN 位，以尝试完全关闭磁盘 IRQ。

## 3. 轮询状态 VS 中断

当驱动发出 PIO 读写命令时，需要等待驱动准备好后才能传输数据。有两种方法可以知道驱动器何时准备好接收数据。当它就绪时，驱动器将发送一个 IRQ。或者，驱动程序可以轮询其中一个状态端口(常规或备用状态)。

轮询有两个优点:

- 轮询的响应速度比 IRQ 快
- 轮询的逻辑比等待 IRQ 简单得多

还有一个巨大的缺点：

- 在多任务环境中，轮询会耗尽所有CPU时间
- 但是，在单请求模式下，这不是问题(CPU 没有更好的事情可做)，所以轮询是一件好事

如何轮询(等待驱动器准备传输数据):

- 读取常规状态端口，直到第 7 位(BSY，值= 0x80)清除，和第 3 位(DRQ，值= 8)设置
- 或直到第 0 位(ERR，值= 1) 或 第 5 位(DF，值= 0x20)设置。
- 如果两个错误位都没有设置，那么设备就已经准备好了

## 4. 抢占/防止中断触发

如果驱动程序在向驱动器发送命令后读取了常规状态端口，则 **响应** IRQ 可能永远不会发生

如果您想要接收 IRQs，那么总是读取 **备用状态端口**，而不是常规状态端口。但有时 IRQ 只是浪费资源，让它们消失可能是一个好主意。

防止 ATA IRQs 发生的更完整的方法是在特定选定驱动器的控制寄存器中设置 nIEN 位。这应该会阻止总线上的驱动器发送任何 IRQs，直到你再次清除该位。

然而，它可能并不总是有效，一些程序员报告了 nIEN 运行时的问题。当 nIEN 是总线上的选定驱动器时，驱动器只响应新写入的 nIEN 值。

也就是说，如果选择了一个驱动器，并且您设置了 nIEN，然后选择带有驱动器选择寄存器的另一个驱动器，然后清除 nIEN ——然后第一个驱动器应该永远“记住”它被告知不要发送 irq ——直到您再次选择它，并在控制寄存器中写入一个 0 到 nIEN 位。

## 5. 读写多块

在多任务 PIO 模式下，尝试减少 IRQ 数量的一种方法是使用 READ MULTIPLE (0xC4) 和 WRITE MULTIPLE (0xC5) 命令。这

些命令使驱动器缓冲区的扇区“块”，并且每个块只发送一个 IRQ，而不是每个扇区发送一个 IRQ。参考 IDENTIFY 命令的 uint16_t 47 和 59 来确定一个块中扇区的数量。您还可以尝试使用 SET MULTIPLE MODE (0xC6) 命令来更改每个块的扇区。

注意:总的来说，PIO 模式是一种慢速传输方式。在实际工作条件下，几乎任何驱动器都应该由 DMA 驱动器控制，而不应该使用 PIO。试图通过抢占 IRQ(或任何其他方法)来加快 PIO 模式的速度基本上是在浪费时间和精力。但是，400MB 或更小的 ATA 驱动器可能不支持多字 DMA 模式0。如果您希望支持这种大小的驱动器，那么在 PIO 模式驱动程序上花点功夫可能是值得的。

## 6. 代码分析

本节实现逻辑比较简单，主要是讲原有的 **忙等待** 改为 **阻塞中断后唤醒** 机制。

根据 [<033 外中断控制器>](../04_interrupt_and_clock/033_irq_controller.md) 一节的 8259a 示意图：

![](../04_interrupt_and_clock/images/8259a.drawio.svg)

以及 bochsrc 配置文件：

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

可以分配中断向量 0x2e (0x20 + 14) 和 0x2f (0x20 + 15) 给 ATA 的 Primary bus 和 Secondary bus

```x86asm
//--> kernel/handler.asm
INTERRUPT_ENTRY 0x2e, 0 ; Primary   ATA Bus
INTERRUPT_ENTRY 0x2f, 0 ; Secondary ATA Bus
```

### 6.1 阻塞等待

相对于上一节中的忙等待逻辑，本节需要实现阻塞等待逻辑，即进程在读写时无需不同的轮询状态（这只会空转 CPU），而是阻塞进程直到设备就绪发出中断来解除该进程的阻塞。这需要记录等待的进程，以方便在中断时进行唤醒。

```c
//--> include/xos/ata.h

typedef struct ata_bus_t {
    ...
    task_t *waiter;                 // 等待总线忙碌结束的进程
} ata_bus_t;

//--> kernel/ata.c
static void ata_bus_init() {
    ...
    for (size_t bidx = 0; bidx < ATA_BUS_NR; bidx++) {
        bus->waiter = NULL;
        ...
    }
}
```

在读写时，使用阻塞逻辑替换原有的忙等待逻辑。但是在解除阻塞回来继续执行时最好在进行一次“忙等待”来检查一下状态，看下是否出现错误（这里的“忙等待”并非真的忙等待，因为这时设备已经就绪了，所以只是检查一下状态看是否发生错误）。

```c
//--> kernel/ata.c

i32 ata_pio_read(ata_disk_t *disk, void *buf, u8 count, size_t lba) {
    assert(count > 0);          // 保证读取扇区数不为 0
    ASSERT_IRQ_DISABLE();       // 保证为外中断禁止
    ...
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
        ...
    }
    ...
}
```

注意调用该函数需要保证外中断禁止，这是因为：如果不禁止外中断，那么在进行阻塞进程之前，硬盘设备可能就发送了一个中断，这会导致处理完该中断回来才进行阻塞，而这样阻塞会导致该进程一致阻塞下去。所以为了保证“每一个硬盘中断处理都对应一个进程阻塞”这一原则，需要进行外中断禁止。

写硬盘逻辑也类似，自行阅读代码即可，不再赘述。

### 6.2 处理中断

处理中断逻辑也很简单：读取对应设备的 **常规状态寄存器** 以表示硬盘中断处理结束，然后对阻塞的进程取消阻塞。

```c
//--> kernel/ata.c

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
```

### 6.3 注册中断

在 ATA 总线、硬盘初始化时注册一下对应的中断函数以及取消对应的中断屏蔽字。因为硬盘对应的中断连接在 8259a 的从片上，所以也需要讲从片的屏蔽字 `IRQ_CASCADE` 取消屏蔽。

```c
//--> kernel/ata.c

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
```

## 7. 功能测试

功能测试的逻辑与上一节类似，但因为我们需要使用中断方式来实现异步，所以我们将测试逻辑放入系统调用 `sys_test()` 内：

```c
//--> kernel/syscall.c

extern ata_bus_t buses[ATA_BUS_NR];
// 系统调用 test 的处理函数
static u32 sys_test() {
    void *buf = (void *)kalloc_page(1);
    // BP 0
    LOGK("read buffer 0x%p\n", buf);
    ata_pio_read(&buses[0].disks[0], buf, 1, 0);
    // BP 1
    memset(buf, 0x5a, SECTOR_SIZE);
    // BP 2
    ata_pio_write(&buses[0].disks[0], buf, 1, 1);
    LOGK("write buffer 0x%p\n", buf);
    kfree_page((u32)buf, 1);
}
```

然后在用户进程 `user_init_thread` 中调用该系统调用进行测试：

```c
//--> kernel/thread.c

static void user_init_thread() {
    test();
    ...
}
```

同样地，预期在断点：

- BP1 处：通过 VS Code 调试控制台输入指令 `-exec x/512xb buf`，可以查看到 `buf` 已经存储第 0 个扇区（即 MBR 扇区）的内容（最简单的判断方式为最后 2 个字节为 0xaa55）。
- BP2 处：通过 Hex Editor 插件查看 master.img 镜像（位于 target/ 目录下），可以查看到第 1 个扇区的内容已经全部变为 0x5a（第 1 个扇区起始于 Hex Editor 标注 0x00000200 地址，因为一个扇区为 512 字节）。

除此之外，可以在：

- 中断处理 `ata_hanlder` 里查看硬盘设备的状态
- 读写硬盘时在中断取消阻塞后返回的 `ata_busy_wait` 里的设备状态，此时应该是就绪状态

## 8. 参考文献

- <https://wiki.osdev.org/PCI_IDE_Controller>
- Information Technology - AT Attachment - 8 ATA/ATAPI Command Set (ATA8-ACS)
- <https://wiki.osdev.org/ATA_PIO_Mode>