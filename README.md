# XOS

A simple OS written in C.

## 内存布局

内核映射前的内存空间布局：

| module | start address | size |
| :----: | :-----------: | :--: |
| boot   | 0x7c00  | 512 Bytes |
| loader | 0x1000  | 2 MB |
| kernel | 0x10000 | 10 MB |

## 相关软件版本参考

- bochs == 2.7.1
- qemu == 8.1.0
- gcc == 13.2.1
- gdb == 13.2
- nasm == 2.16.01

## 参考文献

- Onix: <https://github.com/StevenBaby/onix>
- The Truth of OS: <https://github.com/xukanshan/the_truth_of_operationg_system>