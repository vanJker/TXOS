# Memory Management

## 内存映射变化

进入内核时的物理内存布局：

![](./images/memory_map_01.drawio.svg)

映射内核虚拟内存空间后：

![](./images/memory_map_02.drawio.svg)
![](./images/memory_map_03.drawio.svg)

映射用户虚拟内存空间：

![](./images/memory_map_04.drawio.svg)
![](./images/memory_map_05.drawio.svg)

## 内存管理机制

| 机制/策略 | 所在文档 |
| :------- | :------ |
| 物理内存数组 | [040 物理内存管理](./040_physical_memory.md) |
| 两级页表映射 | [041 内存映射原理 (分页机制)](./041_memory_paging.md) |
| 内核空间页分配 | [044 内存虚拟内存管理](./044_kernel_virtual_memory.md) |
| 内核空间动态内存分配 | [065 内核堆内存管理](./065_kernel_heap_memory.md) |
| 用户空间栈 Lazy Alloction | [067 进程用户态栈](./067_user_stack.md) |
| 用户空间堆 Lazy Allocation | [068 系统调用 brk](../08_syscall/068_brk.md) |
| 进程 fork 时页框 (frame) Copy On Write | [070 系统调用 fork](../08_syscall/070_fork.md) |
