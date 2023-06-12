
TARGET := ./target
SRC	:= ./src

BOOT_BIN := $(TARGET)/bootloader/boot.bin
LOADER_BIN := $(TARGET)/bootloader/loader.bin
IMG := $(TARGET)/master.img

ENTRY := 0x10000
KERNEL_ELF := $(TARGET)/kernel.elf
KERNEL_BIN := $(TARGET)/kernel.bin
KERNEL_MAP := $(TARGET)/kernel.map


.PHONY: all
all: bochs


$(TARGET)/bootloader/%.bin: $(SRC)/bootloader/%.asm
	$(shell mkdir -p $(dir $@))
	nasm -f bin $< -o $@

$(TARGET)/kernel/%.o: $(SRC)/kernel/%.asm
	$(shell mkdir -p $(dir $@))
	nasm -f elf32 $< -o $@


KERNEL_SRCS := $(wildcard $(SRC)/kernel/*.asm)
KERNEL_OBJS := $(patsubst $(SRC)/kernel/%.asm, $(TARGET)/kernel/%.o, $(KERNEL_SRCS))

$(KERNEL_ELF): $(KERNEL_OBJS)
	$(shell mkdir -p $(dir $@))
	ld -m elf_i386 -static $^ -o $@ -Ttext $(ENTRY)

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