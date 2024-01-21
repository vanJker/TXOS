# 070 系统调用 fork

## 1. 原理说明

```c
pid_t fork(void); // 创建子进程
```

- 子进程返回值为 0
- 父进程返回值为子进程的 ID

节选自 `man 2 fork`：

```
DESCRIPTION
       fork()  creates  a  new  process by duplicating the calling process.  The new process is referred to as the child process.  The
       calling process is referred to as the parent process.

       The child process and the parent process run in separate memory spaces.  At the time of fork() both memory spaces have the same
       content.  Memory writes, file mappings (mmap(2)), and unmappings (munmap(2)) performed by one of the processes  do  not  affect
       the other.

       The child process is an exact duplicate of the parent process except for the following points:

       •  The  child  has  its own unique process ID, and this PID does not match the ID of any existing process group (setpgid(2)) or
          session.

       •  The child's parent process ID is the same as the parent's process ID.
       
       ...
    
RETURN VALUE
       On success, the PID of the child process is returned in the parent, and 0 is returned in the child.  On failure, -1 is returned
       in the parent, no child process is created, and errno is set to indicate the error.
```


## 2. 系统调用链

```c
//--> include/xos/syscall.h

// 系统调用号
typedef enum syscall_t {
    ...
    SYS_FORK = 2,   // new
} syscall_t;

/***** 声明用户态封装后的系统调用原型 *****/
...
pid_t fork();     // new


//--> lib/syscall.c

pid_t fork() {
    return _syscall0(SYS_FORK);     // new
}


//--> kernel/syscall.c

// 初始化系统调用
void syscall_init() {
    ...
    syscall_table[SYS_FORK] = sys_fork;     // new
}
```

## 3. sys_fork

`fork` 的内核处理流程：

1. 保证当前进程处于运行态，且不位于任意阻塞队列，以及必须是用户态进程
2. 创建子进程，并拷贝当前进程的内核栈和 PCB 来初始化子进程
3. 设置子进程的特定字段，例如 `pid` 和 `ppid`
4. 对于子进程 PCB 中与内存分配相关的字段，需要新申请内存分配，例如进程虚拟内存位图 `vmap` 字段
5. 拷贝当前进程的页目录，用于设置子进程的页目录，以实现写时复制 copy on write
6. 设置子进程的内核栈，以实现 `fork()` 返回值为 0，以及参与进程调度
7. 父进程返回子进程的 ID

```c
//--> kernel/task.c

pid_t sys_fork() {
    // LOGK("fork is called\n");
    task_t *current = current_task();

    assert(current->uid != KERNEL_TASK);    // 保证调用 fork 的是用户态进程
    assert(current->state == TASK_RUNNING); // 保证当前进程处于运行态
    ASSERT_NODE_FREE(&current->node);       // 保证当前进程不位于任意阻塞队列

    // 创建子进程，并拷贝当前进程的内核栈和 PCB 来初始化子进程
    task_t *child = get_free_task();
    pid_t pid = child->pid;
    memcpy(child, current, PAGE_SIZE);

    child->pid = pid;                   // 设置子进程的 pid
    child->ppid = current->pid;         // 设置子进程的 ppid
    child->jiffies = child->priority;   // 初始时进程的剩余时间片等于优先级
    child->state = TASK_READY;          // 设置子进程为就绪态

    // 对于子进程 PCB 中与内存分配相关的字段，需要新申请内存分配
    child->vmap = (bitmap_t *)kmalloc(sizeof(bitmap_t)); // TODO: kfree
    memcpy(child->vmap, current->vmap, sizeof(bitmap_t));

    u8 *buf = (u8 *)kalloc_page(1); // TODO: kfree_page
    memcpy(buf, current->vmap->bits, PAGE_SIZE);
    child->vmap->bits = buf;

    // 拷贝当前进程的页目录
    child->page_dir = (u32)copy_pde();

    // 设置子进程的内核栈
    task_build_stack(child);

    // schedule();

    // 父进程返回子进程的 ID
    return child->pid;
}
```

