# Syscall

目前支持的系统调用：

| 系统调用号 | 系统调用原型 | 文档位置 |
| :------: | :---------- | :------ |
| 1 | `void exit(int status);` | [071 系统调用 exit](./071_exit.md) |
| 2 | `pid_t fork();` | [070 系统调用 fork](./070_fork.md) |
| 4 | `i32 write(fd_t fd, const void *buf, size_t len);` | [064 printf](../10_user_programs/064_printf.md) |
| 7 | `pid_t waitpid(pid_t pid, int *status);` | [072 系统调用 waitpid](./072_waitpid.md) |
| 13 | `time_t time();` | [073 系统调用 time](./073_time.md) |
| 20 | `pid_t getpid();` | [069 任务 ID](../07_task_management/069_pid.md) |
| 45 | `i32 brk(void *addr);` | [068 系统调用 brk](./068_brk.md) |
| 64 | `pid_t getppid();` | [069 任务 ID](../07_task_management/069_pid.md) |
| 158 | `void yield();` | [048 系统调用 yield](./048_yield.md) |
| 162 | `void sleep(u32 ms);` | [052 任务睡眠和唤醒](../07_task_management/052_sleep_and_wakeup.md) |
