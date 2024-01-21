# 071 系统调用 exit

## 1. 原理说明

```c
noreturn void exit(int status); // 终止当前进程
```

节选自 `man 2 exit`：

```
DESCRIPTION
       _exit()  terminates  the  calling  process  "immediately".   Any  open file descriptors belonging to the
       process are closed.  Any children of the process are inherited by init(1) (or by the nearest "subreaper"
       process as defined through the use of the prctl(2)  PR_SET_CHILD_SUBREAPER  operation).   The  process's
       parent is sent a SIGCHLD signal.

       The  value status & 0xFF is returned to the parent process as the process's exit status, and can be col‐
       lected by the parent using one of the wait(2) family of calls.

       The function _Exit() is equivalent to _exit().

RETURN VALUE
       These functions do not return.
```

- 这个系统调用不会返回，因为调用后进程已被”杀死“。

- 系统调用 `exit` 和 `waitpid` 联系紧密。`exit` 的参数是进程结束状态，因为 `exit` 不会返回，所以为了获取子进程的结束状态，需要父进程通过 `waitpid` 来获取。

- 调用 `exit` 并不会完全杀死进程，只是让进程处于“僵死”状态，释放占用的内存，但仍然在进程表中保留 PCB。而释放“僵死”的子进程 PCB 这项“收尸”任务，则留给父进程来执行。

因为父进程调用 `waitpid` 可以获取“僵死”的子进程的结束状态，而调用 `exit` 陷入“僵死”状态的子进程仍然可以在进程表中保留 PCB。所以，为了保存进程的结束状态，即调用 `exit` 时的参数，可以将这个进程结束状态保存在 PCB。

```c
//--> include/xos/task.h

// 任务控制块 TCB
typedef struct task_t {
    ...
    i32 status; // 进程结束状态
    ...
} task_t;
```

## 2. 系统调用链

```c
//--> include/xos/syscall.h

// 系统调用号
typedef enum syscall_t {
    ...
    SYS_EXIT = 1,
} syscall_t;

/***** 声明用户态封装后的系统调用原型 *****/
...
void exit(int status);


//--> lib/syscall.c

void exit(int status) {
    _syscall1(SYS_EXIT, status);
}


//--> kernel/syscall.c

// 初始化系统调用
void syscall_init() {
    ...
    syscall_table[SYS_EXIT] = sys_exit;
}
```

## 3. sys_exit

`exit` 和 `fork` 的处理是镜像对应的，`fork` 是创建子进程，并分配用户虚拟内存位图、用户页表等，`exit` 则是结束当前进程，并释放所持有的内存。当然也有未对应的处理，例如 `fork` 会在分配一个新的 PCB，而 `exit` 不会抹除 PCB，这是父进程的任务。

参考构造函数和析构函数的对应关系，对应进程持有内存的释放，应当与 `fork` 分配时顺序相反。

`exit` 的内核处理流程：

1. 保证当前进程处于运行态，且不位于任意阻塞队列，以及必须是用户态进程
2. 设置进程 PCB 中与状态相关的字段，例如 `state`, `status`
2. 对于子进程 PCB 中与内存分配相关的字段，需要进行内存释放，例如进程虚拟内存位图 `vmap` 字段
3. 释放当前进程的页目录、页表，以及页框，即释放用户空间
4. 将当前进程的所有子进程的 `ppid` 字段，设置为当前进程的 `ppid`（进程关系的继承）
5. 由于当前进程已经消亡，立即进行进程调度

```c
//--> kernel/task.c

void sys_exit(i32 status) {
    // LOGK("exit is called\n");
    task_t *current = current_task();

    // 保证当前进程处于运行态，且不位于任意阻塞队列，以及必须是用户态进程
    assert(current->uid != KERNEL_TASK);    // 保证调用 exit 的是用户态进程
    assert(current->state == TASK_RUNNING); // 保证当前进程处于运行态
    ASSERT_NODE_FREE(&current->node);       // 保证当前进程不位于任意阻塞队列

    // 设置进程 PCB 中与状态相关的字段，例如 `state`, `status`
    current->state = TASK_DIED; // 进程陷入“僵死”
    current->status = status;   // 保存进程结束状态

    // 对于子进程 PCB 中与内存分配相关的字段，需要进行内存释放，例如进程虚拟内存位图 `vmap` 字段
    u8 *buf = current->vmap->bits;
    kfree_page((u32)buf, 1);
    current->vmap->bits = NULL;

    kfree((void *)current->vmap);
    current->vmap = NULL;

    // 释放当前进程的页目录、页表，以及页框，即释放用户空间
    free_pde();

    // 将当前进程的所有子进程的 `ppid` 字段，设置为当前进程的 `ppid`（进程关系的继承）
    for (size_t i = 2; i < NUM_TASKS; i++) {
        if (task_queue[i] == NULL)    continue;
        if (task_queue[i] == current) continue;
        if (task_queue[i]->ppid != current->pid) continue;

        task_queue[i]->ppid = current->ppid;
    }

    LOGK("task 0x%p (pid = %d) exit...\n", current, current->pid);

    // 由于当前进程已经消亡，立即进行进程调度
    schedule();
}
```

