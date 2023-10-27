#include <xos/global.h>
#include <xos/string.h>
#include <xos/debug.h>

descriptor_t gdt[GDT_SIZE]; // 内核全局描述符表
pointer_t gdt_ptr;          // 内核全局描述符表指针
tss_t tss;                  // 任务状态段

// 使用基址和界限来初始化描述符
static void descriptor_init(descriptor_t *desc, u32 base, u32 limit) {
    desc->base_low   = base & 0xffffff;
    desc->base_high  = (base >> 24) & 0xff;
    desc->limit_low  = limit & 0xffff;
    desc->limit_high = (limit >> 16) & 0xf;
}

// 初始化用户相关的段描述符
static void user_descriptors_init() {
    descriptor_t *desc;

    // 初始化用户代码段描述符
    desc = gdt + USER_CODE_IDX;
    descriptor_init(desc, 0, 0xFFFFF);
    desc->segment = 1;      // 代码段
    desc->granularity = 1;  // 粒度 4K
    desc->big = 1;          // 32 位
    desc->long_mode = 0;    // 不是 64 位
    desc->present = 1;      // 位于内存
    desc->DPL = 3;          // 用户权级
    desc->type = 0b1010;    // 代码段 | 非依从 | 可读 | 没有被访问过

    // 初始化用户数据段描述符
    desc = gdt + USER_DATA_IDX;
    descriptor_init(desc, 0, 0xFFFFF);
    desc->segment = 1;      // 数据段
    desc->granularity = 1;  // 粒度 4K
    desc->big = 1;          // 32 位
    desc->long_mode = 0;    // 不是 64 位
    desc->present = 1;      // 位于内存
    desc->DPL = 3;          // 用户权级
    desc->type = 0b0010;    // 数据段 | 向上扩展 | 可写 | 没有被访问过
} 

// 初始化内核全局描述符表以及指针
void gdt_init() {
    LOGK("init GDT!!!\n");

    asm volatile("sgdt gdt_ptr");

    // 通过已有的描述符表来初始化 GDT 的内核代码段和数据段描述符
    memcpy((void *)&gdt, (void *)gdt_ptr.base, gdt_ptr.limit + 1);

    // 初始化用户代码段和数据段描述符
    user_descriptors_init();

    gdt_ptr.base = (u32)gdt;
    gdt_ptr.limit = sizeof(gdt) - 1;

    asm volatile("lgdt gdt_ptr");
}

// 初始化任务状态段（TSS）及其描述符
void tss_init() {
    // 清空 TSS
    memset(&tss, 0, sizeof(tss));

    // 初始化 TSS
    tss.ss0 = KERNEL_DATA_SELECTOR;
    tss.iobase = sizeof(tss);

    // 初始化 TSS 描述符
    descriptor_t *desc = gdt + KERNEL_TSS_IDX;
    descriptor_init(desc, (u32)&tss, sizeof(tss) - 1);
    desc->segment = 0;      // 系统段
    desc->granularity = 0;  // 粒度 1 Byte
    desc->big = 0;          // 固定为 0
    desc->long_mode = 0;    // 固定为 0
    desc->present = 1;      // 位于内存
    desc->DPL = 0;          // 内核权级（用于任务门或调用门）
    desc->type = 0b1001;    // 32 位可用 TSS

    asm volatile("ltr %%ax\n"::"a"(KERNEL_TSS_SELECTOR));
}