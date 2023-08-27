#include <xos/bitmap.h>
#include <xos/string.h>
#include <xos/assert.h>
#include <xos/debug.h>

// 构造一个位图
void bitmap_new(bitmap_t *map, u8 *bits, u32 size, u32 offset) {
    map->bits = bits;
    map->size = size;
    map->offset = offset;
    map->length = map->offset + map->size * 8;
}

// 构造一个位图，并将位图的缓冲区全部初始化为零
void bitmap_init(bitmap_t *map, u8 *bits, u32 size, u32 offset) {
    bitmap_new(map, bits, size, offset);
    memset(map->bits, 0, map->size);
}

// 位图是否包含某一位
bool bitmap_contains(bitmap_t *map, u32 index) {
    // 保证 index 在有效区域内
    assert(index >= map->offset && index < map->length);

    // 在缓冲区的相对偏移
    size_t i = index - map->offset;
    // 位图数组中的所在字节
    size_t bytes = i / 8;
    // 所在字节中的所在位
    size_t bits = i % 8;

    return (map->bits[bytes] & (1 << bits));
}

// 设置位图中某一位的值
static void bitmap_set(bitmap_t *map, u32 index, bool value) {
    // 保证 index 在有效区域内
    assert(index >= map->offset && index < map->length);

    // 在缓冲区的相对偏移
    size_t i = index - map->offset;
    // 位图数组中的所在字节
    size_t bytes = i / 8;
    // 所在字节中的所在位
    size_t bits = i % 8;

    if (value) { // 置为 1
        map->bits[bytes] |= (1 << bits);
    } else {     // 置为 0
        map->bits[bytes] &= ~(1 << bits);
    }
}

// 将某一位插入位图
void bitmap_insert(bitmap_t *map, u32 index) {
    bitmap_set(map, index, 1);
}

// 将某一位从位图中删除
void bitmap_remove(bitmap_t *map, u32 index) {
    bitmap_set(map, index, 0);
}

// 往位图中插入连续 n 位的 1。返回满足条件的起始位。
// 如果没有满足条件的空闲空间，返回 EOF。
size_t bitmap_insert_nbits(bitmap_t *map, u32 n) {
    size_t start = EOF;                 // 标记目标开始的起始位置
    size_t bits_left = map->size * 8;   // 剩余未检测的位数
    size_t next_bit = map->offset;      // 下一个检测的位
    size_t counter = 0;                 // 记录当前已有的连续空闲位数

    while (bits_left-- > 0) {
        if (!bitmap_contains(map, next_bit)) {
            // 如果下一位没被占用，则计数器加一
            counter++;
        } else {
            // 否则重置计时器，继续检测
            counter = 0;
        }

        next_bit++;

        // 获得足够的连续空闲位数
        if (counter == n) {
            start = next_bit - counter;
            break;
        }
    }

    // 如果没有足够的连续空闲位数，直接返回 EOF
    if (start == EOF) return EOF;

    // 否则将连续 n 个 1 插入到空闲空间中
    next_bit = start;
    while (counter-- > 0) {
        bitmap_insert(map, next_bit);
        next_bit++;
    }
    return start;
}
