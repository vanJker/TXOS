# 016 QEMU 和 VMare

## 1. 安装 qemu

```bash
# 这条命令只会安装 i386 和 x86_64 相关的架构
sudo pacman -S qemu
# 这条命令会安装其它的架构，例如 aarch64, riscv64
sudo pacman -S qemu-arch-extra
```

## 2. 补充 qemu 的 run/debug 启动选项

```makefile
QEMU := qemu-system-i386
QEMU_FLAGS := -m 32M \
				-boot c \
				-hda $(IMG)

.PHONY: qemu-run
qemu-run: $(IMG)
	$(QEMU) $(QEMU_FLAGS)

.PHONY: qemu-debug
qemu-debug: $(IMG)
	$(QEMU) $(QEMU_FLAGS) -s -S
```

参数说明：

| 参数 | 作用 |
| --- | ---- |
| `-m`  | 指定虚拟系统的内存大小，`-m 32M` 即为 32M 大小内存 |
| `-boot` | 指定启动方式，`-boot c` 为从硬盘启动 |
| `-hda` | 指定启动的硬盘文件 |
| `-s` | 表示监听本地主机的 1234 端口，等待 gdb 连接 |
| `-S` | 表示当 gdb 尚未连接到 qemu 时，不执行 |

## 3. 从 VMare 启动系统

本项目编写的内核可以在 VMare 启动，而这只需要进行一些格式转换。

转换硬盘格式：

```makefile
VMDK := $(TARGET)/master.vmdk
$(VMDK): $(IMG)
	qemu-img convert -pO vmdk $< $@

.PHONY: vmdk
vmdk: $(VMDK)
```

## 4. 参考文献

- [QEMU - Documentation](https://www.qemu.org/docs/master/)