对于进程虚拟内存位图的拷贝机制，可以通过以下图示来理解：

![](./images/task_vmap_copy.drawio.svg)

在父进程的 `sys_fork()` 返回前，也可以进行进程调度 `schedule()`（此时子进程已经配置完成，可以参与进程调度了），也可以不进行进程调度，这两种策略的不同仅在于父子进程的调度顺序 / 父子进程的执行顺序。

## 4. 配置子进程内核栈

由于只能是用户态进程调用 `fork()`，所以子进程对应的内核栈包括：

- 一个 **中断帧**：用于系统调用（中断 `int 80`）返回，同时可以设置中断帧的 `eax` 字段来设置系统调用的返回值。
- 一个 **任务上下文**：用于进程调度中任务上下文切换 `task_switch()` 的返回，可以设置其中的 `eip` 字段来设置 `task_switch()` 的返回地址。

在设置完子进程的中断帧和任务上下文之后，还需要设置进程控制块的内核栈指针。这样的话，就完成了子进程的设置，可以参与进程调度，并达到 `fork()` 的效果。

```c
//--> kernel/task.c

// 配置子进程的内核栈，使得子进程调用 fork() 的返回值为 0，并使得其可以参与进程调度
static void task_build_stack(task_t *task) {
    u32 addr = (u32)task + PAGE_SIZE;

    // 中断帧
    addr -= sizeof(intr_frame_t);
    intr_frame_t *intr_frame = (intr_frame_t *)addr;
    intr_frame->eax = 0; // 设置 `eax` 字段来设置子进程 `fork` 的返回值为 0

    // 任务上下文
    addr -= sizeof(task_frame_t);
    task_frame_t *task_frame = (task_frame_t *)addr;
    task_frame->eip = interrupt_exit; // 设置 `eip` 字段来设置 `task_switch()` 的返回地址为 `interrupt_exit`

    // DEBUG
    task_frame->edi = 0xaa55aa55;
    task_frame->esi = 0xaa55aa55;
    task_frame->ebx = 0xaa55aa55;
    task_frame->ebp = 0xaa55aa55;

    // 设置紫禁城的内核栈指针
    task->stack = (u32 *)addr;
}
```

## 5. 页目录拷贝

`fork` 对于父进程内存拷贝的策略是 **写时拷贝 copy on write**，实现页框 **frame** 的写时复制比较简单，只需要在写入数据时新分配一页物理帧即可，当然具体的实现还是比较复杂，这个我们后面会进行说明。但是对于页表的写时拷贝，就比较复杂了，所以在拷贝父进程的页目录时，我们把页表也进行拷贝，仅对页框实现写时拷贝策略。

子进程拷贝父进程页目录的流程：

1. 拷贝父进程页目录，并设置递归页表。

2. 对于页目录中的每个有效项，拷贝该项对应的页表。这样的话，父子进程都引用了相同的页框资源。

3. 对于父子进程均引用的页框资源，更新页框的引用数量，并将父子进程对这些页框的读写权限均设置为只读。这是因为拷贝页目录之后，父进程和子进程共享相同的页框资源，此时为了保证写时拷贝 **copy on write** 机制，必须将页框设置为只读，在父进程 / 子进程尝试向页框写入数据时，捕获该写动作，从而进行页框的分配拷贝。（**父子进程对应页框的属性都需要设置为只读，否则怎么捕捉到写入页框的动作？**）

4. 因为也设置了父进程的页表中的属性（设置页框只读），所以需要重新加载 TLB。

对于进程的写时拷贝 copy on write 机制，可以通过以下图示来理解：

![](./images/frame_copy_on_write.drawio.svg)

