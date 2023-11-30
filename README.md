# XOS

A simple OS written in C.

## Memory Layout

Before Memory Mapping：

| module | start address | size |
| :----: | :-----------: | :--: |
| boot   | 0x7c00        | 512 B |
| loader | 0x1000        | 2 KB  |
| kernel | 0x10000       | 100 KB |

After Memory Mapping：

| module | start address | size |
| :----: | :-----------: | :--: |
| kernel | 0x000000      | 8 MB |
| user   | 0x800000      | 10 MB|

## Tools

- bochs == 2.7.1
- qemu == 8.1.0
- gcc == 13.2.1
- gdb == 13.2
- nasm == 2.16.01

## References

- Onix: <https://github.com/StevenBaby/onix>
- The Truth of OS: <https://github.com/xukanshan/the_truth_of_operationg_system>