# 系统调用 time

## 1. 原理说明

```c
time_t time(); // 获取当前时间戳 (从 1970-01-01 00:00:00 开始的秒数)
```

节选自 `man 2 waitpid`：

```
DESCRIPTION
       time() returns the time as the number of seconds since the Epoch, 1970-01-01 00:00:00 +0000 (UTC).

RETURN VALUE
       On success, the value of time in seconds since the Epoch is returned.  On error, ((time_t) -1) is returned, and errno is set to indicate the error.
```

> 由于需要获取时间戳，以及进行秒数的转换，可以参考 [<037 时间>](../04_interrupt_and_clock/037_time.md)，以及相关文件 [time.c](/src/kernel/time.c), [time.h](/src/include/xos/time.h)
> 
> 时间片计数器 `jiffies` 和时间片对应秒数的转换可以参考 [<035 计数器与时钟>](../04_interrupt_and_clock/035_counter_and_clock.md)，以及相关文件 [clock.c](/src/kernel/clock.c), [time.h](/src/include/xos/time.h)

系统调用 `time` 的逻辑十分简单，只需将系统启动时的时间戳 `startup_time` 与当前的时间片计数器 `jiffies` 对应的秒数相加即可。

## 2. 系统调用链

```c
//--> include/xos/syscall.h

// 系统调用号
typedef enum syscall_t {
    ...
    SYS_TIME = 13,    // new
} syscall_t;

/***** 声明用户态封装后的系统调用原型 *****/
...
time_t time();


//--> lib/syscall.c

time_t time() {
    return _syscall0(SYS_TIME);
}


//--> kernel/syscall.c

// 初始化系统调用
void syscall_init() {
    ...
    syscall_table[SYS_TIME] = sys_time;
}
```

## 3. sys_time

```c
//--> kernel/clock.c

extern time_t startup_time;

time_t sys_time() {
    return startup_time + (jiffies * jiffy) / 1000;
}
```

`jiffies * jiffy` 为时间片对应的 ms 数，除以 1000 转换成秒数，然后将系统启动时的时间戳 `startup_time` 与 `jiffies` 对应的秒数相加即可。

由于系统启动时间戳 `startup_time` 在 `time_init()` 时进行记录，所以需要在 `main.c` 中加入逻辑：

```c
void kernel_init() {
    ...
    time_init();  // new
    ...
}
```

## 4. 功能测试

在 `thread.c` 中的任务加入类似逻辑测试系统调用 `time`：

```c
printf("time = %d\n", time());
sleep(1000); // sleep 1s
```

预期结果类似于：

```bash
...
time = 1705840597
...
time = 1705840598
...
time = 1705840599
...
```
