# 系统调用 waitpid

## 1. 原理说明

```c
pid_t waitpid(pid_t pid, int *status); // 等待进程状态变化
```

节选自 `man 2 waitpid`：

```
DESCRIPTION
       All  of these system calls are used to wait for state changes in a child of the calling process, and obtain information about the child whose state has changed.  A state
       change is considered to be: the child terminated; the child was stopped by a signal; or the child was resumed by a signal.  In the case of a terminated child, performing
       a wait allows the system to release the resources associated with the child; if a wait is not performed, then the terminated child remains in a "zombie" state (see NOTES
       below).

       If a child has already changed state, then these calls return immediately.  Otherwise, they block until either a child changes state or a signal handler  interrupts  the
       call  (assuming  that  system  calls  are not automatically restarted using the SA_RESTART flag of sigaction(2)).  In the remainder of this page, a child whose state has
       changed and which has not yet been waited upon by one of these system calls is termed waitable.

   wait() and waitpid()
       The wait() system call suspends execution of the calling thread until one of its children terminates.  The call wait(&wstatus) is equivalent to:

           waitpid(-1, &wstatus, 0);

       The waitpid() system call suspends execution of the calling thread until a child specified by pid argument has changed state.  By default, waitpid() waits only for  ter‐
       minated children, but this behavior is modifiable via the options argument, as described below.

       The value of pid can be:

       < -1   meaning wait for any child process whose process group ID is equal to the absolute value of pid.

       -1     meaning wait for any child process.

       0      meaning wait for any child process whose process group ID is equal to that of the calling process at the time of the call to waitpid().

       > 0    meaning wait for the child whose process ID is equal to the value of pid.

RETURN VALUE
       wait(): on success, returns the process ID of the terminated child; on failure, -1 is returned.

       waitpid(): on success, returns the process ID of the child whose state has changed; if WNOHANG was specified and one or more child(ren) specified by pid exist, but  have
       not yet changed state, then 0 is returned.  On failure, -1 is returned.
```

我们实现的系统调用 `waitpid` 比较简单，相对 Linux 来说并没有 `option` 参数，而且目前只支持等待子进程 **终止**。

- 参数 `pid` 指定等待的子进程，如果为 -1 则表示等待任一子进程结束
- 参数 `status` 表示获取指定子进程的结束状态
- 返回值为等待的子进程标识符，返回 -1 表示调用失败

### 1.3 父子进程执行流

由于父子进程的指令流是并行执行的，所以会有两种情况：

- 子进程先 `exit`，父进程才 `waitpid`
- 父进程先 `waitpid`，子进程才 `exit`

对于这两种执行流的情况，man 手册也介绍了处理机制：

```
If a child has already changed state, then these calls return immediately.  Otherwise, they block until either a child changes state or a signal handler  interrupts  the call.
```

翻译一下，就是 ***如果（参数指定的）子进程已经改变状态了，则这个系统调用会直接返回。否则的话，调用 `waitpid` 的进程会阻塞，直到（参数指定的）子进程改变状态或信号量打断了这个系统调用。***

由于我们目前并没有信号量机制，而且只支持等待子进程终止。所以，我们可以提炼出适合我们的处理机制：

- 如果（参数指定的）子进程已经终止了，那么系统调用 `waitpid` 直接返回终止的子进程标识符。
- 否则的话，阻塞进行 `waitpid` 的进程，直到（参数指定的）子进程终止，由该子进程进行唤醒被阻塞的父进程。

因为父进程调用 `waitpid` 可能会陷入阻塞状态，并由 `exit` 的子进程来进行唤醒。所以，为了保存父进程的等待子进程信息，使得子进程调用 `exit` 时的可以唤醒父进程，可以将这个进程结束状态保存在 PCB。

```c
//--> include/xos/task.h

// 任务控制块 TCB
typedef struct task_t {
    ...
    pid_t waitpid; // 进程等待的子进程 pid
    ...
} task_t;
```

### 1.2 ZOMBIE

关于上一节所说的“僵尸”进程，也可以参考手册中的解释：

```
NOTES
       A child that terminates, but has not been waited for becomes a "zombie".  The kernel maintains a  minimal
       set  of information about the zombie process (PID, termination status, resource usage information) in or‐
       der to allow the parent to later perform a wait to obtain information about the child.  As long as a zom‐
       bie is not removed from the system via a wait, it will consume a slot in the kernel process table, and if
       this table fills, it will not be possible to create further processes.  If a parent  process  terminates,
       then its "zombie" children (if any) are adopted by init(1), (or by the nearest "subreaper" process as de‐
       fined through the use of the prctl(2) PR_SET_CHILD_SUBREAPER operation); init(1) automatically performs a
       wait to remove the zombies.
```

## 2. 系统调用链

