
TARGET := ./target
SRC	:= ./src

# img 格式的目标磁盘映像
IMG := $(TARGET)/master.img

# iso 格式的内核镜像
ISO := $(TARGET)/systerm.iso

# grub 启动的配置文件
GRUB_CFG := $(SRC)/utils/grub.cfg

BOOT_BIN := $(TARGET)/bootloader/boot.bin
LOADER_BIN := $(TARGET)/bootloader/loader.bin

MULTIBOOT2 := 0x10000
KERNEL_ENTRY := 0x10040
KERNEL_ELF := $(TARGET)/kernel.elf
KERNEL_BIN := $(TARGET)/kernel.bin
KERNEL_SYM := $(TARGET)/kernel.map

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
			   $(TARGET)/kernel/keyboard.o \
			   $(TARGET)/kernel/arena.o \

# lib 的目标文件
LIB_OBJS := $(patsubst $(SRC)/lib/%.c, $(TARGET)/lib/%.o, $(wildcard $(SRC)/lib/*.c))

# gcc 参数
CFLAGS := -m32			# 生成 32 位机器上的程序
CFLAGS += -nostdinc		# 不引入标准头文件
CFLAGS += -nostdlib		# 不引入标准库文件
CFLAGS += -fno-builtin	# 无需 gcc 内置函数
CFLAGS += -fno-pic		# 无需位置无关代码 (pic: position independent code)
CFLAGS += -fno-pie		# 无需位置无关的可执行程序 (pie: position independent executable)
CFLAGS += -fno-stack-protector	# 无需栈保护
CFLAGS := $(strip ${CFLAGS}) 	# 去除 CFLAGS 中多余的空白符

# debug 参数
DEBUG_FLAGS := -g
# 头文件查找路径参数
INCLUDE_FLAGS := -I $(SRC)/include

# ld 参数
LDFLAGS := -m elf_i386 \
			-static \
			-Ttext $(KERNEL_ENTRY) \
			--section-start=.multiboot2=$(MULTIBOOT2) \


$(TARGET)/bootloader/%.bin: $(SRC)/bootloader/%.asm
	$(shell mkdir -p $(dir $@))
	nasm -f bin $< -o $@

$(TARGET)/kernel/%.o: $(SRC)/kernel/%.asm
	$(shell mkdir -p $(dir $@))
	nasm -f elf32 $(DEBUG_FLAGS) $< -o $@

$(TARGET)/kernel/%.o: $(SRC)/kernel/%.c
	$(shell mkdir -p $(dir $@))
	gcc $(CFLAGS) $(DEBUG_FLAGS) $(INCLUDE_FLAGS) -c $< -o $@

$(TARGET)/lib/%.o: $(SRC)/lib/%.c
	$(shell mkdir -p $(dir $@))
	gcc $(CFLAGS) $(DEBUG_FLAGS) $(INCLUDE_FLAGS) -c $< -o $@

$(KERNEL_ELF): $(KERNEL_OBJS) $(LIB_OBJS)
	$(shell mkdir -p $(dir $@))
	ld $(LDFLAGS) $^ -o $@

$(KERNEL_BIN): $(KERNEL_ELF)
	objcopy -O binary $< $@

$(KERNEL_SYM): $(KERNEL_ELF)
	nm $< | sort > $@


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

.PHONY: img
img: $(IMG)


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

.PHONY: iso
iso: $(ISO)


.PHONY: bochs-run
bochs-run: $(IMG)
	bochs -q -f ./bochs/bochsrc -unlock

.PHONY: bochs-debug
bochs-debug: $(IMG)
	bochs-gdb -q -f ./bochs/bochsrc.gdb -unlock

.PHONY: bochs-grub
bochs-grub: $(ISO)
	bochs -q -f ./bochs/bochsrc.grub -unlock


QEMU := qemu-system-i386
# qemu 参数
QFLAGS := -m 32M \
			-audiodev pa,id=hda \
			-machine pcspk-audiodev=hda \
			-rtc base=localtime \

QEMU_DISK := -boot c \
				-drive file=$(IMG),if=ide,index=0,media=disk,format=raw \

QEMU_CDROM := -boot d \
				-drive file=$(ISO),media=cdrom \

.PHONY: qemu-run
qemu-run: $(IMG)
	$(QEMU) $(QFLAGS) $(QEMU_DISK)

.PHONY: qemu-debug
qemu-debug: $(IMG)
	$(QEMU) $(QFLAGS) $(QEMU_DISK) -s -S

.PHONY: qemu-grub
qemu-grub: $(ISO)
	$(QEMU) $(QFLAGS) $(QEMU_CDROM)


VMDK := $(TARGET)/master.vmdk
$(VMDK): $(IMG)
	qemu-img convert -pO vmdk $< $@

.PHONY: vmdk
vmdk: $(VMDK)


.PHONY: all
all: qemu-run

.PHONY: build
build: $(IMG) $(ISO)

.PHONY: clean
clean:
	@rm -rf $(TARGET)
