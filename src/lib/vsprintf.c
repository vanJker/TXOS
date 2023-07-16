/* vsprintf.c -- Lars Wirzenius & Linus Torvalds. */
/*
 * Wirzenius wrote this portably, Torvalds fucked it up :-)
 */

#include <xos/stdio.h>
#include <xos/string.h>

#define ZEROPAD (1 << 0)    // 填充 0
#define SIGN    (1 << 1)    // unsigned/signed long
#define PLUS    (1 << 2)    // 显示 +
#define SPACE   (1 << 3)    // 如果是加，则置空格
#define LEFT    (1 << 3)    // 左对齐
#define SPECIAL (1 << 4)    // 八进制左补 0，十六进制左补 0x
#define SMALL   (1 << 5)    // 使用小写字母输出十六进制数

#define is_digit(c) ((c) >= '0' && (c) <= '9')

// 将数字的字符串转换成整数，并将指针前移
static int skip_atoi(const char **s) {
    int i = 0;
    while (is_digit(**s)) {
        i = i * 10 + **s - '0';
        (*s)++;
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
        while (size-- > 0)
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
        while (size-- > 0)
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
    while (size-- > 0)
        *str++ = ' ';
    
    return str;
}

int vsprintf(char *buf, const char *fmt, va_list args) {
    int len;
    int i;

    // 用于存放转换过程中的字符串
    char *str;
    char *s;
    char *ip;

    // number() 函数需要使用的标志
    int flags;

    int field_width; // 输出字段宽度
    int precision;   // min：整数数字个数；max：字符串中字符个数
    int qualifier;   // 'h', 'l', 'L' 用于整数字段

    // 首先将字符指针指向 buf
    // 然后扫描格式字符串
    // 对各个转换指示进行相应的处理
    for (str = buf; *fmt; ++fmt) {
        // 格式转换指示字符串均以 '%' 开始
        // 这里从 fmt 格式字符串中扫描 ’%‘，寻找格式转换字符串的开始
        // 不是格式指示的一般字符均被依次存入 str
        if (*fmt != '%') {
            *str++ = *fmt;
            continue;
        }

        // 下面取得格式指示字符串中的标志域，并将标志常量放入 flags 变量中
        flags = 0;

    repeat:
        // 过掉第一个字符 ’%‘
        ++fmt;
        switch (*fmt) {
        // 左对齐调整
        case '-':
            flags |= LEFT;
            goto repeat;
        // 置加号
        case '+':
            flags |= PLUS;
            goto repeat;
        // 置空格
        case ' ':
            flags |= SPACE;
            goto repeat;
        // 是特殊转换
        case '#':
            flags |= SPECIAL;
            goto repeat;
        // 要填零（即 '0'），否则填空格
        case '0':
            flags |= ZEROPAD;
            goto repeat;
        }

        // 取当前参数字段宽度域值，放入 field_wodth 变量中
        field_width = -1;

        // 如果当前参数字段宽度中是数值，则直接取其为宽度值
        if (is_digit(*fmt))
            field_width = skip_atoi(&fmt);
        
        // 如果宽度域中是字符 '*'，表示由下一个参数指定宽度
        else if (*fmt == '*') {
            ++fmt;
            // 调用 va_arg 取得下一个参数，即指定的宽度值
            field_width = va_arg(args, int);

            // 如果此时宽度值小于 0，则该负数表示其带有标志域 '-' 标志（需要进行左对齐）
            if (field_width < 0) {
                // 因此还需要在标志域中添入该标志，并将字段宽度取为其绝对值
                field_width = -field_width;
                flags |= LEFT;
            }
        }

        // 取格式转换串的精度域，并存入 precision 变量中
        precision = -1;

        // 精度域开始的标志是 '-'，其处理过程与上面的宽度域类似
        if (*fmt == '.') {
            ++fmt;
            // 如果精度域中是数值，则直接取为精度值
            if (is_digit(*fmt))
                precision = skip_atoi(&fmt);

            // 如果精度域中是字符 '*'，表示由下一个参数指定精度
            else if (*fmt == '*') {
                ++fmt;
                // 调用 va_arg 取得下一个参数，即指定的精度值
                precision = va_arg(args, int);
            }

            // 如果此时精度值小于 0，则将字段精度值取为 0
            if (precision < 0)
                precision = 0;
        }

        // 下面这段代码分析长度修饰符，并将其存入 qualifer 变量中
        qualifier = -1;
        if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L') {
            qualifier = *fmt;
            ++fmt;
        }

        // 下面分析转换指示符
        switch (*fmt) {
        // 如果转换指示符是 'c'，则表示对应参数应是字符
        case 'c':
            // 此时如果标志域表明不是左对齐
            if (!(flags & LEFT))
                // 则该字段前面放入（宽度域值 - 1）个空格字符，然后再放入参数字符
                while (--field_width)
                    *str++ = ' ';
            
            // 然后放入参数字符
            *str++ = (unsigned char)va_arg(args, int);

            // 如果宽度域还大于 0，则表示有左对齐标志
            // 则在参数字符后面添加（宽度域值 - 1）个空格字符
            while (--field_width)
                *str++ = ' ';

            break;

        // 如果转换指示符是 's'，则表示对应参数是字符串
        case 's':
            s = va_arg(args, char *);
            // 首先取参数字符串的长度
            len = strlen(s);
            // 如果其超过了精度域值，则将字符串长度等于精度域值
            if (precision < 0)
                precision = len;
            else
                len = precision;
            
            // 此时如果标志域表明不是左对齐
            if (!(flags & LEFT))
                // 则该字段前放入（宽度值-字符串长度）个空格字符
                while (len < field_width--)
                    *str++ = ' ';

            // 然后放入参数字符串
            for (i = 0; i < len; i++)
                *str++ = *s++;

            // 如果宽度域还大于 0，则表示有左对齐标志
            // 则在参数字符后面添加（宽度域值 - 字符串长度）个空格字符
            while (len < field_width--)
                *str++ = ' ';
            
            break;
        
        // 如果格式转换符是 'o'，表示需将对应的参数转换成八进制数的字符串
        case 'o':
            str = number(str, va_arg(args, unsigned long), 8,
                         field_width, precision, flags);
            break;

        // 如果格式转换符是 'p'，表示需要将对应的参数是一个指针类型
        case 'p':
            // 此时如果该参数没有设置宽度值，则默认宽度为 8，并且需要填零
            if (field_width == -1) {
                field_width = 8;
                flags |= ZEROPAD;
            }
            str = number(str, (unsigned long)va_arg(args, void *), 16,
                         field_width, precision, flags);
            break;
        
        // 如果格式转换符是 'x' 或 'X'
        // 表示需要将对应的参数打印成十六进制输出
        case 'x':
            // 'x' 表示用小写字母表示
            flags |= SMALL;
        case 'X':
            int num = va_arg(args, unsigned long);
            str = number(str, num, 16,
                         field_width, precision, flags);
            break;

        // 如果格式转换符是 'd'，'i' 或 'u'，表示需将对应的参数是一个指针类型
        case 'd':
        case 'i':
            // 'd'，'i' 表示有符号整数，因此需要加上带符号标志
            flags |= SIGN;
        case 'u':
            // 'u' 表示无符号整数
            str = number(str, va_arg(args, unsigned long), 10,
                         field_width, precision, flags);
            break;

        // 如果格式转换符是 'n'
        // 表示需要将到目前为止转换输出的字符数保存到对应参数指针指定的位置中
        case 'n':
            // 首先使用 va_arg 取得对应参数指针
            ip = va_arg(args, char *);
            *ip = (str - buf);
            break;
        
        default:
            // 如果格式指示符不是 '%'，则表示格式字符串有错
            if (*fmt != '%')
                // 直接将一个 '%' 写入到输出中
                *str++ = '%';
            // 如果格式转换符的位置处还有字符，则也直接将该字符写入输出串中
            // 然后继续循环处理格式字符串
            if (*fmt)
                *str++ = *fmt;
            else
                // 否则表示已经处理到格式字符串的结尾处，则退出循环
                --fmt;
            break;
        }
    }

    // 最后在转换好的字符串结尾处添上字符串结束标志
    *str = '\0';

    // 返回转换好的字符串长度
    return str - buf;
}

// 结果按格式输出字符串到 buf，返回格式化后字符串的长度
int sprintf(char *buf, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int i = vsprintf(buf, fmt, args);
    va_end(args);
    return i;
}