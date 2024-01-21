# 067 进程用户态栈

## 1. 内存布局

本节的主要任务就是将用户栈映射到 $0x7e00000$ ~ $0x8800000$ 这 $10M$ 虚拟地址空间处。

![](./images/memory_map_05.drawio.svg)

## 2. 缺页异常

在创建用户任务时直接分配 $10M$ 内存作为用户栈十分浪费，因为一般的用户任务并不会使用这么多的栈空间。所以我们利用 **缺页异常** 来按需分配用户栈空间。

主要原理为：访问未被映射的虚拟地址会触发缺页异常，而且用户栈是自顶向下增长的。所以在触发缺页异常时，检查触发异常的地址，是否位于用户栈空间内，如果是，则分配用户栈空间。

---

触发缺页异常会压入 **错误码**，这个错误码附带一些错误信息，比如每一个有效位都表示触发该缺页异常的各种原因。

错误码的结构如下：

![](./images/page_fault.jpg)

我们稍微解释一下会涉及的位：

- `P`：0 - 访问了不存在（未被映射）的页，1 - 访问了高权级才能访问的页
- `W/R`：0 - 读取时触发了异常，1 - 写入时触发了异常
- `U/S`：0 - 在内核态触发了异常，1 - 在用户态触发了异常

> 关于异常，可以参考 [<031 异常>](../04_interrupt_and_clock/031_exception.md) 这一小节。

## 3. 代码分析

### 3.1 用户栈内存

根据原理说明，定义一些与用户栈内存相关的常量。

> `include/xos/memory.h`

```c
// 用户栈顶地址 136M
#define USER_STACK_TOP  USER_MEMORY_TOP 
// 用户栈大小 10M
#define USER_STACK_SIZE 0xa00000        
// 用户栈底地址（136M - 8M）
#define USER_STACK_BOOTOM (USER_MEMORY_TOP - USER_STACK_SIZE) 
```

### 3.2 CR2 寄存器

在触发缺页异常时，`cr2` 寄存器当中会存放触发缺页异常的地址。所以我们需要实现读取 `cr2` 寄存器值的功能。

> `kernel/memory.c`

```c
u32 get_cr2() {
    // 根据函数调用约定，将 cr2 的值复制到 eax 作为返回值
    asm volatile("movl %cr2, %eax");
}
```

### 3.3 页目录拷贝

在上一节的 FAQ 中我们已经知道，需要进行页目录拷贝来保证用户进程之间的隔离性。所以我们实现页目录拷贝功能，需要注意的是，最后需要修改页目录的最后一项指向页目录自身（否则最后一项指向的是被拷贝的那一个页目录）。

> `kernel/memory.c`

```c
// 拷贝当前任务的页目录
page_entry_t *copy_pgdir() {
    task_t *current = current_task();
    page_entry_t *pde = (page_entry_t *)kalloc_page(1); // TODO: free
    memcpy((void *)pde, (void *)current->page_dir, PAGE_SIZE);

    // 将最后一个页表项指向页目录自身，方便修改页目录和页表
    page_entry_t *entry = &pde[1023];
    page_entry_init(entry, PAGE_IDX(pde));

    return pde;
}
```

---

因为在创建任务 / 紧凑时，并没有分配新的页目录和进程位图（使用的都是内核的页目录和位图），所以与用户虚拟内存位图类似，我们在切换到用户态函数 `real_switch_to_user_mode()` 当中，增加拷贝 **内核页目录** 逻辑（因为每个进程的页目录前 $8M$ 都是恒等映射到内核部分），并切换到该进程的页目录（即刚刚拷贝获得的新页目录）。

这样在后续使用 `link_page()` 或 `unlink_page()` 来建立或取消映射，只会影响用户进程自己的虚拟内存空间，实现了隔离性。

> `kernel/task.c`

```c
static void real_task_to_user_mode(target_t target) {
    task_t *current = current_task();

    // 设置用户虚拟内存位图
    current->vmap = (bitmap_t *)kmalloc(sizeof(bitmap_t)); // TODO: kfree()
    u8 *buf = (u8 *)kalloc_page(1); // TODO: kfree_page()
    bitmap_init(current->vmap, buf, PAGE_SIZE, KERNEL_MEMORY_SIZE / PAGE_SIZE);

    // 设置用户任务/进程页表
    current->page_dir = (u32)copy_pgdir();
    set_cr3(current->page_dir);

    ...
}
```

---

将函数 `task_tss_activate()` 增强为 `task_activate()`，其作用主要是在切换到下一个任务 / 进程前，对该任务进行激活操作。例如，进行页目录切换，设置 `tss` 的 `esp0` 字段为该任务对应的内核栈顶。

