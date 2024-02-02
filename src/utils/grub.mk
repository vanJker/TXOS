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

.PHONY: iso
iso: $(ISO)

.PHONY: bochs-grub
bochs-grub: $(ISO)
	bochs -q -f ./bochs/bochsrc.grub -unlock

QEMU_CDROM := -boot d \
				-drive file=$(ISO),media=cdrom \

.PHONY: qemu-grub
qemu-grub: $(ISO)
	$(QEMU) $(QFLAGS) $(QEMU_CDROM)
