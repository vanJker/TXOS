
TARGET := ./target
SRC	:= ./src

BOOT_BIN := $(TARGET)/bootloader/boot.bin
LOADER_BIN := $(TARGET)/bootloader/loader.bin
IMG := $(TARGET)/master.img

KERNEL_LINKER := $(SRC)/kernel/linker.ld
KERNEL_ELF := $(TARGET)/kernel.elf
KERNEL_BIN := $(TARGET)/kernel.bin
KERNEL_MAP := $(TARGET)/kernel.map

KERNEL_SRCS := $(wildcard $(SRC)/kernel/*.asm)
KERNEL_OBJS := $(patsubst $(SRC)/kernel/%.asm, $(TARGET)/kernel/%.o, $(KERNEL_SRCS))
KERNEL_SRCS := $(wildcard $(SRC)/kernel/*.c)
KERNEL_OBJS += $(patsubst $(SRC)/kernel/%.c, $(TARGET)/kernel/%.o, $(KERNEL_SRCS))

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
all: bochs

$(TARGET)/bootloader/%.bin: $(SRC)/bootloader/%.asm
	$(shell mkdir -p $(dir $@))
	nasm -f bin $< -o $@

$(TARGET)/kernel/%.o: $(SRC)/kernel/%.asm
	$(shell mkdir -p $(dir $@))
	nasm -f elf32 $(DEBUG_FLAGS) $< -o $@

$(TARGET)/kernel/%.o: $(SRC)/kernel/%.c
	$(shell mkdir -p $(dir $@))
	gcc $(CFLAGS) $(DEBUG_FLAGS) $(INCLUDE_FLAGS) -c $< -o $@

$(KERNEL_ELF): $(KERNEL_OBJS)
	$(shell mkdir -p $(dir $@))
	ld -m elf_i386 -static $^ -o $@ -T $(KERNEL_LINKER)

$(KERNEL_BIN): $(KERNEL_ELF)
	objcopy -O binary $< $@

$(KERNEL_MAP): $(KERNEL_ELF)
	nm $< | sort > $@


$(IMG): $(BOOT_BIN) $(LOADER_BIN) $(KERNEL_BIN)
	yes | bximage -q -hd=16 -func=create -sectsize=512 -imgmode=flat $@
	dd if=$(BOOT_BIN) of=$@ bs=512 count=1 conv=notrunc
	dd if=$(LOADER_BIN) of=$@ bs=512 count=4 seek=2 conv=notrunc
	dd if=$(KERNEL_BIN) of=$@ bs=512 count=200 seek=10 conv=notrunc


.PHONY: clean
clean:
	rm -rf $(TARGET)/*

.PHONY: bochs 
bochs: $(IMG)
	bochs -q