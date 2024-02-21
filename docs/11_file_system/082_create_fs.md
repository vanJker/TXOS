# 创建文件系统

## 文件系统

为简单起见，本操作系统使用 minix 第一版文件系统。主要的好处是，可以抄 linux 0.11 的代码。

为了简单起见，通过开发环境的主机来创建文件系统：

1. 通过开发环境的主机挂载 XOS 的主从磁盘，到主机的设备上
2. 通过开发环境的主机已挂载的设备，在其分区中创建特定格式的文件系统
3. 通过开发环境的主机挂载上述设备的文件系统，到主机的指定目录
4. 通过开发环境的主机在上述挂载目录下，创建目录和文件
5. 通过开发环境的主机卸载上述涉及到的设备和文件系统

可以通过 Makefile 的 master.img 相关的操作，来体会上述逻辑：

```makefile
$(TARGET)/master.img: $(BOOT_BIN) $(LOADER_BIN) $(SYSTEM_BIN) $(SYSTEM_SYM) $(SRC)/utils/master.sfdisk
    ...
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
```

> slave.img 相关的操作逻辑也是类似的。

另外，为了查询 XOS 磁盘、分区内文件系统的内容，实现了 mount0, umount0, mount1, umount1 的 target，用于挂载/卸载相应的设备、文件系统，方便我们查看对应设备内的文件系统的内容。

以 master.img 涉及的 mount0, umount0 为例：

```makefile
# 挂载 master.img 到主机设备 loop0 和目录 /mnt
.PHONY: mount0
mount0: $(BUILD)/master.img
	sudo losetup /dev/loop0 --partscan $<
	sudo mount /dev/loop0p1 /mnt
	sudo chown ${USER} /mnt 

# 卸载 master.img 相关的设备和文件系统
.PHONY: umount0
umount0: /dev/loop0
	-sudo umount /mnt
	-sudo losetup -d $<
```

> slave.img 涉及的 mount1, unmount1 的逻辑也是类似的。

## 磁盘分区

为了简单起见，我们将 master.img 和 slave.img 都设置为只划分一个分区：

> utils/master.sfdisk 和 utils/slave.sfdisk
```
target/master.img1 : start=        2048, size=       16384, type=83
```

- [077 硬盘分区](../09_device_driver/077_disk_partition.md)

## 功能测试

阅读 Makefile 可知，生成的 img 镜像 master.img, slave.img 的第 1 个分区都会创建 Minix V1 的文件系统，并且在该分区的文件系统内创建相应的目录和文件 (以及文件的内容)：

- master.img
    - Directory: 
        - /home
        - /d1/d2/d3/d4
    - File:
        - /hello.txt 
            - "Hello XOS!!!, from root direcotry file..."
        - /home/hello.txt
            - "Hello XOS!!!, from home direcotry file..."
- slave.img
    - File:
        - /hello.txt
            - "slave root direcotry file..."

在生成 img 镜像后 (通过 `make img` 命令)，通过 mount0, umount0, mount1, umount1 这些 makefile 的 targets，以及 cat 命令，来查看是否在 XOS 对应的磁盘中正确创建文件、目录。以 master.img 为例：

```bash
# 挂载 XOS 的设备和文件系统
$ make mount0

# 列出挂载的 XOS 的文件系统的内容
$ ls /mnt
d1  hello.txt  home
$
$ tree /mnt
/mnt/
├── d1
│   └── d2
│       └── d3
│           └── d4
├── hello.txt
└── home
    └── hello.txt

6 directories, 2 files

# 查看对应文件的内容
$ cat /mnt/hello.txt 
Hello XOS!!!, from root direcotry file...
$ cat /mnt/home/hello.txt 
Hello XOS!!!, from home direcotry file...

# 卸载 XOS 的设备和文件系统
$ make umount0
```

> slave.img 的操作也是类似的，请自行完成。

也可以通过 VS Code 的 Hex Editor 插件，来直接查看 master.img, slave.img 中的内容，确认是否按预期一样创建了目录和文件。

## 参考文献

- man mkfs.minix
- man losetup
- man mount
- man umount
- man chown