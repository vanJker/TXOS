# 055 multiboot2 头

## 1. multiboot2 header

要支持 multiboot2，内核必须添加一个 multiboot header，而且这个 header 必须在内核开始的前 32768(0x8000) 字节内，而且必须以 64 字节对齐。在实际中一般将 multiboot2 header 放在最开始的位置，即内核的 text 段之前。

> An OS image must contain an additional header called Multiboot2 header, besides the headers of the format used by the OS image. The Multiboot2 header must be contained completely within the first 32768 bytes of the OS image, and must be 64-bit aligned. In general, it should come as early as possible, and may be embedded in the beginning of the text segment after the real executable header.

以下是 multiboot2 header 的组成部分：

| 偏移  | 类型 | 名称                | 备注 |
| ----- | ---- | ------------------- | ---- |
| 0     | u32  | 魔数 (magic)        | 必须 |
| 4     | u32  | 架构 (architecture) | 必须 |
| 8     | u32  | 头部长度 (header_length)   | 必须 |
| 12    | u32  | 校验和 (checksum)   | 必须 |
| 16-XX |      | 标记 (tags)         | 必须 |

- `magic` = 0xE85250D6
- `architecture`:
    - 0 表示 i386 的 32 位保护模式
- `checksum`：与 `magic`, `architecture`, `header_length` 相加必须为 `0`

其中 multiboot2 header 中的 tags 部分的组成如下：

| 偏移 | 类型 | 名称 |
| --- | ---- | --- |
| 16  | u16  | type |
| 18  | u16  | flags |
| 20  | u32  | size |

tags 部分以 type ‘0’ 以及 size '8' 来结束，目前处于简化目的，我们将 flags 也设置为 '0'。

> If bit ‘0’ of ‘flags’ (also known as ‘optional’) is set, the bootloader may ignore this tag if it lacks relevant support. Tags are terminated by a tag of type ‘0’ and size ‘8’

## 2. 代码分析

### 2.1 multiboot header

我们在 `kernel/start.asm` 里定义一个 multiboot2 header：

```x86asm
header_magic    equ 0xe85250d6
header_arch     equ 0
header_length   equ (multiboot2_header_end - multiboot2_header_start)
header_checksum equ -(header_magic + header_arch + header_length)

section .multiboot2
multiboot2_header_start:
    dd header_magic     ; multiboot2 魔数
    dd header_arch      ; multiboot2 引导架构，0 表示 32 位保护模式的 i386
    dd header_length    ; multiboot2 header 的长度
    dd header_checksum  ; multiboot2 校验和

    ; multiboot2 header 结束标记
    dw 0    ; type  (16 bit)
    dw 0    ; flags (16 bit)
    dd 8    ; size  (32 bit)
multiboot2_header_end:
```

接着我们修改 `Makefile`，使得 multiboot2 section 位于内核的 text section 之前。

```makefile
MULTIBOOT2 := 0x10000
KERNEL_ENTRY := 0x10040

# ld 参数
LDFLAGS := -m elf_i386 \
			-static \
			-Ttext $(KERNEL_ENTRY) \
			--section-start=.multiboot2=$(MULTIBOOT2) \
```

因为 multiboot2 header 需要以 64 字节对齐，所以 `KERNEL_ENTRY = MULTIBOOT2 + 64 = 0x10040`。

- `-Ttext $(KERNEL_ENTRY)` 用于指定 kernel 的 text section 的起始地址为 `$(KERNEL_ENTRY)`。
- `--section-start=.multiboot2=$(MULTIBOOT)` 用于指定 multiboot2 section 的起始地址为 `$(MULTIBOOT2)`。

## 2.2 loader

由于修改了内核入口地址 `KERNEL_ENTRY`，所以在内核加载器 loader 需要对内核入口地址的约定进行相对应的修改，以确保 BIOS 启动方式能正确进入内核（但是 multiboot2 不需要通过 BIOS 和 loader 进入内核，这部分流程在下一节会进行讲解）。

```x86asm
[bits 32]
protected_mode:
    ...
    jmp 0x10040 ; 进入内核
```

## 2.3 grub

接下来我们需要配置 grub 多系统引导，我们需要使用 **elf 格式的内核目标文件** 与 grub 配置文件来生成支持 grub 引导的 iso 文件。

典型的生成文件夹如下：

```bash
iso/
 |__ boot/
      |__ kernel.elf
      |__ grub/
           |__ grub.cfg
```

