#ifndef XOS_IO_H
#define XOS_IO_H

#include <xos/types.h>

extern u8  inb(u32 port); // 获取外设端口 port 的一个字节数据
extern u16 inw(u32 port); // 获取外设端口 port 的一个字数据

extern void outb(u32 port, u8  value); // 向外设端口 port 输入一个字节数据 value
extern void outw(u32 prot, u16 value); // 向外设端口 port 输入一个字数据   value

#endif