```c
//--> include/xos/syscall.h

// 系统调用号
typedef enum syscall_t {
    ...
    SYS_WAITPID = 7,    // new
} syscall_t;

/***** 声明用户态封装后的系统调用原型 *****/
...
pid_t waitpid(pid_t pid, int *status);


//--> lib/syscall.c

pid_t waitpid(pid_t pid, int *status) {
    return _syscall2(SYS_WAITPID, pid, status);
}


//--> kernel/syscall.c

// 初始化系统调用
void syscall_init() {
    ...
    syscall_table[SYS_WAITPID]  = sys_waitpid;
}
```

## 3. sys_waitpid

原理说明处我们已经探讨了 `waitpid` 的处理机制，现在我们把这个机制扩充完善成处理流程。

1. 保证当前进程处于运行态，且不位于任意阻塞队列，以及必须是用户态进程
2. 遍历进程表，寻找指定的子进程。
3. 如果寻找到指定的子进程：
    - 如果找到的子进程处于“僵死”态，则释放子进程未释放的资源（即 PCB 和内核栈所处的页），并返回子进程的标识符和结束状态（如果 `status` 不是空指针）。
    - 如果找到的子进程并未处于“僵死”态，则阻塞当前进程，直到被 `exit` 的子进程唤醒，重新进入 ***步骤 1*** 执行流程。
4. 如果没有寻找到指定的子进程，则直接返回 -1

```c
//--> kernel/task.c

pid_t sys_waitpid(pid_t pid, i32 *status) {
    // LOGK("waitpid is called\n");
    task_t *current = current_task();
    task_t *child = NULL;

    assert(current->uid != KERNEL_TASK);    // 保证调用 waitpid 的是用户态进程
    assert(current->state == TASK_RUNNING); // 保证当前进程处于运行态
    ASSERT_NODE_FREE(&current->node);       // 保证当前进程不位于任意阻塞队列

    while (true) {
        bool has_child = false; // 是否寻找到符合条件的子进程

        for (size_t i = 2; i < NUM_TASKS; i++) {
            if (task_queue[i] == NULL)    continue;
            if (task_queue[i] == current) continue;

            if (task_queue[i]->ppid != current->pid)    continue;
            if (task_queue[i]->pid != pid && pid != -1) continue;

            has_child = true; // 已找到符合条件的子进程
            child = task_queue[i];
            if (child->state == TASK_DIED) {
                // 子进程处于“僵死”
                task_queue[i] = NULL; // 释放进程表中占据的位置
                goto rollback;
            } else {
                // 子进程未处于“僵死”
                break;
            }
        }

        if (has_child) {
            // 找到子进程但子进程未处于“僵死”态
            current->waitpid = child->pid;
            task_block(current, NULL, TASK_WAITING);
            continue;
        } else {
            // 未找到子进程
            break;
        }
    }

    return -1;

rollback:
    assert(child != NULL);
    pid_t ret = child->pid;     // 保存子进程的 pid 用于返回
    if (status != NULL) {       // 传递子进程的结束状态
        *status = child->status;
    }
    kfree_page((u32)child, 1);  // 释放 PCB 和内核栈所在的页
    return ret;
}
```

这个实现值得仔细琢磨，

- 使用了被世人认为的洪水猛兽 `goto`，但其实在 Linux 中 `goto` 的使用相当普遍，常见于错误处理。这里使用 `goto` 语句使得可以跳出两重循环，并使得代码结构更加清晰（因为也可以直接在 `if` 内进行处理，不过这样代码不够规整，嵌套层数过多）。

- 为了使得父进程阻塞结束后，可以在 *步骤 1* 重新执行一遍，我们使用 `has_child` 在遍历进程表的 `for` 循环外，来判定是否寻找到子进程。如果寻找到子进程，则进行阻塞，并在阻塞结束后重新进行遍历进程表。如果没找到子进程，则返回 -1。

- 因为要求阻塞结束后，父进程可以通过 `for` 循环来重新遍历进程表，所以我们在外层增加了一个 `while` 循环。虽然是 `while (true)` 类型的无限循环，但实际上最多只会执行两次内部的 `for` 循环。
    - 如果第一次遍历进程表就找到了"僵死"子进程，则通过 `goto` 跳出了 `while` 和 `for` 循环，进入 `rollback` 处理。*只执行了一次 `for` 循环*。
    - 如果第一次遍历进程表找到的子进程并没有处于“僵死”，则进程通过 `for` 循环后的 `if` 语句陷入阻塞，由子进程唤醒后，进程通过 `continue` 来在 `while` 循环内进行第二次 `for` 循环。因为此时子进程已经“僵死”，则会通过 `goto` 来跳出 `while` 循环。*这种情况执行力两次 `for` 循环*。
    - 如果一次遍历进程表并没有找到进程表，则在 `for` 循环后面的 `if` 语句的 `else` 分支通过 `break` 跳出 `while` 循环，最终返回 -1。*只执行了一次 `for` 循环*。

