# Makefile 文件说明

下面是 XOS 的 makefile 文件的注释说明版：

> 注：下面的代码只是为例解释说明 makefile 的作用，并不是一个有效的 makefile 文件，因为这里使用的注释大部分位于有效命令之后，在实际中可能会出现奇奇怪怪的错误。而且有些地方为了方便注释，进行了改写。

```makefile
TARGET := ./target # 目标文件路径
SRC	:= ./src       # 源文件路径

IMG := $(TARGET)/master.img # img 格式的目标磁盘映像文件

BOOT_BIN := $(TARGET)/bootloader/boot.bin       # boot 目标文件
LOADER_BIN := $(TARGET)/bootloader/loader.bin   # loader 目标文件
 
KERNEL_LINKER := $(SRC)/kernel/linker.ld    # kernel 的链接脚本（已弃用）
KERNEL_ENTRY := 0x10000                     # kernel 的 text section 起始地址
kernel.elf := $(TARGET)/kernel.elf          # kernel 的 elf 格式的目标文件
KERNEL_BIN := $(TARGET)/kernel.bin          # kernel 的二进制格式的目标文件
KERNEL_SYM := $(TARGET)/kernel.map          # kernel 的符号表

# ld 的目标文件参数次序必须满足依赖次序，且保证第一条指令在 $(KERNEL_ENTRY) 地址处
# 所以必须要将 start.o 置于 ld 命令的链接第一个参数位置
KERNEL_OBJS := $(TARGET)/kernel/start.o \
			   $(TARGET)/kernel/main.o \
			   $(TARGET)/kernel/io.o \
			   $(TARGET)/kernel/console.o \
			   $(TARGET)/kernel/printk.o \
			   $(TARGET)/kernel/assert.o \
			   $(TARGET)/kernel/debug.o \
			   $(TARGET)/kernel/global.o \
			   $(TARGET)/kernel/task.o \
			   $(TARGET)/kernel/schedule.o \
			   $(TARGET)/kernel/interrupt.o \
			   $(TARGET)/kernel/handler.o \
			   $(TARGET)/kernel/clock.o \
			   $(TARGET)/kernel/time.o \
			   $(TARGET)/kernel/rtc.o \
			   $(TARGET)/kernel/memory.o \
			   $(TARGET)/kernel/syscall.o \
			   $(TARGET)/kernel/thread.o \
			   $(TARGET)/kernel/mutex.o \

# lib 的目标文件
LIB_OBJS := $(patsubst $(SRC)/lib/%.c, $(TARGET)/lib/%.o, $(wildcard $(SRC)/lib/*.c))


# gcc 参数
CFLAGS := -m32          # 生成 32 位机器上的程序
CFLAGS += -nostdinc     # 不引入标准头文件
CFLAGS += -nostdlib     # 不引入标准库文件
CFLAGS += -fno-builtin  # 无需 gcc 内置函数
CFLAGS += -fno-pic      # 无需位置无关代码 (pic: position independent code)
CFLAGS += -fno-pie      # 无需位置无关的可执行程序 (pie: position independent executable)
CFLAGS += -fno-stack-protector  # 无需栈保护
CFLAGS := $(strip ${CFLAGS})    # 去除 CFLAGS 中多余的空白符

# debug 参数
DEBUG_FLAGS := -g
# 头文件查找路径参数
INCLUDE_FLAGS := -I $(SRC)/include

# ld 参数
LDFLAGS := -m elf_i386  # 目标机器为 i386
LDFLAGS += -static      # 静态链接
LDFLAGS += -Ttext $(KERNEL_ENTRY)   # text section 的起始地址为 $(KERNEL_ENTRY)
LDFLAGS := $(strip ${LDFLAGS})      # 去除 LDFLAGS 中多余的空白符


# bootloader 目标文件生成规则（源文件为 nasm 风格的 x86 汇编）
$(TARGET)/bootloader/%.bin: $(SRC)/bootloader/%.asm
	$(shell mkdir -p $(dir $@))
	nasm -f bin $< -o $@

# kernel 目标文件生成规则（源文件为 nasm 风格的 x86 汇编）
$(TARGET)/kernel/%.o: $(SRC)/kernel/%.asm
	$(shell mkdir -p $(dir $@))
	nasm -f elf32 $(DEBUG_FLAGS) $< -o $@

# kernel 目标文件生成规则（源文件为 C 语言）
$(TARGET)/kernel/%.o: $(SRC)/kernel/%.c
	$(shell mkdir -p $(dir $@))
	gcc $(CFLAGS) $(DEBUG_FLAGS) $(INCLUDE_FLAGS) -c $< -o $@

# lib 目标文件生成规则（源文件为 C 语言）
$(TARGET)/lib/%.o: $(SRC)/lib/%.c
	$(shell mkdir -p $(dir $@))
	gcc $(CFLAGS) $(DEBUG_FLAGS) $(INCLUDE_FLAGS) -c $< -o $@

# kernel 的 elf 格式的目标文件的生成规则
$(kernel.elf): $(KERNEL_OBJS) $(LIB_OBJS)
	$(shell mkdir -p $(dir $@))
	ld $(LDFLAGS) $^ -o $@

# kernel 的二进制格式的目标文件的生成规则
$(KERNEL_BIN): $(KERNEL_ELF)
	objcopy -O binary $< $@

# kernel 的符号表的生成规则
$(KERNEL_SYM): $(KERNEL_ELF)
	nm $< | sort > $@


# 目标磁盘映像的 ing 格式的目标文件生成规则
$(IMG): $(BOOT_BIN) $(LOADER_BIN) $(KERNEL_BIN) $(KERNEL_SYM)
# 创建一个 16M 的硬盘镜像
	yes | bximage -q -hd=16 -func=create -sectsize=512 -imgmode=flat $@
# 将 boot.bin 写入主引导扇区
	dd if=$(BOOT_BIN) of=$@ bs=512 count=1 conv=notrunc
# 将 loader.bin 写入硬盘
	dd if=$(LOADER_BIN) of=$@ bs=512 count=4 seek=2 conv=notrunc
# 测试 system.bin 小于 100k，否则需要修改下面的 count
	test -n "$$(find $(TARGET)/kernel.bin -size -100k)"
# 将 kernel.bin 写入硬盘
	dd if=$(KERNEL_BIN) of=$@ bs=512 count=200 seek=10 conv=notrunc


# 构建系统的规则
.PHONY: build
build: $(IMG)

# bochs 运行系统的规则
.PHONY: bochs-run
bochs-run: $(IMG)
	bochs -q -f ./bochs/bochsrc -unlock

# bochs 调试系统的规则
.PHONY: bochs-debug
bochs-debug: $(IMG)
	bochs-gdb -q -f ./bochs/bochsrc.gdb -unlock

# qemu 系统模拟的架构为 i386
QEMU := qemu-system-i386
# qemu 参数
QFLAGS := -m 32M    # 指定 32 MB 的虚拟内存
QFLAGS += -boot c   # 从硬盘启动
QFLAGS += -drive file=$(IMG),if=ide,index=0,media=disk,format=raw   # 硬盘启动文件文件为 $(IMG)，启动方式为 ide
QFLAGS += -audiodev pa,id=hda   # 音频驱动
QFLAGS += -machine pcspk-audiodev=hda   # PC Speaker/蜂鸣器
QFLAGS += -rtc base=localtime   # 将 qemu 的时区设置成当前所在地时区
QFLAGS := $(strip ${QFLAGS})    # 去除 QFLAGS 中多余的空白符

# qemu 运行系统的规则
.PHONY: qemu-run
qemu-run: $(IMG)
	$(QEMU) $(QFLAGS)

# qemu 调试系统的规则
# -s 表示监听本地主机的 1234 端口，等待 gdb 连接
# -S 表示当 gdb 尚未连接到 qemu 时，不执行
.PHONY: qemu-debug
qemu-debug: $(IMG)
	$(QEMU) $(QFLAGS) -s -S

# vmdk 格式的目标磁盘映像文件
VMDK := $(TARGET)/master.vmdk
$(VMDK): $(IMG)
	qemu-img convert -pO vmdk $< $@

# 生成 vmdk 格式的磁盘映像的规则
.PHONY: vmdk
vmdk: $(VMDK)

# 清除系统的规则（@可以使该行命令在执行时不输出）
.PHONY: clean
clean:
	@rm -rf $(TARGET)

# make 命令的默认规则
.PHONY: all
all: qemu-run
```