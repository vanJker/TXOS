
TARGET := ./target
SRC	:= ./src

BOOT_BIN := $(TARGET)/bootloader/boot.bin
LOADER_BIN := $(TARGET)/bootloader/loader.bin
IMG := $(TARGET)/master.img

KERNEL_LINKER := $(SRC)/kernel/linker.ld
KERNEL_ENTRY := 0x10000
KERNEL_ELF := $(TARGET)/kernel.elf
KERNEL_BIN := $(TARGET)/kernel.bin
KERNEL_SYM := $(TARGET)/kernel.map

# ld 的目标文件参数次序必须满足依赖次序，且保证第一条指令在 0x10000 地址处
# 所以必须要将 start.o 置于第一个参数位置
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
			   $(TARGET)/kernel/cmos.o \

LIB_OBJS := $(patsubst $(SRC)/lib/%.c, $(TARGET)/lib/%.o, $(wildcard $(SRC)/lib/*.c))

# gcc 参数
CFLAGS := -m32			# 生成 32 位机器上的程序
CFLAGS += -nostdinc		# 不引入标准头文件
CFLAGS += -nostdlib		# 不引入标准库文件
CFLAGS += -fno-builtin	# 无需 gcc 内置函数
CFLAGS += -fno-pic		# 无需位置无关代码 (pic: position independent code)
CFLAGS += -fno-pie		# 无需位置无关的可执行程序 (pie: position independent executable)
CFLAGS += -fno-stack-protector	# 无需栈保护
CFLAGS:=$(strip ${CFLAGS}) # 去除 CFLAGS 中的换行符

DEBUG_FLAGS := -g # debug 参数
INCLUDE_FLAGS := -I $(SRC)/include # 头文件查找路径参数


.PHONY: all
all: qemu-run

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
	ld -m elf_i386 -static $^ -o $@ -Ttext $(KERNEL_ENTRY)

$(KERNEL_BIN): $(KERNEL_ELF)
	objcopy -O binary $< $@

$(KERNEL_SYM): $(KERNEL_ELF)
	nm $< | sort > $@


$(IMG): $(BOOT_BIN) $(LOADER_BIN) $(KERNEL_BIN) $(KERNEL_SYM)
	yes | bximage -q -hd=16 -func=create -sectsize=512 -imgmode=flat $@
	dd if=$(BOOT_BIN) of=$@ bs=512 count=1 conv=notrunc
	dd if=$(LOADER_BIN) of=$@ bs=512 count=4 seek=2 conv=notrunc
	dd if=$(KERNEL_BIN) of=$@ bs=512 count=200 seek=10 conv=notrunc


.PHONY: clean
clean:
	rm -rf $(TARGET)/*

.PHONY: build
build: $(IMG)

.PHONY: bochs-run
bochs-run: $(IMG)
	bochs -q -f ./bochs/bochsrc

.PHONY: bochs-debug
bochs-debug: $(IMG)
	bochs-gdb -q -f ./bochs/bochsrc-gdb

QEMU := qemu-system-i386
QFLAGS := -m 32M \
			-boot c \
			-drive file=$(IMG),if=ide,index=0,media=disk,format=raw \
			-audiodev pa,id=hda \
			-machine pcspk-audiodev=hda \
			-rtc base=localtime \

.PHONY: qemu-run
qemu-run: $(IMG)
	$(QEMU) $(QFLAGS)

.PHONY: qemu-debug
qemu-debug: $(IMG)
	$(QEMU) $(QFLAGS) -s -S

VMDK := $(TARGET)/master.vmdk
$(VMDK): $(IMG)
	qemu-img convert -pO vmdk $< $@

.PHONY: vmdk
vmdk: $(VMDK)