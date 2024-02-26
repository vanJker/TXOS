TARGET := ./target
SRC	:= ./src

# img 格式的目标磁盘映像
IMG := $(TARGET)/master.img $(TARGET)/slave.img

BOOT_BIN := $(TARGET)/bootloader/boot.bin
LOADER_BIN := $(TARGET)/bootloader/loader.bin

MULTIBOOT2 := 0x10000
KERNEL_ENTRY := 0x10040
SYSTEM_ELF := $(TARGET)/system.elf
SYSTEM_BIN := $(TARGET)/system.bin
SYSTEM_SYM := $(TARGET)/system.map

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
			   $(TARGET)/kernel/ata.o \
			   $(TARGET)/kernel/device.o \
			   $(TARGET)/kernel/buffer.o \

# fs 的目标文件
FS_OBJS := $(patsubst $(SRC)/fs/%.c, $(TARGET)/fs/%.o, $(wildcard $(SRC)/fs/*.c))

# lib 的目标文件
LIB_OBJS := $(patsubst $(SRC)/lib/%.c, $(TARGET)/lib/%.o, $(wildcard $(SRC)/lib/*.c))

# 头文件
HEADERS := $(wildcard $(SRC)/include/xos/*.h)

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

$(TARGET)/fs/%.o: $(SRC)/fs/%.c
	$(shell mkdir -p $(dir $@))
	gcc $(CFLAGS) $(DEBUG_FLAGS) $(INCLUDE_FLAGS) -c $< -o $@

$(TARGET)/lib/%.o: $(SRC)/lib/%.c
	$(shell mkdir -p $(dir $@))
	gcc $(CFLAGS) $(DEBUG_FLAGS) $(INCLUDE_FLAGS) -c $< -o $@

$(SYSTEM_ELF): $(KERNEL_OBJS) $(FS_OBJS) $(LIB_OBJS)
	$(shell mkdir -p $(dir $@))
	ld $(LDFLAGS) $^ -o $@

$(SYSTEM_BIN): $(SYSTEM_ELF)
	objcopy -O binary $< $@

$(SYSTEM_SYM): $(SYSTEM_ELF)
	nm $< | sort > $@


$(TARGET)/master.img: $(BOOT_BIN) $(LOADER_BIN) $(SYSTEM_BIN) $(SYSTEM_SYM) $(SRC)/utils/master.sfdisk
# 创建一个 16M 的硬盘镜像
	yes | bximage -q -hd=16 -func=create -sectsize=512 -imgmode=flat $@
# 将 boot.bin 写入主引导扇区
	dd if=$(BOOT_BIN) of=$@ bs=512 count=1 conv=notrunc
# 将 loader.bin 写入硬盘
	dd if=$(LOADER_BIN) of=$@ bs=512 count=4 seek=2 conv=notrunc
# 测试 system.bin 小于 100k，否则需要修改下面的 count
	test -n "$$(find $(SYSTEM_BIN) -size -100k)"
# 将 system.bin 写入硬盘
	dd if=$(SYSTEM_BIN) of=$@ bs=512 count=200 seek=10 conv=notrunc
# 对硬盘进行分区
	sfdisk $@ < $(SRC)/utils/master.sfdisk
# 挂载设备到主机
	sudo losetup /dev/loop0 --partscan $@
# 在设备第一个分区内创建 minux 文件系统
	sudo mkfs.minix -1 -n 14 /dev/loop0p1
# 挂载设备的文件系统到主机的 /mnt 目录
	sudo mount /dev/loop0p1 /mnt
# 切换文件系统的所有者
	sudo chown ${USER} /mnt 
# 创建目录
	mkdir -p /mnt/home
	mkdir -p /mnt/d1/d2/d3/d4
# 创建文件
	echo "Hello XOS!!!, from root direcotry file..." > /mnt/hello.txt
	echo "Hello XOS!!!, from home direcotry file..." > /mnt/home/hello.txt
# 从主机上卸载文件系统
	sudo umount /mnt
# 从主机上卸载设备
	sudo losetup -d /dev/loop0

$(TARGET)/slave.img:
# 创建一个 32M 的硬盘镜像
	yes | bximage -q -hd=32 -func=create -sectsize=512 -imgmode=flat $@
# 对硬盘进行分区
	sfdisk $@ < $(SRC)/utils/slave.sfdisk
# 挂载设备到主机
	sudo losetup /dev/loop1 --partscan $@
# 在设备第一个分区内创建 minux 文件系统
	sudo mkfs.minix -1 -n 14 /dev/loop1p1
# 挂载设备的文件系统到主机的 /mnt 目录
	sudo mount /dev/loop1p1 /mnt
# 切换文件系统的所有者
	sudo chown ${USER} /mnt 
# 创建文件
	echo "slave root direcotry file..." > /mnt/hello.txt
# 从主机上卸载文件系统
	sudo umount /mnt
# 从主机上卸载设备
	sudo losetup -d /dev/loop1

# 挂载 master.img 到主机设备 loop0 和目录 /mnt
.PHONY: mount0
mount0: $(TARGET)/master.img
	sudo losetup /dev/loop0 --partscan $<
	sudo mount /dev/loop0p1 /mnt
	sudo chown ${USER} /mnt 

# 卸载 master.img 相关的设备和文件系统
.PHONY: umount0
umount0: /dev/loop0
	-sudo umount /mnt
	-sudo losetup -d $<

# 挂载 slave.img 到主机设备 loop1 和目录 /mnt
.PHONY: mount1
mount1: $(TARGET)/slave.img
	sudo losetup /dev/loop1 --partscan $<
	sudo mount /dev/loop1p1 /mnt
	sudo chown ${USER} /mnt 

# 卸载 slave.img 相关的设备和文件系统
.PHONY: umount1
umount1: /dev/loop1
	-sudo umount /mnt
	-sudo losetup -d $<


.PHONY: img
img: $(IMG)


.PHONY: bochs-run
bochs-run: $(IMG)
	bochs -q -f ./bochs/bochsrc -unlock

.PHONY: bochs-debug
bochs-debug: $(IMG)
	bochs-gdb -q -f ./bochs/bochsrc.gdb -unlock


QEMU := qemu-system-i386
# qemu 参数
QFLAGS := -m 32M \
			-audiodev pa,id=hda \
			-machine pcspk-audiodev=hda \
			-rtc base=localtime \

QEMU_DISK := -boot c
QEMU_DISK += -drive file=$(TARGET)/master.img,if=ide,index=0,media=disk,format=raw
QEMU_DISK += -drive file=$(TARGET)/slave.img,if=ide,index=1,media=disk,format=raw

.PHONY: qemu-run
qemu-run: $(IMG)
	$(QEMU) $(QFLAGS) $(QEMU_DISK)

.PHONY: qemu-debug
qemu-debug: $(IMG)
	$(QEMU) $(QFLAGS) $(QEMU_DISK) -s -S


VMDK := $(TARGET)/master.vmdk
$(VMDK): $(IMG)
	qemu-img convert -pO vmdk $< $@

.PHONY: vmdk
vmdk: $(VMDK)


.PHONY: all
all: qemu-run

include $(SRC)/utils/grub.mk

.PHONY: build
build: $(IMG) $(ISO)

.PHONY: clean
clean:
	@rm -rf $(TARGET)
