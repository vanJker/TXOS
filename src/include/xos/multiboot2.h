#ifndef XOS_MULTIBOOT2
#define XOS_MULTIBOOT2

#include <xos/types.h>

// 魔数，multiboot2 引导时存放在 EAX 
#define MULTIBOOT2_MAGIC 0x36d76289

// multiboot2 tag 类型
#define MULTIBOOT2_TAG_TYPE_END 0
#define MULTIBOOT2_TAG_TYPE_MAP 6

// multiboot2 memory-map 的类型
#define MULTIBOOT2_MEMORY_AVAILABLE 1
#define MULTIBOOT2_MEMORY_RESERVED  2
#define MULTIBOOT2_MEMORY_ACPI_RECLAIMABLE 3
#define MULTIBOOT2_MEMORY_NVS       4
#define MULTIBOOT2_MEMORY_BADRAM    5

// multiboot2 tag
typedef struct multiboot2_tag_t {
    u32 type;   // tag 的类型
    u32 size;   // tag 的大小
} multiboot2_tag_t;

// multiboot2 memory-map entry
typedef struct multiboot2_mmap_entry_t {
    u64 addr;   // 地址区域的起始地址
    u64 len;    // 地址区域的长度
    u32 type;   // 地址区域的类型
    u32 zero;   // 保留（为 0）
} multiboot2_mmap_entry_t;

// multiboot2 memory-map tag
typedef struct multiboot2_tag_mmap_t {
    u32 type;           // tag 的类型（为 6）
    u32 size;           // tag 的大小
    u32 entry_size;     // entry 的大小
    u32 entry_version;  // entry 的版本（目前为 0）
    multiboot2_mmap_entry_t entries[0];
} multiboot2_tag_mmap_t;


#endif