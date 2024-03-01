# 084 根超级块

类似于进程表，创建 **超级块表**，以及读取磁盘/分区的根超级块，到超级块表的空闲项中，方便内核后续操作。

使用在主机 (host) 上已经创建好的文件系统，假设主机上 Linux 的文件系统是稳定的（这个假设极其合理），方便排错。

```c
// 在超级块表中查找设备号 dev_id 对应的超级块，没有则返回 NULL
superblock_t *get_superblock(devid_t dev_id);

// 读取设备号 dev_id 对应的超级块
superblock_t *read_superblock(devid_t dev_id);
```

## 内存中的结构

- include/fs.h

```c
// 内存中的 inode 格式 (只能用于内存，提供给内核使用)
typedef struct inode_t {
    inode_desc_t *desc;     // inode 描述符
    buffer_t *buf;          // inode 描述符所在 buffer
    devid_t dev_id;         // 设备号
    size_t nr;              // inode 号
    u32 count;              // 引用计数
    time_t atime;           // access time
    time_t ctime;           // create time
    list_node_t node;       // inode 链表节点
    devid_t mount;          // 安装设备
} inode_t;

// 内存中的 superblock 格式 (只能用于内存，提供给内核使用)
typedef struct superblock_t {
    super_desc_t *desc;     // superblock 描述符
    buffer_t *buf;          // superblock 描述符所在 buffer
    buffer_t *imaps[IMAP_MAX_BLOCKS];   // inode 位图对应的 buffer
    buffer_t *zmaps[ZMAP_MAX_BLOCKS];   // 块位图对应的 buffer
    devid_t dev_id;         // 设备号
    list_t inode_list;      // 使用中的 inode 链表
    inode_t *iroot;         // 根目录对应的 inode
    inode_t *imount;        // 挂载文件系统对应的 inode
} superblock_t;
```

这里需要解释一下 `inode_t` 中的 `mount` 字段，这个字段是用于表示：该 inode 索引的是其它设备上挂载的文件系统。这个字段和 `superblock_t` 的 `imount` 字段配合，表示该文件系统上挂载了其它的文件系统。

> 一个文件系统上可以挂载其它的文件系统是很自然的，回想一下你装系统时分区时创建的文件系统，`/` 根目录下还可以挂载其它分区的文件系统，例如 `/home`

## 超级块

### 超级块表

使用与任务列表类似的机制，用于管理内核所需的超级块信息：

- superblock_table
- get_free_superblock
- get_superblock

### 读取超级块

- read_superblock

这里需要解释一下以下代码片段，因为 `superblock_t` 中的 `imaps` 和 `zmaps` 数组的长度是最大允许值，但是文件系统可能并不会使用到这么多块，例如最大值为 8，但是 inode 位图才使用 1 块。这时候需要保证未使用到数组元素为 NULL 防止非法访问数据。

```c
    // 因为可能不需要这么多的位图空间，所以需要将其设置为 0，防止非法访问
    memset(sb->imaps, 0, sizeof(sb->imaps));
    memset(sb->zmaps, 0, sizeof(sb->zmaps));
```

### 挂载根目录

假设主硬盘 (master) 的首个分区 (partition 0) 是根目录对应的文件系统

- mount_root

## 功能测试

使用调试器跟随一遍本节相关操作的流程，确认是否按照预期行为执行。
