# 087 文件系统状态

本节主要是定义文件访问权限，以及一些相关的宏，还有实现系统调用 `umask`。

## 1. 文件信息

```bash
$ ls -ld bochs/
drwxr-xr-x 4 cc cc 4.0K Feb  3 23:34 bochs/
```

| 条目         | 说明           |
| ------------- | -------------- |
| d             | 表示文件类型   |
| rwx           | 文件所有者权限 |
| r-x           | 组用户权限     |
| r-x           | 其他人权限     |
| 4             | 硬链接数       |
| cc            | 用户名         |
| root          | 组名           |
| 4.0K          | 文件大小       |
| Mar. 17 23:34 | 最后修改时间   |
| bochs         | 文件名         |

## 2. 文件类型

- `-` 表示一般文件
- `d` 表示目录文件
- `l` 表示符号链接，或软连接，用于使用一个不同的文件名来引用另一个文件，符号链接可以跨越文件系统，而链接到另一个文件系统的文件上。删除一个符号链接并不影响被链接的文件。此外还有硬链接，硬链接无法跨越文件系统。链接数表示硬连接的数量。
- `p` 命名管道
- `c` 字符设备
- `b` 块设备
- `s` 套接字

MINIX 的文件信息存储在 `inode.mode` 字段中，总共有 16 位，其中：

- 高 4 位用于表示文件类型
- 中 3 位用于表示特殊标志
- 低 9 位用于表示文件权限

通过 `man inode` 可以查阅 POSIX 规范对于各种文件类型对应的编号：

```bash
The following mask values are defined for the file type:
           S_IFMT     0170000   bit mask for the file type bit field

           S_IFSOCK   0140000   socket
           S_IFLNK    0120000   symbolic link
           S_IFREG    0100000   regular file
           S_IFBLK    0060000   block device
           S_IFDIR    0040000   directory
           S_IFCHR    0020000   character device
           S_IFIFO    0010000   FIFO
```

以及一些与文件读写执行权限相关的规范：

```bash
       The following mask values are defined for the file mode component of the st_mode field:
           S_ISUID     04000   set-user-ID bit (see execve(2))
           S_ISGID     02000   set-group-ID bit (see below)
           S_ISVTX     01000   sticky bit (see below)

           S_IRWXU     00700   owner has read, write, and execute permission
           S_IRUSR     00400   owner has read permission
           S_IWUSR     00200   owner has write permission
           S_IXUSR     00100   owner has execute permission

           S_IRWXG     00070   group has read, write, and execute permission
           S_IRGRP     00040   group has read permission
           S_IWGRP     00020   group has write permission
           S_IXGRP     00010   group has execute permission

           S_IRWXO     00007   others (not in group) have read, write, and execute permission
           S_IROTH     00004   others have read permission
           S_IWOTH     00002   others have write permission
           S_IXOTH     00001   others have execute permission
```

还有一些判断文件类型的宏定义，并给出了一些例子：

```bash
Thus, to test for a regular file (for example), one could write:

           stat(pathname, &sb);
           if ((sb.st_mode & S_IFMT) == S_IFREG) {
               /* Handle regular file */
           }

       Because  tests  of  the  above form are common, additional macros are defined by POSIX to allow the test of the
       file type in st_mode to be written more concisely:

           S_ISREG(m)  is it a regular file?

           S_ISDIR(m)  directory?

           S_ISCHR(m)  character device?

           S_ISBLK(m)  block device?

           S_ISFIFO(m) FIFO (named pipe)?

           S_ISLNK(m)  symbolic link?  (Not in POSIX.1-1996.)

           S_ISSOCK(m) socket?  (Not in POSIX.1-1996.)

       The preceding code snippet could thus be rewritten as:

           stat(pathname, &sb);
           if (S_ISREG(sb.st_mode)) {
               /* Handle regular file */
           }
```

上面提到的 POSIX 规范对于我们后续编写代码非常用帮助（可以直接抄哈哈），毕竟我们也是兼容 POSIX 规范的操作系统 doge

## 3. 代码分析

### 3.1 相关的宏定义

根据之前所提的 POSIX 规范，定义一些宏，由于数量太多，只列举一些代表性的宏：

```c
//--> include/xos/stat.h

// 文件类型
#define IFMT 00170000 // 文件类型（8 进制表示）
...

// 文件访问权限
#define IRWXU 00700 // 所有者可以读、写、执行/搜索
...

// 文件类型判断
#define ISREG(m)    (((m) & IFMT) == IFREG) // 是常规文件
...
```

### 3.2 文件信息

Linux 会提供系统调用 `stat`, `fstat` 使用户获得系统、文件相关的信息，所以我们先定义一个文件信息相关的结构体，它的使用留置后续我们实现 `stat`, `fstat` 系统调用时。

