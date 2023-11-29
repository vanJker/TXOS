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

## 2. 代码分析

### 2.1 系统调用链

```c
//--> include/xos/syscall.h

// 系统调用号
typedef enum syscall_t {
    ...
    SYS_FORK = 2,   // new
} syscall_t;

/***** 声明用户态封装后的系统调用原型 *****/
...
pid_t   fork();     // new


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

### 2.2 **sys_fork**

```c
//--> include/xos/task.h

// 系统调用 fork 的处理函数
pid_t sys_fork();
```

`fork` 的内核处理流程：

1. 保证当前进程处于运行态，且不位于任意阻塞队列，以及必须是用户态进程
2. 创建子进程，并拷贝当前进程的内核栈和 PCB 来初始化子进程
3. 设置子进程的特定字段，例如 `pid`
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

![](./images/task_vmap_copy)

在父进程的 `sys_fork()` 返回前，也可以进行进程调度 `schedule()`（此时子进程已经配置完成，可以参与进程调度了），也可以不进行进程调度，这两种策略的不同仅在于父子进程的调度顺序 / 父子进程的执行顺序。

### 2.3 配置子进程内核栈

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

### 2.4 页目录拷贝
