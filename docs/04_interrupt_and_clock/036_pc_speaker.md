# 036 蜂鸣器

打印字符 `\a` 会发出蜂鸣声 bee。

## 1. 扬声器

![](./images/speaker_01.jpg)

音频是录音设备在特定的时刻记录当时空气的张力值。

![](./images/speaker_02.jpg)

- 位数：音频张力量化位数，采样的精度。
- 采样率：每秒钟的采样数量，通常为 44100 Hz，或者 48000 Hz，人耳能听到的频率为 20 ~ 20000 Hz，而成年人一般只能听到 20 ~ 15000 Hz，所以根据 **奈奎斯特采样定律** 44100 Hz 的音频完全可以满足人耳的需要。采样率表示了，录音设备每秒采集数据的次数，也就是 bit 位数，每秒采集相应次数的数值，用来记录一秒内声音张力的变化；
- 声道：声轨的数量，一般位为单声道或者立体声；
- 码率（比特率）：每秒播放的字节数，可以估计出缓冲区大小，也就是 位数 * 采样率，一些编码方式可能有压缩，所以码率一般不是恒定的值；

## 2. PC Speaker

PC Speaker 是 PC 兼容机中最原始的声音设备，特点是独特的蜂鸣效果，所以有时也被称为蜂鸣器；

扬声器有两种状态，输入和输出，状态可以通过键盘控制器中的端口号 `0x61` 设置，该寄存器结构如下：

| 位  | 描述            |
| --- | --------------- |
| 0   | 计数器 2 门有效 |
| 1   | 扬声器数据有效  |
| 2   | 通道校验有效    |
| 3   | 奇偶校验有效    |
| 4   | 保留            |
| 5   | 保留            |
| 6   | 通道错误        |
| 7   | 奇偶错误        |

需要将 0 ~ 1 位置为 1，然后计数器 2 设置成 方波模式，就可以播放方波的声音。

## 3. 440 Hz

蜂鸣频率选择 440 HZ的原因如下：

`A4 = 440Hz` 第一国际高度；

- 小提琴：GDAE
- 吉他：EADGBE
- 二胡：DA
- 琵琶：ADEA

## 4. qemu 音频驱动

- ALSA
- coreaudio
- dsound
- oss
- PulseAudio
- SDL
- spice
- wav

qemu 音频驱动需要使用以下参数：

```makefile
QFLAGS := -m 32M \
			-boot c \
			-drive file=$(IMG),if=ide,index=0,media=disk,format=raw \
			-audiodev pa,id=hda \           # 音频驱动
			-machine pcspk-audiodev=hda \   # PC Speaker/蜂鸣器
```
其中参数 `-drive file=$(IMG),if=ide,index=0,media=disk,format=raw` 与原先的 `-hda=$<` 的功能相同，只是更加灵活，可以通过 `if` 和 `format` 等参数来指定接口和磁盘镜像格式等。

## 5. 代码分析

### 5.1 蜂鸣器相关端口及频率

```c
/* kernel/clock.c */

// PC Speaker 对应的端口
#define PC_SPEAKER_PORT 0x61
#define BEEP_HZ 440         // 蜂鸣器频率
#define BEEP_COUNTER  (OSCILLATOR / BEEP_HZ)
```

### 5.2 蜂鸣功能

