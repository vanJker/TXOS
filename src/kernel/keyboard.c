#include <xos/interrupt.h>
#include <xos/io.h>
#include <xos/assert.h>
#include <xos/debug.h>
#include <xos/types.h>
#include <xos/stdlib.h>

static void keyboard_set_leds();

#define KEYBOARD_DATA_PORT 0x60 // 键盘的数据端口
#define KEYBOARD_CTRL_PORT 0x64 // 键盘的状态/控制端口

#define KEYBOARD_CMD_LED    0xED // 设置 LED 状态
#define KEYBOARD_CMD_ACK    0xFA // ACK 上一条命令
#define KEYBOARD_CMD_RESEND 0xFE // 重传上一条命令（一般是上一条命令发生了错误）

#define INV 0        // 不可见字符
#define EXTCODE 0xe0 // 扩展码字节

// 第一套键盘扫描码
typedef enum key_t {
    KEY_ESC             = 0x01,
    KEY_1               = 0x02,
    KEY_2               = 0x03,
    KEY_3               = 0x04,
    KEY_4               = 0x05,
    KEY_5               = 0x06,
    KEY_6               = 0x07,
    KEY_7               = 0x08,
    KEY_8               = 0x09,
    KEY_9               = 0x0a,
    KEY_0               = 0x0b,
    KEY_MINUS           = 0x0c,
    KEY_EQUAL           = 0x0d,
    KEY_BACKSPACE       = 0x0e,
    KEY_TAB             = 0x0f,
    KEY_Q               = 0x10,
    KEY_W               = 0x11,
    KEY_E               = 0x12,
    KEY_R               = 0x13,
    KEY_T               = 0x14,
    KEY_Y               = 0x15,
    KEY_U               = 0x16,
    KEY_I               = 0x17,
    KEY_O               = 0x18,
    KEY_P               = 0x19,
    KEY_BRACKET_L       = 0x1a,
    KEY_BRACKET_R       = 0x1b,
    KEY_ENTER           = 0x1c,
    KEY_CTRL_L          = 0x1d,
    KEY_A               = 0x1e,
    KEY_S               = 0x1f,
    KEY_D               = 0x20,
    KEY_F               = 0x21,
    KEY_G               = 0x22,
    KEY_H               = 0x23,
    KEY_J               = 0x24,
    KEY_K               = 0x25,
    KEY_L               = 0x26,
    KEY_SEMICOLON       = 0x27,
    KEY_QUOTE           = 0x28,
    KEY_BACKQUOTE       = 0x29,
    KEY_SHIFT_L         = 0x2a,
    KEY_BACKSLASH       = 0x2b,
    KEY_Z               = 0x2c,
    KEY_X               = 0x2d,
    KEY_C               = 0x2e,
    KEY_V               = 0x2f,
    KEY_B               = 0x30,
    KEY_N               = 0x31,
    KEY_M               = 0x32,
    KEY_COMMA           = 0x33,
    KEY_POINT           = 0x34,
    KEY_SLASH           = 0x35,
    KEY_SHIFT_R         = 0x36,
    KEY_STAR            = 0x37,
    KEY_ALT_L           = 0x38,
    KEY_SPACE           = 0x39,
    KEY_CAPSLOCK        = 0x3a,
    KEY_F1              = 0x3b,
    KEY_F2              = 0x3c,
    KEY_F3              = 0x3d,
    KEY_F4              = 0x3e,
    KEY_F5              = 0x3f,
    KEY_F6              = 0x40,
    KEY_F7              = 0x41,
    KEY_F8              = 0x42,
    KEY_F9              = 0x43,
    KEY_F10             = 0x44,
    KEY_NUMLOCK         = 0x45,
    KEY_SCRLOCK         = 0x46,
    KEY_PAD_7           = 0x47,
    KEY_PAD_8           = 0x48,
    KEY_PAD_9           = 0x49,
    KEY_PAD_MINUS       = 0x4a,
    KEY_PAD_4           = 0x4b,
    KEY_PAD_5           = 0x4c,
    KEY_PAD_6           = 0x4d,
    KEY_PAD_PLUS        = 0x4e,
    KEY_PAD_1           = 0x4f,
    KEY_PAD_2           = 0x50,
    KEY_PAD_3           = 0x51,
    KEY_PAD_0           = 0x52,
    KEY_PAD_POINT       = 0x53,
    KEY_F11             = 0x57,
    KEY_F12             = 0x58,
    KEY_WIN_L           = 0x5b,
    KEY_WIN_R           = 0x5c,
    KEY_CLIPBOARD       = 0x5d,
    KEY_PRINT_SCREEN    = 0xb7,
} key_t;