> `kernel/task.c`

```c
// 任务激活，在切换到下一个任务之前必须对该任务进行一些激活操作
void task_activate(task_t *task) {
    assert(task->magic == XOS_MAGIC);   // 检测栈溢出

    // 如果下一个任务的页表与当前任务的页表不同，则切换页表
    if (task->page_dir != get_cr3()) {
        set_cr3(task->page_dir);
    }
    
    // 如果下一个任务是用户态的任务，需要将 tss 的栈顶位置修改为该任务对应的内核栈顶
    if (task->uid != KERNEL_TASK) {
        tss.esp0 = (u32)task + PAGE_SIZE;
    }
}

// 任务调度
void schedule() {
    ...

    task_activate(next);

    task_switch(next);
}
```

### 3.4 缺页异常错误码

根据原理说明，定义一个结构体作为缺页异常的错误码，方便后续我们判断触发缺页异常的原因。

> `include/xos/interrupt.h`

```c
// Page Fault 缺页异常的错误码
typedef struct page_error_code_t {
    u8 present : 1;
    u8 write : 1;
    u8 user : 1;
    u8 rsvd : 1;
    u8 fetch : 1;
    u8 protection : 1;
    u8 shadow : 1;
    u8 reserved0;
    u8 sgx : 1;
    u16 reserved1;
} _packed page_error_code_t;
```

### 3.5 Lazy Allocation

有了之前实现的功能，现在我们可以来实现缺页异常的处理函数。

1. 通过 `cr2` 寄存器获取触发缺页异常的地址。
2. 获取缺页异常的错误码。
3. 如果触发缺页异常的原因是，在用户态访问了不该存在的页，且该页位于用户栈空间内，则通过 `link_page()` 给该页进行映射，实现用户栈扩展。
4. 否则触发 `panic`。

> `kernel/interrupt.c`

```c
// 缺页异常处理函数
void page_fault_handler(
    int vector,
    u32 edi, u32 esi, u32 ebp, u32 esp,
    u32 ebx, u32 edx, u32 ecx, u32 eax,
    u32 gs, u32 fs, u32 es, u32 ds,
    u32 vector0, u32 error, u32 eip, u32 cs, u32 eflags
) {
    assert(vector == 0xe); // 缺页异常中断向量号

    u32 vaddr = get_cr2(); // 获取触发缺页异常的虚拟地址
    LOGK("Page fault address 0x%p\n", vaddr);

    // 前 8M 为恒等映射，不可能触发缺页异常
    assert(KERNEL_MEMORY_SIZE <= vaddr && vaddr < USER_STACK_TOP);

    // 缺页异常错误码
    page_error_code_t *page_error = (page_error_code_t *)&error;

    // 如果缺页异常发生在用户栈范围内
    if (!page_error->present && page_error->user 
        && (vaddr > USER_STACK_BOOTOM)
    ) {
        u32 vpage = PAGE_ADDR(PAGE_IDX(vaddr));
        link_page(vpage);
        return;
    }

    panic("Page Fault!!!");
}
```

- `PAGE_ADDR(PAGE_IDX(vaddr))` 的作用相当于取地址 `vaddr` 所在页的起始地址。

---

然后需要在异常处理函数表，注册该缺页异常处理函数：

> `kernel/interrupt.c`

```c
// 初始化中断描述符表，以及中断处理函数表
void idt_init() {
    ...
    // 初始化异常处理函数表
    for (size_t i = 0; i < EXCEPTION_SIZE; i++) {
        handler_table[i] = exception_handler;
    }
    handler_table[0xe] = page_fault_handler; // 注册缺页异常处理函数
    ...
}
```

## 4. 功能测试

通过一个递归函数来测试缺页异常是否按照我们预期来执行。

```c
void test_recursion() {
    char temp[0x1000]; // 每次占用 4096 Bytes
    test_recursion();
}

static void user_init_thread() {
    size_t counter = 0;

    while (true) {
        printf("task in user mode can use printf! %d\n", counter++);
        test_recursion();
        sleep(1000);
    }
}
```

- 使用调试来跟踪触发缺页异常时的指令执行流程。
- 使用 Bochs 来查看页表的变化，观察缺页异常是否按照预期扩展用户栈。
- 观察当访问地址位于用户栈范围之外时，缺页异常是否会按照预期进行 `panic`。

## 5. 参考文献

- Intel® 64 and IA-32 Architectures Software Developer's Manual Volume 3 Chapter 6 Interrupt and Exception Handling