```c
//--> kernel/memory.c

// 拷贝当前任务的页目录（表示的用户空间）
page_entry_t *copy_pde() {
    task_t *current = current_task();

    // 拷贝当前进程的页目录
    page_entry_t *page_dir = (page_entry_t *)kalloc_page(1); // TODO: free
    memcpy((void *)page_dir, (void *)current->page_dir, PAGE_SIZE);

    // 设置递归页表。将最后一个页表项指向页目录自身，方便修改页目录和页表
    page_entry_t *entry = &page_dir[PAGE_ENTRY_SIZE - 1];
    page_entry_init(entry, PAGE_IDX(page_dir));

    // 对于页目录中的每个有效项，拷贝该项对应的页表，并更新页框的引用数量
    for (size_t pde_idx = 2; pde_idx < PAGE_ENTRY_SIZE - 1; pde_idx++) {
        page_entry_t *pde = &page_dir[pde_idx];
        if (!pde->present) continue;

        page_entry_t *page_tbl = (page_entry_t *)(PDE_RECUR_MASK | (pde_idx << 12));
        // 对于每个有效页表中的每个有效项，更新页框的引用数量，并将对应页框的读写权限设置为只读
        for (size_t pte_idx = 0; pte_idx < PAGE_ENTRY_SIZE; pte_idx++) {
            page_entry_t *pte = &page_tbl[pte_idx];
            if (!pte->present) continue;

            assert(mm.memory_map[pte->index] > 0);
            mm.memory_map[pte->index]++;    // 更新页框的引用数量
            pte->write = 0;                 // 设置页框为只读
            assert(mm.memory_map[pte->index] < 255);
        }
        
        // 拷贝页表所在页，并设置页目录项
        u32 paddr = copy_page(page_tbl); 
        pde->index = PAGE_IDX(paddr);
    }

    // 因为也设置了父进程的页表中的属性（设置页框只读），所以需要重新加载 TLB。
    set_cr3(current->page_dir);

    return page_dir;
}
```

### 5.1 页目录项

对于页目录项的处理，只需处理内核专用区域和递归项之外的页目录项即可。因为内核的恒等映射到每一个进程的，无需进行写时拷贝，也不允许写，因为用户程序是不能修改内核的。所以只需要遍历  `[2, 1023)` 之间有效的页目录项即可（第 0 页未映射，`[1, 2)` 是内核专用区域对应的页目录项，`1023` 是递归项）。

```c
    for (size_t pde_idx = 2; pde_idx < PAGE_ENTRY_SIZE - 1; pde_idx++) {
        ...
    }
```

### 5.2 访问页表

对于有效的页目录项，我们需要访问其对应的页表。我们可以通过 `get_pte()` 函数来访问页表，但是我们仅在页目录项有效时采访问页表，即页表一定存在，所以我们可以直接通过 **递归页表** 方法来访问页表，提高效率（`get_pte()` 会进行各种操作，例如判断页表是否存在、是否进行页表分配等等）。

```c
#define PDE_RECUR_MASK 0xFFC00000 // 递归页表的掩码

page_entry_t *copy_pde() {
    ...
    for (size_t pde_idx = 2; pde_idx < PAGE_ENTRY_SIZE - 1; pde_idx++) {
        ...
        page_entry_t *page_tbl = (page_entry_t *)(PDE_RECUR_MASK | (pde_idx << 12));
        ..
    }
    ...
}
```

- 注意运算的优先级，所以 `pde_idx << 12` 必须使用括号括起来。

## 6. 页拷贝

```c
//--> include/xos/memory.h

// 将虚拟地址 vaddr 所在的页拷贝到一个空闲物理页，并返回该物理页的物理地址
u32 copy_page(u32 vaddr);
```

在上面页目录拷贝当中，我们使用了 `copy_page()` 来对页表进行拷贝。由它的使用可以看出，这是一个很有意思的函数，它的参数是一个虚拟地址，返回值却是一个物理地址，而它的功能是将参数指定的虚拟地址所在的页，拷贝到一个空闲物理页，并返回一个该空闲物理页的物理地址。