// 按键的字符及相关状态
typedef struct ket_state_t {
    char keycap[2];     // [0] 单独按键的字符，  [1] 与 shift 组合按键的字符
    bool key_state[2];  // [0] 是否按下该扫描码，[2] 是否按下该扫描码的扩展码
} key_state_t;

static key_state_t keymap[] = {
    /* 扫描码 = { 单独按键的字符 | 与 shift 组合按键的字符 | 是否按下该扫描码 | 是否按下该扫描码的扩展码 } */
    [KEY_ESC]           = {{0x1b, 0x1b}, {false, false}},
    [KEY_1]             = {{'1', '!'}, {false, false}},
    [KEY_2]             = {{'2', '@'}, {false, false}},
    [KEY_3]             = {{'3', '#'}, {false, false}},
    [KEY_4]             = {{'4', '$'}, {false, false}},
    [KEY_5]             = {{'5', '%'}, {false, false}},
    [KEY_6]             = {{'6', '^'}, {false, false}},
    [KEY_7]             = {{'7', '&'}, {false, false}},
    [KEY_8]             = {{'8', '*'}, {false, false}},
    [KEY_9]             = {{'9', '('}, {false, false}},
    [KEY_0]             = {{'0', ')'}, {false, false}},
    [KEY_MINUS]         = {{'-', '_'}, {false, false}},
    [KEY_EQUAL]         = {{'=', '+'}, {false, false}},
    [KEY_BACKSPACE]     = {{'\b', '\b'}, {false, false}},
    [KEY_TAB]           = {{'\t', '\t'}, {false, false}},
    [KEY_Q]             = {{'q', 'Q'}, {false, false}},
    [KEY_W]             = {{'w', 'W'}, {false, false}},
    [KEY_E]             = {{'e', 'E'}, {false, false}},
    [KEY_R]             = {{'r', 'R'}, {false, false}},
    [KEY_T]             = {{'t', 'T'}, {false, false}},
    [KEY_Y]             = {{'y', 'Y'}, {false, false}},
    [KEY_U]             = {{'u', 'U'}, {false, false}},
    [KEY_I]             = {{'i', 'I'}, {false, false}},
    [KEY_Q]             = {{'q', 'Q'}, {false, false}},
    [KEY_P]             = {{'p', 'P'}, {false, false}},
    [KEY_BRACKET_L]     = {{'[', '{'}, {false, false}},
    [KEY_BRACKET_R]     = {{']', '}'}, {false, false}},
    [KEY_ENTER]         = {{'\n', '\n'}, {false, false}},
    [KEY_CTRL_L]        = {{INV, INV}, {false, false}},
    [KEY_A]             = {{'a', 'A'}, {false, false}},
    [KEY_S]             = {{'s', 'S'}, {false, false}},
    [KEY_D]             = {{'d', 'D'}, {false, false}},
    [KEY_F]             = {{'f', 'F'}, {false, false}},
    [KEY_G]             = {{'g', 'G'}, {false, false}},
    [KEY_H]             = {{'h', 'H'}, {false, false}},
    [KEY_J]             = {{'j', 'J'}, {false, false}},
    [KEY_K]             = {{'k', 'K'}, {false, false}},
    [KEY_L]             = {{'l', 'L'}, {false, false}},
    [KEY_SEMICOLON]     = {{';', ':'}, {false, false}},
    [KEY_QUOTE]         = {{'\'', '\"'}, {false, false}},
    [KEY_BACKQUOTE]     = {{'`', '~'}, {false, false}},
    [KEY_SHIFT_L]       = {{INV, INV}, {false, false}},
    [KEY_BACKSLASH]     = {{'\\', '|'}, {false, false}},
    [KEY_Z]             = {{'z', 'Z'}, {false, false}},
    [KEY_X]             = {{'x', 'X'}, {false, false}},
    [KEY_C]             = {{'c', 'C'}, {false, false}},
    [KEY_V]             = {{'v', 'V'}, {false, false}},
    [KEY_B]             = {{'b', 'B'}, {false, false}},
    [KEY_N]             = {{'n', 'N'}, {false, false}},
    [KEY_M]             = {{'m', 'M'}, {false, false}},
    [KEY_COMMA]         = {{',', '<'}, {false, false}},
    [KEY_POINT]         = {{'.', '>'}, {false, false}},
    [KEY_SLASH]         = {{'/', '?'}, {false, false}},
    [KEY_SHIFT_R]       = {{INV, INV}, {false, false}},
    [KEY_STAR]          = {{'*', '*'}, {false, false}},
    [KEY_ALT_L]         = {{INV, INV}, {false, false}},
    [KEY_SPACE]         = {{' ', ' '}, {false, false}},
    [KEY_CAPSLOCK]      = {{INV, INV}, {false, false}},
    [KEY_F1]            = {{INV, INV}, {false, false}},
    [KEY_F2]            = {{INV, INV}, {false, false}},
    [KEY_F3]            = {{INV, INV}, {false, false}},
    [KEY_F4]            = {{INV, INV}, {false, false}},
    [KEY_F5]            = {{INV, INV}, {false, false}},
    [KEY_F6]            = {{INV, INV}, {false, false}},
    [KEY_F7]            = {{INV, INV}, {false, false}},
    [KEY_F8]            = {{INV, INV}, {false, false}},
    [KEY_F9]            = {{INV, INV}, {false, false}},
    [KEY_F10]           = {{INV, INV}, {false, false}},
    [KEY_NUMLOCK]       = {{INV, INV}, {false, false}},
    [KEY_SCRLOCK]       = {{INV, INV}, {false, false}},
    [KEY_PAD_7]         = {{'7', INV}, {false, false}},
    [KEY_PAD_8]         = {{'8', INV}, {false, false}},
    [KEY_PAD_9]         = {{'9', INV}, {false, false}},
    [KEY_PAD_MINUS]     = {{'-', '-'}, {false, false}},
    [KEY_PAD_4]         = {{'4', INV}, {false, false}},
    [KEY_PAD_5]         = {{'5', INV}, {false, false}},
    [KEY_PAD_6]         = {{'6', INV}, {false, false}},
    [KEY_PAD_PLUS]      = {{'+', '+'}, {false, false}},
    [KEY_PAD_1]         = {{'1', INV}, {false, false}},
    [KEY_PAD_2]         = {{'2', INV}, {false, false}},
    [KEY_PAD_3]         = {{'3', INV}, {false, false}},
    [KEY_PAD_0]         = {{'0', INV}, {false, false}},
    [KEY_PAD_POINT]     = {{'.', 0x7f}, {false, false}},
    [KEY_F11]           = {{INV, INV}, {false, false}},
    [KEY_F12]           = {{INV, INV}, {false, false}},
    [KEY_WIN_L]         = {{INV, INV}, {false, false}},
    [KEY_WIN_R]         = {{INV, INV}, {false, false}},
    [KEY_CLIPBOARD]     = {{INV, INV}, {false, false}},
    [KEY_PRINT_SCREEN]  = {{INV, INV}, {false, false}},
};