```c
//--> include/xos/stat.h

// 文件状态
typedef struct stat_t {
    devid_t dev;    // 含有文件的设备号
    size_t inode;   // 文件 i 节点号
    u16 mode;       // 文件类型和属性
    u8  nlinks;     // 指定文件的连接数
    u16 uid;        // 文件的用户(标识)号
    u8  gid;        // 文件的组号
    devid_t rdev;   // 设备号(如果文件是特殊的字符文件或块文件)
    size_t size;    // 文件大小（字节数）（如果文件是常规文件）
    time_t atime;   // 上次（最后）访问时间
    time_t mtime;   // 最后修改时间
    time_t ctime;   // 最后节点修改时间
} stat_t;
```

### 3.3 umask

接下来是本节的重头戏：系统调用 `umask`

man 2 umask
> umask() sets the calling process's file mode creation mask (umask) to 
> mask & 0777 (i.e., only the file permission bits of mask are used), and 
> returns the previous value of the mask.

并通过查阅 `sys/stat.h` 头文件以及相关的类型定义，最终发现 Linux 中 `mode_t` 在 64 位系统是 u32 的，在 32 位系统是 u16 的。

```c
//--> include/xos/types.h

// 文件权限
typedef u16 mode_t;
```

### 3.4 进程

接下来给进程控制块补充一些成员：

```c
//--> include/xos/task.h

// 任务控制块 TCB
typedef struct task_t {
    ...
    u32 gid;                    // 用户组 ID
    ...
    u16 umask;                  // 进程用户权限
    ...
} task_t;
```

umask 是用于指示进程用户权限的掩码。对于文件来说，用户权限一般是 755（八进制），即所有者可以读写执行，组成员和其它用户只能读、执行，而 umask 刚好是与权限是按位反过来的，因为它是权限掩码（022 表示遮蔽组成员和其它用户的写权限）。对于进程来说，定义用户权限是合理的，因为进程本质也是通过加载执行可执行文件才成为运行态的进程的，例如 `ls`, `cat` 这类命令对应的进程也是由可执行文件加载执行的，所以在进程控制块里包含一个与其对应的可执行文件的用户权限的成员，是合理的。

```c
// 创建一个默认的任务 TCB
static task_t *task_create(target_t target, const char *name, u32 priority, u32 uid) {
    ...
    task->gid = 0; // TODO: group id
    ...
    task->umask = 0022; // 对应默认的 0755 (八进制)
    ...
}
```

目前在创建任务时将进程的默认用户权限掩码 `umask` 设置成创建的 022（八进制），对应文件常见的用户权限 755（八进制），所有者可读可写可执行，组成员和其它用户可读可执行。后期可以在加载可执行文件时将这个成员设置为对应可执行文件的权限。

### 3.5 系统调用

完成了必要的准备后，只需按流程实习系统调用 `umask` 即可：

```c
//--> include/xos/syscall.h

// 系统调用号
typedef enum syscall_t {
    ...
    SYS_UMASK   = 60,
}

// umask() sets the calling process's file mode creation mask (umask) to 
// mask & 0777 (i.e., only the file permission bits of mask are used), and 
// returns the previous value of the mask.
mode_t  umask(mode_t mask);


//--> lib/syscall.c

mode_t umask(mode_t mask) {
    return _syscall1(SYS_UMASK, (u32)mask);
}


//--> kernel/syscall.c

extern mode_t sys_umask(mode_t mask);

// 初始化系统调用
void syscall_init() {
    ...
    syscall_table[SYS_UMASK]    = sys_umask;
}
```

最后按照 `man 2 umask` 里描述的逻辑实现 `sys_umask` 即可：

> umask() sets the calling process's file mode creation mask (umask) to 
> mask & 0777 (i.e., only the file permission bits of mask are used), and 
> returns the previous value of the mask.

```c
//--> kernel/system.c

mode_t sys_umask(mode_t mask) {
    task_t *current = current_task();
    mode_t old_mask = current->umask;
    current->umask = mask & 0777;
    return old_mask;
}
```

## 4. 功能测试

在测试进程 `test_thread` 上调用 `umask` 系统调用，并通过调试观察来确认其按预期执行：

```c
//--> kernel/thread.c

// 测试任务 test
void test_thread() {
    ...
    mode_t mode = umask(0002);
    ...
}
```

预期为：

- 进程旧的掩码为 022（八进制）
- 进程新的掩码为 002（八进制）

## 5. 参考文献

- [赵炯 / Linux内核完全注释 / 机械工业出版社 / 2005](https://book.douban.com/subject/1231236/)
- <https://www.mkssoftware.com/docs/man1/ls.1.asp>