- 遍历进程表时，从 2 号进程开始遍历，理由同 [<071 系统调用 exit - sys_exit>](./071_exit.md#3-sys_exit)。

- 释放 PCB 和内核栈所在页时的函数，需要与创建进程时使用的分配函数相对应。

## 4. sys_exit

根据原理说明，如果父进程因为子进程未处于“僵死”态而陷入阻塞，那么我们需要在子进程的 `sys_exit()` 处增加唤醒父进程的逻辑。

```c
//--> kernel/task.c

void sys_exit(i32 status) {
    ...
    // 如果父进程因为等待该进程而处于 WAITING 态的话，进行唤醒
    task_t *parent = task_queue[current->ppid];
    if (parent->state == TASK_WAITING
        && (parent->waitpid == current->pid || parent->waitpid == -1)
    ) {
        task_unblock(parent);
    }
    ...
    schedule();
}
```

- 仅当当前进程是其父进程所等待的子进程时，才进行唤醒操作。等待子进程 pid 为 -1，表示等待任一子进程终止。


## 5. TASK_WAITING

由于我们约定进程状态 `TASK_WAITING` 来专门表示进程当前正在等待子进程终止（子进程调用 `exit` 就使用了这个约定）。所以我们需要将之前键盘驱动的环形缓冲区的等待逻辑，从 `TASK_WAITING` 转换成 `TASK_BLOCKED`，当然表示的意义仍然是等待键盘输入，只不过因为 `TASK_WAITING` **专门** 表示等待子进程终止。

```c
//--> kernel/keyboard.c

// 从键盘的环形缓冲区读取字符到指定缓冲区，并返回读取字符的个数
size_t keyboard_read(char *buf, size_t count) {
    ...
    while (i < count) {
        while (fifo_empty(&keyboard.fifo)) {
            keyboard.waiter = current_task();
            task_block(keyboard.waiter, NULL, TASK_BLOCKED);    // new
        }
        ...
    }
    ...
}
```

## 6. 功能测试

```c
//--> kernel/thread.c

// 初始化任务 init 的用户态线程
static void user_init_thread() {
    size_t counter = 0;
    i32 status;

    while (true) {
        pid_t pid = fork();

        if (pid == 0) {
            // child process
            printf("fork after child:  fork() = %d, pid = %d, ppid = %d\n", pid, getpid(), getppid());
            // sleep(1000);
            exit(0);
        } else {
            // parent process
            printf("fork after parent: fork() = %d, pid = %d, ppid = %d\n", pid, getpid(), getppid());
            // sleep(1000);
            pid_t childpid = waitpid(pid, &status);
            printf("wait pid = %d, status = %d, counter = %d\n", childpid, status, counter++);
        }

        sleep(2000);
    }
}
```

因为 `waitpid` 系统调用对于父子进程的执行顺序有不同的处理机制，为了能观察到这两种不同的处理流程，我们通过在父子进程处各加入一个 `sleep` 系统调用，来控制父子进程的执行顺序。

这在单处理器上是可行的：
- 如果子进程在 `exit` 前休眠一秒，父进程在 `waitpid` 前不主动休眠，那么会在子进程终止前执行父进程的 `waitpid` 系统调用。
- 如果子进程未休眠一秒，父进程在 `waitpid` 前主动休眠一秒，那么父进程会在子进程终止之后才执行父进程的 `waitpid` 系统调用。

### 6.1 断点观察

在父子进程打印信息处各自打上断点，在 `sys_exit()`，`sys_waitpid()` 处也打上断点。在父子进程不同执行顺序时，使用 ***调试*** 来观察 `sys_exit()` 和 `sys_waitpid()` 的调用顺序，以及 `sys_waitpid()` 中的执行逻辑。 

预期的执行顺序为：
- 子进程 `exit` 在父进程 `waitpid` 之前：
    - 子进程 `sys_exit()` -> 父进程 `sys_waitpid()` -> 父进程直接返回等待的子进程 pid
- 子进程 `exit` 在父进程 `waitpid` 之后：
    - 父进程 `sys_waitpid()` -> 父进程阻塞等待子进程 -> 子进程 `sys_exit()` 并唤醒父进程 -> 父进程 `sys_waitpid()` 返回等待的子进程 pid

### 6.2 LOG 观察

直接运行内核，观察打印出的 LOG 信息。因为父进程调用了 `waitpid` 来等待子进程，并对“僵尸”子进程进行收尸，所以在 `user_init_thread` 进程的 `while` 循环里，可用物理内存并没有减少，进程 PID 也没有减少。

## 7. 扩展

- 实现 `pid_t wait(int *_Nullable wstatus);` 调用。
    > 提示：根据手册说明，调用 `wait(&wstatus)` 等价于调用 `waitpid(-1, &swtatus, 0)`。
