/* vsprintf.c -- Lars Wirzenius & Linus Torvalds. */
/*
 * Wirzenius wrote this portably, Torvalds fucked it up :-)
 */

#include <xos/stdio.h>
#include <xos/string.h>

#define ZEROPAD 1   // 填充 0
#define SIGN    2   // unsigned/signed long
#define PLUS    4   // 显示 +
#define SPACE   8   // 如果是加，则置空格
#define LEFT    16  // 左对齐
#define SPECIAL 32  // 八进制左补 0，十六进制左补 0x
#define SMALL   64  // 使用小写字母输出十六进制数

#define is_digit(c) ((c) >= '0' && (c) <= '9')

// 将数字的字符串转换成整数，并将指针前移
static int skip_atoi(const char **s) {
    int i = 0;
    while (is_digit(**s)) {
        i = i * 10 + **s - '0';
        *s++;
    }
    return i;
}

// 将整数转换为指定进制的字符串
// str - 输出字符串指针
// num - 整数
// base - 进制基数
// size - 字符串长度
// precision - 数字长度(精度)
// flags - 选项
static char *number(char *str, unsigned long num, int base, int size, int precision, int flags) {
    char c, sign, tmp[36];
    const char *digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    int i;
    int index;
    char *ptr = str;

    // 如果 flags 指出用小写字母，则定义小写字母集
    if (flags & SMALL)
        digits = "0123456789abcdefghijklmnopqrstuvwxyz";

    // 如果 flags 指出要左对齐，则屏蔽 flags 中的填零标志
    if (flags & LEFT)
        flags & ~ZEROPAD;
    
    // 如果进制基数小于 2 或大于 36，则退出处理
    // 即本函数只能处理基数在 2-36 之间的数
    if (base < 2 || base > 36)
        return 0;
    
    // 如果 flags 指出要填零，则置字符变量 c='0'，否则 c 为空格字符
    c = (flags & ZEROPAD) ? '0' : ' ';

    // 如果 flags 指出带符号数并且数值 num 小于 0，则置符号变量 sign = '-'，并将 num 取绝对值
    if (flags & SIGN && num < 0) {
        sign = '-';
        num = -num;
    } else 
        // 否则如果 flags 指出是加号，则置 sign = '+'，否则如果 flags 带空格标志，则置 sign = ' '，否则 sign = 0
        sign = (flags & PLUS) ? '+' : (flags & SPACE) ? ' ' : 0;

    // 如果带符号，宽读值减 1
    if (sign)
        size--;
    
    // 如果 flags 指出是特殊符号，则对于十六进制，宽度减 2（用于前置的 0x）
    if (flags & SPECIAL) {
        if (base == 16)
            size -= 2;
        // 对于八进制，宽度减 1（用于前置的 0）
        else if (base == 8)
            size--;
    }
    
    i = 0;
    // 如果数值 num = 0，则临时字符串 tmp = "0"；否则根据给定的基数将数值 num 转换成字符形式
    if (num == 0)
        tmp[i++] = '0';
    else
        while (num != 0) {
            index = num % base;
            num /= base;
            tmp[i++] = digits[index];
        }
    
    // 如果数值字符个数大于精度值，则将精度值扩展为数值字符个数
    if (i > precision)
        precision = i;
    
    // 宽度值 size 减去用于存放数值字符的个数
    size -= precision;

    // 从这里真正开始形成所需要的转换结果，并暂时放在字符串 str 中

    // 如果 flags 中没有填零（ZEROPAD）和左对齐（LEFT）标志
    // 则在 str 中首先填放剩余宽度指出的空格数
    if (!(flags & (ZEROPAD | LEFT)))
        while (size--)
            *str++ = ' ';
    
    // 如果带符号位，则存入符号
    if (sign)
        *str++ = sign;
    
    // 如果 flags 指出是特殊转换
    if (flags & SPECIAL) {
        // 对于八进制置 '0'
        if (base == 8)
            *str++ = '0';
        // 对于十六进制置 '0x'
        else if (base == 16) {
            *str++ = '0';
            *str++ = digits[33]; // x or X
        }
    }

    // 如果 flags 中没有左对齐（LEFT）标志，则在剩余宽度中存放 c 字符（'0' 或空格）
    if (!(flags & LEFT))
        while (size--)
            *str++ = c;
    
    // 此时 i 存有数值 num 的数字位数

    // 如果 num 的位数小于精度值，则在 str 中放入（精度值-i）个 ‘0’
    while (i < precision)
        *str++ = '0';
    
    // 将转换好的数值字符串填入 str 中，长度为 i
    while (i--)
        *str++ = tmp[i];
    
    // 如果宽度值仍然大于 0
    // 则表示 flags 中有左对齐（LEFT）标志
    // 则在剩余宽度中放入空格
    while (size--)
        *str++ = ' ';
    
    return str;
}