我们先编写一个 grub 配置文件 `util/grub.cfg`：

```ini
set timeout=5 # grub 多系统引导界面停留 5 秒
set default=0 # 默认选择第 0 个的系统

# 引导界面包括的系统
menuentry "XOS" {
    multiboot2 /boot/kernel.elf
}
```

> 注：`grub.cfg` 文件认为其所在的目录为 `iso`，所以对应的内核镜像为 `boot/kernel.elf`，如果还有其它系统，可以为 `boot/kernel2.elf` 等等。

## 2.4 生成 iso 镜像

grub 引导的系统文件必须为 iso 格式的，接下来我们就配置下 `Makefile` 来生成内核的 iso 镜像。

```makefile
# iso 格式的内核镜像
ISO := $(TARGET)/systerm.iso

# grub 启动的配置文件
GRUB_CFG := $(SRC)/utils/grub.cfg


$(ISO): $(KERNEL_ELF) $(GRUB_CFG)
# 检测内核目标文件是否合法
	grub-file --is-x86-multiboot2 $<
# 创建 iso 目录
	mkdir -p $(TARGET)/iso/boot/grub
# 拷贝内核目标文件
	cp $< $(TARGET)/iso/boot
# 拷贝 grub 配置文件
	cp $(GRUB_CFG) $(TARGET)/iso/boot/grub
# 生成 iso 格式的内核镜像
	grub-mkrescue -o $@ $(TARGET)/iso
```

主要逻辑为：
- 先通过 `grub-file --is-x86-multibooo2` 来检测内核的 elf 格式目标文件是否包含 multiboot2 header。
- 再按照之前所述的 grub 配置部分，构建一个 `iso` 目录，用于生成内核的 iso 镜像。并且将 elf 格式的内核目标文件 `kernel.elf` 以及 grub 配置文件 `grub.cfg` 复制到正确位置。
- 最后通过 `grub-mkrescue` 生成可以进行 grub 引导的内核 iso 镜像。

## 2.5 bochs & qemu 的 grub 引导启动

给 bochs 新增一个 grub 引导启动的配置文件 `bochs/bochsrc.grub`：

```ini
...
boot: cdrom
...
ata0-master: type=cdrom, path="target/systerm.iso", status=inserted
...
```

同 `bochs/bochsrc` 相比，只是将启动方式从 disk 改成了 cdrom 启动，并且使用的是内核的 iso 镜像文件（从 CDROM 启动是因为 ISO 镜像以及 GRUB 只能从 CDROM 进行启动）。

修改 `Makefile` 增加 bochs 和 qemu 的 grub 启动： 

```makefile
# bochs grub 启动
.PHONY: bochs-grub
bochs-grub: $(ISO)
	bochs -q -f ./bochs/bochsrc.grub -unlock


QEMU_CDROM := -boot d \
				-drive file=$(ISO),media=cdrom \

# qemu grub 启动
.PHONY: qemu-grub
qemu-grub: $(ISO)
	$(QEMU) $(QFLAGS) $(QEMU_CDROM)
```

## 3. 功能测试

分别测试 BIOS 启动和 GRUB 启动：

- BIOS 启动：预期为，bochs 和 qemu 均成功构建系统，并正常运行系统。
- GRUB 启动：预期为，bochs 和 qemu 均成功构建系统，运行系统时会出现 GRUB 引导界面，之后与 `memory_init()` 处触发 `panic`，表示无法识别魔数，这个是我们下一节要完成的内容。

## 4. FAQ

> 如果构建 iso 镜像时出现错误：`grub-mkrescue: error: xorriso not found`，输入以下命令按照 `xorriso` 即可解决。
> ***
> `sudo pacman -S xorriso`

## 5. 参考文献

- <https://www.gnu.org/software/grub/manual/grub/grub.html>
- <https://www.gnu.org/software/grub/manual/multiboot2/multiboot.pdf>
- <https://intermezzos.github.io/book/first-edition/multiboot-headers.html>
- <https://os.phil-opp.com/multiboot-kernel/>
- <https://bochs.sourceforge.io/doc/docbook/user/bios-tips.html>
- <https://forum.osdev.org/viewtopic.php?f=1&t=18171>
- <https://wiki.gentoo.org/wiki/QEMU/Options>
- <https://hugh712.gitbooks.io/grub/content/>