乍一看这是一个很简单的函数，使用 `alloc_page()` 分配一个物理页 `phy_addr`，然后直接使用 `memcpy()` 解决不就得了吗？但是我们需要意识到，此时我们已经开启了分页机制，即我们操作的地址都是 **虚拟地址**，那么我们使用 `memcpy()` 的参数也必须都是虚拟地址，像之前我们想当然的 `memcpy(phy_addr, vir_addr, size)` 是行不通的，因为 CPU 会将参数 `phy_addr` 视为虚拟地址而不是物理地址，这会导致 MMU 在翻译地址时因为未映射发生错误，或者修改了不相干的页。

所以我们需要将分配的物理地址 `phy_addr` 临时映射到一页虚拟地址，然后再使用 `memcpy()` 进行拷贝。所以，我们需要找到一页未映射的虚拟地址，将之前获取的物理地址映射到该虚拟地址上。获取一页未映射的虚拟地址十分简单，对于用户态进程来说，检索一下进程虚拟内存位图即可。除此之外，我们还可以利用第 0 页虚拟内存，为了防止空指针访问，第 0 页虚拟内存并没有被映射，所以我们可以将第 0 页虚拟内存作为获取的物理页的映射，拷贝结束后再取消第 0 页虚拟内存的映射即可，因为我们只需要返回物理地址，临时映射到虚拟地址仅仅是为了实现拷贝。

```c
//--> kernel/memory.c

// 将虚拟地址 vaddr 所在的页拷贝到一个空闲物理页，并返回该物理页的物理地址
u32 copy_page(u32 vaddr) {
    // 保证页对齐
    ASSERT_PAGE_ADDR(vaddr);

    // 分配一个空闲物理页
    u32 paddr = alloc_page();
    
    // 临时映射到第 0 页虚拟内存
    page_entry_t *entry = (page_entry_t *)(PDE_RECUR_MASK | (0 << 12));
    page_entry_init(entry, PAGE_IDX(paddr));

    // 拷贝 vaddr 所在页的数据
    memcpy((void *)0, (void *)vaddr, PAGE_SIZE);

    // 取消第 0 页虚拟内存的临时映射
    entry->present = 0;

    // 返回物理地址
    return paddr;
}
```

- 由于第 0 页虚拟内存所在的页表必然存在（因为第 1 页开始就是内核专用区域），所以我们可以直接使用 **递归页表掩码** 来加深页表访问。
- 拷贝时使用了空指针 `0`，但这是没关系的，因为此时映射了第 0 页虚拟内存，因此不会触发异常。

## 7. Copy On Write

如前面所说的，拷贝页目录时，设置所引用的页框资源的读写权限为只读。那么当父进程 / 子进程（不仅仅只有 2 个进程，因为子进程也可以进行 `fork` 创建子进程，而且引用的也是同一部分页框）尝试写入数据时，就会触发 **Page Fault**。

这是我们故意为之的，因为我们需要像 **Lazy Allocation** 一样，借助 **Page Fault** 来进行 **Copy On Write**。其处理流程如下：

1. 当某一进程尝试写入用户空间的只读页时，说明需要对该页进行 **Copy On Write**。

2. 如果将写入的页对应的页框引用数大于 1，则需要分配一新物理页并进行拷贝，同时需要更新当前进程的页表、刷新 TLB，以及页框的引用数。

3. 如果将写入的页对应的页框引用数等于 1，说明原先引用该页框的其它进程都已经对该页进行了 **Copy On Write**，所以此时只有当前进程引用了该页框。那么只需将该页框的读写权限重新设置为可写即可。