// 键盘管理器
typedef struct keyboard_t {
    bool capslock;  // 大写锁定
    bool scrlock;   // 滚动锁定
    bool numlock;   // 数字锁定
    bool extcode;   // 扩展码状态
    bool ctrl;      // Ctrl 键状态
    bool alt;       // Alt 键状态
    bool shift;     // Shift 键状态
} keyboard_t;
// 键盘管理器
static keyboard_t keyboard;

// 初始化键盘管理器
void keyboard_new(keyboard_t *keyboard) {
    keyboard->capslock = false;
    keyboard->scrlock  = false;
    keyboard->numlock  = false;
    keyboard->extcode  = false;
    keyboard->ctrl     = false;
    keyboard->alt      = false;
    keyboard->shift    = false;
    keyboard_set_leds();
}

// 等待键盘的输出缓冲区为满
static void keyboard_output_wait() {
    u8 state;
    do {
        state = inb(KEYBOARD_CTRL_PORT);
    } while (!(state & 0x01));
}

// 等待键盘的输入缓冲区为空
static void keyboard_input_wait() {
    u8 state;
    do {
        state = inb(KEYBOARD_CTRL_PORT);
    } while (state & 0x02);
}

// 等待直到键盘返回对上一条命令的处理结果
static u8 keyboard_cmd_respond() {
    keyboard_output_wait();
    return inb(KEYBOARD_DATA_PORT);
}

