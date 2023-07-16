# 012 bootloader 补充说明

本项目的 bootloader 由 `boot.asm`，`loader.asm` 这两大部分组成。bootloader 的主要功能是，从 BIOS 中加载内核，并提供内核所需要的参数。

## 1. 0xAA55

魔数，位于主引导扇区的最后两个字节，用于检测加载主引导扇区是否有误。因为 x86 为小端序，所以第 510 字节为 0x55，第 511 字节为 0xAA。

## 2. 0x7C00

0x7C00 作为 bootloader 在内存第一条指令所在地址，是由于其历史兼容导致的。早期的计算机 IBM PC 5150 和 DOS 1.0，如果要访问地址 0x7C00，至少需要有 2^15 = 32KB 大小的内存。又因为栈是向下增长的，所以一般会放在内存的最后。当时规定栈大小同主引导扇区一样为 512 Bytes,则主引导扇区的第一条指令所在地址显然为：32KB - 512B - 512B = 31K = 0x7c00。

## 3. GRUB

- multiboot：多系统启动，即一台机器支持多个系统启动，本项目后续也会加入该特性的支持。

## 4. 参考文献

- [GRUB - OSDev Wiki](https://wiki.osdev.org/GRUB)
- [The Multiboot Specification](http://nongnu.askapache.com/grub/phcoder/multiboot.pdf)