```c
//-> kernel/memory.c

void page_fault_handler(...) {
    ...
    // 尝试写入用户空间的只读页（存在且只读）时，需要对该页进行 Copy On Write
    if (page_error->present) {
        assert(page_error->write);

        // 获取页表项以及对应页框的索引
        page_entry_t *pgtbl = get_pte(vaddr, false);
        page_entry_t *pte = &pgtbl[PTE_IDX(vaddr)];
        u32 pidx = pte->index;

        assert(memory_map()[pidx] > 0);
        if (memory_map()[pidx] == 1) {
            // 将写入的页对应的页框引用数等于 1，说明原先引用该页框的其它进程都已经对该页进行了 Copy On Write，
            // 所以此时只有当前进程引用了该页框。那么只需将该页框的读写权限重新设置为可写即可
            pte->write = 1;
            LOGK("WRITE page for 0x%p\n", vaddr);
        } else {
            // 如果将写入的页对应的页框引用数大于 1，则需要分配一新物理页并进行拷贝，
            // 同时需要更新当前进程的页表、刷新 TLB，以及页框的引用数。
            u32 paddr = copy_page(PAGE_ADDR(PAGE_IDX(vaddr)));
            page_entry_init(pte, PAGE_IDX(paddr));
            flush_tlb(vaddr);
            memory_map()[pidx]--;
            LOGK("WRITE page for 0x%p\n", vaddr);
        }
        assert(memory_map()[pidx] > 0);
        
        return;
    }
    ...
}
```

> 注：这一部分处理需要在 **Lazy Allocation** 处理之前。

为了实现在 **Page Fault** 处理中实现页框的 **Copy On Write** 机制，我们需要将内存管路的一部分接口开放：

```c
//--> include/xos/memory.h

// 初始化页表项，设置为指定的页索引 | U | W | P
void page_entry_init(page_entry_t *entry, u32 index);

// 获取页目录
page_entry_t *get_pde();

// 获取虚拟内存 vaddr 所在的页表
page_entry_t *get_pte(u32 vaddr, bool create);

// 刷新 TLB 中与 vaddr 有关的项
void flush_tlb(u32 vaddr);

// 物理内存数组
u8 *memory_map();
```

<details>

<summary>用户空间只读页的处理</summary>

目前的处理可能会与用户空间的只读页发生冲突，但是我们现在还不支持用户空间的只读页，所以先这么处理，后续再进行改进。

</details>

## 8. 功能测试

```c
//--> kernel/thread.c

// 初始化任务 init 的用户态线程
static void user_init_thread() {
    while (true) {
        pid_t pid = fork();

        if (pid == 0) {
            // child process
            printf("fork after child:  fork() = %d, pid = %d, ppid = %d\n", pid, get_pid(), get_ppid());
        } else {
            // parent process
            printf("fork after parent: fork() = %d, pid = %d, ppid = %d\n", pid, get_pid(), get_ppid());
        }

        hang();
    }
}
```

预期输出为：

```c
fork after parent: fork() = 3, pid = 1, ppid = 0
fork after child:  fork() = 0, pid = 3, ppid = 1
```

## 3.1 Copy On Write

在 `sys_fork()` 和 `page_fault_handler()` 处打断点，观察父子进程的 **Copy On Write** 机制的过程。

- 观察物理数组 `memory_map` 中对应页框引用数的变化
- 观察父子进程页表的变化

## 3.2 子进程内核栈

在 `task_build()` 和 `schedule()` 处打断点，观察子进程内核栈的配置以及参与调度的过程。

- 在任务上下文切换 `task_switch()` 中观察寄存器的值，例如栈上存储的连续 4 个 `0xaa55aa55` 的寄存器值。以及借助符号表 `kernel_map` 确定上下文切换后返回的地址。

- 在中断返回 `interrupt_exit()` 中观察寄存器的值，例如中断返回值 `eax`。以及借助符号表 `kernel.map` 确定中断返回的地址。

可以使用 Bochs 或 Qemu 进行调试，Qemu 调试时输出信息可以参考 [Debug 指南 - Qemu Debug](../others/debug.md#qemu-debug)。