// 设置键盘的 LED 灯
static void keyboard_set_leds() {
    u8 leds = (keyboard.capslock << 2) | (keyboard.numlock << 1) | keyboard.scrlock;
    u8 state;
    
    // 设置 LED 命令
    do {
        keyboard_input_wait();
        outb(KEYBOARD_DATA_PORT, KEYBOARD_CMD_LED);
        state = keyboard_cmd_respond();
    } while (state == KEYBOARD_CMD_RESEND);
    assert(state == KEYBOARD_CMD_ACK); // 保证命令被 ACK

    // 设置 LED 状态
    do {
        keyboard_input_wait();
        outb(KEYBOARD_DATA_PORT, leds);
        state = keyboard_cmd_respond();
    } while (state == KEYBOARD_CMD_RESEND);
    keyboard_input_wait(); // 保证数据被成功输入
}

// 键盘中断处理函数
void keyboard_handler(int vector) {
    // 键盘中断向量号
    assert(vector == IRQ_KEYBOARD + IRQ_MASTER_NR);

    // 向中断控制器发送中断处理完成的信号
    send_eoi(vector);

    // 从键盘的数据端口读取按键信息的扫描码
    keyboard_output_wait();
    u16 scan_code = inb(KEYBOARD_DATA_PORT);
    size_t ext_state = 0; // 按键的状态索引，默认不是扩展码

    // 如果接收的是扩展码字节，则设置扩展码状态
    if (scan_code == EXTCODE) {
        keyboard.extcode = true;
        return;
    }

    // 如果是扩展码
    if (keyboard.extcode) {
        ext_state = 1;              // 修改按键的状态索引
        scan_code |= 0xe0000;       // 修改扫描码，增加 0xe0 前缀
        keyboard.extcode = false;   // 重置扩展码状态
    }

    // 获取通码
    u16 make_code = (scan_code & 0x7f);

    // 如果通码非法
    if (make_code != KEY_PRINT_SCREEN && make_code > KEY_CLIPBOARD) {
        return;
    }

    // 获取断码状态
    bool break_code = ((scan_code & 0x0080) != 0);
    if (break_code) {
        // 如果是断码，则按键状态为抬起，并返回
        keymap[make_code].key_state[ext_state] = false;
        return;
    } else {
        // 如果是通码，则按键状态为按下
        keymap[make_code].key_state[ext_state] = true;
    }

    // 是否需要设置 LED 灯
    bool led = false;
    if (make_code == KEY_NUMLOCK) {
        keyboard.numlock = !(keyboard.numlock);
        led = true;
    } else if (make_code == KEY_CAPSLOCK) {
        keyboard.capslock = !(keyboard.capslock);
        led = true;
    } else if (make_code == KEY_SCRLOCK) {
        keyboard.scrlock = !(keyboard.scrlock);
        led = true;
    }

    // 至少一个 LED 灯状态发送变化
    if (led) keyboard_set_leds();

    // 设置 Ctrl, Alt, Shift 按键状态
    keyboard.ctrl  = keymap[KEY_CTRL_L].key_state[0]  || keymap[KEY_CTRL_L].key_state[1];
    keyboard.alt   = keymap[KEY_ALT_L].key_state[0]   || keymap[KEY_ALT_L].key_state[1];
    keyboard.shift = keymap[KEY_SHIFT_L].key_state[0] || keymap[KEY_SHIFT_R].key_state[0];

    // 计算 Shift 状态
    bool shift = false;
    if (keyboard.capslock && isAlpha(keymap[make_code].keycap[0])) {
        // Capslock 锁定只对字母按键有效，对于数字按键无效
        shift = !shift;
    }
    if (keyboard.shift) {
        shift = !shift;
    }

    // 获取按键对应的 ASCII 码
    char ch;
    // [/?] 这个键比较特殊，只有这个键的扩展码和普通码一样，会显示字符。其它键的扩展码都是不可见字符，比如 KEY_PAD-1
    if (ext_state && (make_code != KEY_SLASH)) {
        ch = keymap[make_code].keycap[ext_state];
    } else {
        ch = keymap[make_code].keycap[shift];
    }

    // 如果是不可见字符，则直接返回
    if (ch == INV) return;

    // 否则的话，就打印按键组合对应的字符
    LOGK("press key %c\n", ch);

    return;
}

// 初始化键盘中断
void keyboard_init() {
    keyboard_new(&keyboard);

    set_interrupt_handler(IRQ_KEYBOARD, keyboard_handler);
    set_interrupt_mask(IRQ_KEYBOARD, true);
}