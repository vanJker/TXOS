# 043 数据结构 - 位图

## 1. 原理说明

为了利用内存，我们使用位图这样的数据结构来标记一些二值（0/1）的信息。

我们的位图设计和实现尽可能简单，避免复杂的计算，当然性能不会特别出色。

位图的结构示意图如下：

![](./images/bitmap.drawio.svg)

依据示意图，在 `include/xos/bitmap.h` 中声明位图的数据类型，以及相关的位图操作。

```c
typedef struct bitmap_t {
    u8 *bits;   // 位图缓冲区
    u32 size;   // 位图缓冲区长度（以字节为单位）
    u32 offset; // 位图开始的偏移（以比特为单位）
    u32 length; // 位图的长度
} bitmap_t;

// 构造一个位图
void bitmap_new(bitmap_t *map, u8 *bits, u32 size, u32 offset);

// 构造一个位图，并将位图的缓冲区全部初始化为零
void bitmap_init(bitmap_t *map, u8 *bits, u32 size, u32 offset);

// 位图是否包含某一位
bool bitmap_contains(bitmap_t *map, u32 index);

// 将某一位插入位图
void bitmap_insert(bitmap_t *map, u32 index);

// 将某一位从位图中删除
void bitmap_remove(bitmap_t *map, u32 index);

// 往位图中插入连续 n 位的 1。返回满足条件的起始位。
// 如果没有满足条件的空闲空间，返回 EOF。
size_t bitmap_insert_nbits(bitmap_t *map, u32 n);
```

## 2. 代码分析

> 以下代码均位于 `lib/bitmap.c`。由于本节代码比较简单，请对照源码阅读。

- 构造位图相关的操作为：`bitmap_new()` 和 `bitmap_init()`，其实现都十分简单。

- `bitmap_contains()` 检测位图中是否包含某一位，这个实现涉及了一些位操作，以及位图缓冲区和位图下标的关系。

- 往位图中插入 / 删除某一位，先使用与检测位图中是否包含某一位的机制，实现设置位图中某一位的值 `bitmap_set()`。

- 接下来封装 `bitmap_set()` 即可实现位图的插入 / 删除。

- `bitmap_insert_nbits()` 在位图中寻找连续 n 位为 0 的空闲空间，如果有，往其中插入连续的 1，并返回该空间的起始位置。

### 3. 功能测试

```c
/* kernel/main.c */
void kernel_init() {
    ...
    // 测试位图功能
    bitmap_test();
    ...
}

/* lib/bitmap.c */
void bitmap_test() {
    const size_t LEN = 2;
    u8 buf[LEN];
    bitmap_t map;

    bitmap_init(&map, buf, LEN, 0);
    for (size_t i = 0; i < 33; i++) {
        size_t k = bitmap_insert_nbits(&map, 1);
        if (k == EOF) {
            LOGK("TEST FINISH\n");
            break;
        }
        LOGK("%d\n", k);
    }
}
```

由于 `buf` 由 2 个字节组成，即 16 个 bit。所以预期为，依次输出 0 ~ 15，最后打印 `TEST FINISH`。