参照 [PC Speaker](#2-pc-speaker) 处的原理说明，设置端口 `0x61` 的低 2 位来产生蜂鸣。

```c
/* kernel/clock.c */

// 时间片计数器
u32 volatile jiffies = 0;
// 蜂鸣器计数器
u32 volatile beeping = 0;

// 开始蜂鸣
void start_beep() {
    if (!beeping) {
        outb(PC_SPEAKER_PORT, inb(PC_SPEAKER_PORT) | 0b11);
        // 蜂鸣持续 5 个时间片
        beeping = jiffies + 5;
    }
}

// 结束蜂鸣
void stop_beep() {
    if (jiffies > beeping) {
        outb(PC_SPEAKER_PORT, inb(PC_SPEAKER_PORT) & ~0b11);
        beeping = 0;
    }
}
```

这里时钟中断也起到定时功能，即蜂鸣需要持续 5 个时间片（大约 50 ms）。每次时钟中断都会进入到时钟中断处理函数，并调用 `stop_beep()` 来检测是否持续 5 个时间片，如果是，则关闭蜂鸣，反之持续蜂鸣。

```c
/* kernel/clock.c */

// 时钟中断处理函数
void clock_handler(int vector) {
    assert(vector == 0x20);
    send_eoi(vector);

    jiffies++;

    // 每个时间片结束前都需要检查当前蜂鸣是否完成（蜂鸣持续 5 个时间片）
    stop_beep();
}
```

### 5.3 设置计数器 2

PTC 初始化时，设置计数器 2 为方波模式，这样与端口 `0x61` 配合才可以发出蜂鸣。

```c
/* kernel/clock.c */

void pit_init() {
    // 配置计数器 0 时钟
    outb(PIT_CTRL_PORT, 0b00110100);
    outb(PIT_CHAN0_PORT, CLOCK_COUNTER & 0xff);
    outb(PIT_CHAN0_PORT, (CLOCK_COUNTER >> 8) & 0xff);

    // 配置计数器 2 蜂鸣器
    outb(PIT_CTRL_PORT, 0b10110110);
    outb(PIT_CHAN2_PORT, BEEP_COUNTER & 0xff);
    outb(PIT_CHAN2_PORT, (BEEP_COUNTER >> 8) & 0xff);
}
```

### 5.4 `\a` 蜂鸣

在 `console_write` 中实现打印 `\a` 时发出蜂鸣的效果。

```c
/* kernel/console.c */

// 向 console 当前光标处以 attr 样式写入一个字节序列
void console_write(char *buf, size_t count, u8 attr) {
	char ch;

	while (count--) {
        ch = *buf++;
		switch (ch) {
			...
			case ASCII_BEL: // 蜂鸣 beep
                start_beep();
                break;
			...
		}
	...
}
```

## 6. 功能测试

>**这个功能只能在 qemu 下测试**

在时钟中断处理函数里定时调用蜂鸣：

```c
/* kernel/clock.c */

// 时钟中断处理函数
void clock_handler(int vector) {
    assert(vector == 0x20);
    send_eoi(vector);

    // 定时蜂鸣，用于测试
    if (jiffies % 200 == 0) start_beep();

    jiffies++;
    DEBUGK("clock jiffies %d ...\n", jiffies);

    // 每个时间片结束前都需要检查当前蜂鸣是否完成（蜂鸣持续 5 个时间片）
    stop_beep();
}
```

```c
/* kernel/main.c */

void kernel_init() {
    console_init();
    gdt_init();
    interrupt_init();
    clock_init();

	return;
}
```

预期为，每 200 个时间片（大约 2s）发起一次蜂鸣。

---

测试打印字符 `\a` 是否发出蜂鸣：

```c
void kernel_init() {
    console_init();
    gdt_init();
    interrupt_init();
    clock_init();

    printk("\a");
    hang();

    return;
}
```

预期为，打印时发出一声蜂鸣。

## 7. 参考文献

- <http://blog.ccyg.studio/article/593eaa9a-7457-4561-ad97-7fabacb6c05d/>
- <https://wiki.osdev.org/%228042%22_PS/2_Controller>
- <https://www.cs.usfca.edu/~cruse/cs630f08/lesson15.ppt>
- <https://wiki.osdev.org/PC_Speaker>
- <https://www.kraxel.org/blog/2020/01/qemu-sound-audiodev/>
- <https://www.qemu.org/docs/master/system/qemu-manpage.html>
- <https://www.freedesktop.org/wiki/Software/PulseAudio/>