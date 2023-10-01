#include <xos/interrupt.h>
#include <xos/io.h>
#include <xos/assert.h>
#include <xos/debug.h>

#define KEYBOARD_DATA_PORT 0x60 // 键盘的数据端口
#define KEYBOARD_CTRL_PORT 0x64 // 键盘的状态/控制端口

// 键盘中断处理函数
void keyboard_handler(int vector) {
    // 键盘中断向量号
    assert(vector == IRQ_KEYBOARD + IRQ_MASTER_NR);

    // 向中断控制器发送中断处理完成的信号
    send_eoi(vector);

    // 从键盘的数据端口读取按键信息的扫描码
    u8 scan_code = inb(KEYBOARD_DATA_PORT);

    LOGK("Keyboard input 0x%x\n", scan_code);
}

// 初始化键盘中断
void keyboard_init() {
    set_interrupt_handler(IRQ_KEYBOARD, keyboard_handler);
    set_interrupt_mask(IRQ_KEYBOARD, true);
}