- 由于 0 号进程为 `idle` 进程，1 号进程为 `init` 进程，这两个进程都不会存在父进程（所以初始化时，`ppid` 字段为 0）。为了后续 `waitpid` 的正确性，在遍历进程表时应该跳过这两个进程（`wait(0)` 表示等待任意子进程结束）。

## 4. 释放页表

```c
//--> include/xos/memory.h

// 释放当前任务的页目录（表示的用户空间）
void free_pde();
```

`sys_exit()` 使用了 `free_pde()` 来释放进程的页表，类似的，`free_pde()` 函数与 [<>] 一节实现的 `copy_pde()` 是镜像对应的。当然 `free_pde()` 会更加简单直接一点，主要逻辑是遍历页表并释放对应页即可。

- 页目录是通过内核堆内存分配的，使用了 `kalloc_page()`
- 页表和页框都是通过物理数组来分配的，使用的是 `alloc_page()`
- 页目录前 2 项（即前 8M）是内核专用空间，最后一项是递归项，都不可进行释放操作
- 进行释放操作时，需要额外注意函数参数需要的是物理地址还是虚拟地址

```c
//--> kernel/memory.c

// 释放当前任务的页目录（表示的用户空间）
void free_pde() {
    LOGK("Before free_pde(), free pages: %d", mm.free_pages);

    task_t *current = current_task();

    page_entry_t *page_dir = (page_entry_t *)current->page_dir;
    // 对于页目录中的每个有效项，释放该项对应的页表
    for (size_t pde_idx = 2; pde_idx < PAGE_ENTRY_SIZE - 1; pde_idx++) {
        page_entry_t *pde = &page_dir[pde_idx];
        if (!pde->present) continue;

        page_entry_t *page_tbl = (page_entry_t *)(PDE_RECUR_MASK | (pde_idx << 12));
        // 对于每个有效页表中的每个有效项，释放对应的页框
        for (size_t pte_idx = 0; pte_idx < PAGE_ENTRY_SIZE; pte_idx++) {
            page_entry_t *pte = &page_tbl[pte_idx];
            if (!pte->present) continue;

            free_page(PAGE_ADDR(pte->index));
        }

        free_page(PAGE_ADDR(pde->index));
    }

    // 释放页目录
    kfree_page((u32)page_dir, 1);

    LOGK("After free_pde(), free pages: %d", mm.free_pages);
}
```

- 注意运算的优先级，所以 `pde_idx << 12` 必须使用括号括起来。

## 5. 功能测试

```c
//--> kernel/thread.c

// 初始化任务 init 的用户态线程
static void user_init_thread() {
    size_t counter = 0;

    while (true) {
        // printf("task in user mode can use printf! %d\n", counter++);
        // printf("init thread pid: %d, ppid: %d, counter: %d\n", get_pid(), get_ppid(), counter++);
        pid_t pid = fork();

        if (pid == 0) {
            // child process
            printf("fork after child:  fork() = %d, pid = %d, ppid = %d\n", pid, get_pid(), get_ppid());
            exit(0);
        } else {
            // parent process
            printf("fork after parent: fork() = %d, pid = %d, ppid = %d\n", pid, get_pid(), get_ppid());
        }

        sleep(1000);
    }
}
```

预期为：

- `free_pde()` 中释放了两页物理内存，一页栈内存，一页对应页表的内存
- 可用物理页在减少，因为子进程调用 `exit` 后只是陷入“僵死”，但是并没有释放 PCB 和内核栈所在的物理页，同时这也导致了新进程的 PID 一直在增大
- 最终无法分配可用的空闲进程，导致触发 `panic`
