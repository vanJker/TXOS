# 078 虚拟设备

目前 XOS 已经拥有了控制台界面、键盘、硬盘等设备，这些设备的操作具有共通性，例如控制台、键盘这类字符设备可以进行字符粒度的读写操作，硬盘、分区这类设备可以进行块粒度（例如扇区）的读写操作。

所以可以对硬件设备进行一层抽象，使得读写更加的统一，方便以后的操作。思路是定义一个抽象设备类型 `device_t`，对该类型的读写会根据设备的具体类型，从而调用不同的读写操作函数。类似于 C++ 的 Abstract Base Class, Java 的 Interface，Rust 的 Trait 等，这一节本质上是使用 C 语言来写面向对象。

本节需要实现的主要功能：

```c
//--> include/xos/device.h

// 安装设备
did_t dev_install(dev_type_t type, dev_subtype_t subtype, void *dev, char *name, 
                  did_t parent, void *ioctl, void *read, void *write);

// 根据设备具体类型查找该类型的第 idx 个设备
dev_t *dev_find(dev_subtype_t subtype, size_t idx);

// 根据设备号查找设备
dev_t *dev_get(did_t did);

// 控制设备
i32 dev_ioctl(did_t did, dev_cmd_t cmd, void *args, i32 flags);

// 读设备
i32 dev_read(did_t did, void *buf, size_t count, size_t idx, i32 flags);

// 写设备
i32 dev_write(did_t did, void *buf, size_t count, size_t idx, i32 flags);
```

## 2. 代码分析

### 2.1 虚拟设备类型

XOS 对于虚拟设备的类型进行划分，例如字符设备，块设备等；而对于每个设别类型，右划分为具体的设备类型，例如控制台、键盘等设备是字符设备，硬盘、分区等是块设备。

```c
//--> include/xos/device.h

// 设备类型 (例如字符设备、块设备等)
typedef enum dev_type_t {
    DEV_NULL,   // 空设备
    DEV_CHAR,   // 字符设备
    DEV_BLOCK,  // 块设备
} dev_type_t;

// 设备具体类型 (例如控制台、键盘、硬盘等)
typedef enum dev_subtype_t {
    DEV_CONSOLE,    // 控制台
    DEV_KEYBOARD,   // 键盘
} dev_subtype_t;
```

### 2.2 虚拟设备命令

虽然有多种具体设备，但是这些设备都有一些共同操作，例如读、写。当然可能有些设备只能进行读操作（例如控制台），有些设备只能进行写操作（例如键盘），有些设备既可以进行读操作也可以进行写操作（例如硬盘）。所以可以抽象出读写的设备命令：

```c
//--> include/xos/device.h

// 设备命令 (例如读、写等)
typedef enum dev_cmd_t {
    DEV_CMD_NULL,   // 空命令
} dev_cmd_t;
```

### 2.3 虚拟设备

使用函数指针来实现 C 语言的面向对象，抽象设备 `dev_t` 类似于面向对象中的一个类，拥有数据和成员函数（这里使用函数指针实现），因为是使用函数指针实现的成员函数，所以这个“类”拥有更大的灵活性，可以实现成员函数的多态性（即不同设备可以调用不同的具体读、写、控制函数）：

```c
//--> include/xos/types.h

// 设备标识符
typedef i32 did_t;


//--> include/xos/device.h

// 设备名称长度
#define DEV_NAMELEN 16

// 虚拟设备
typedef struct dev_t {
    char name[DEV_NAMELEN]; // 设备名
    dev_type_t type;        // 设备类型
    dev_subtype_t subtype;  // 设备具体类型
    did_t did;              // 设备号
    did_t parent;           // 父设备号
    void *dev;              // 具体设备位置

    // 控制设备
    i32 (*ioctl)(void *dev, dev_cmd_t cmd, void *args, i32 flags);
    // 读设备
    i32 (*read)(void *dev, void *buf, size_t count, size_t idx, i32 flags);
    // 写设备
    i32 (*write)(void *dev, void *buf, size_t count, size_t idx, i32 flags);
} dev_t;
```

这里使用 `dev` 指针来指向具体设备的位置（例如可以指向 `ata_disk_t` 的具体硬盘），在函数指针中使用 `void * dev` 作为第一个参数，功能类似于 C++ 类中的 `this` 指针。

这也要求设备对应的具体读、写、控制功能函数的原型，必须与这里的函数指针类型相对应，因为 C 语言标准中不同类型的函数指针之间的转换会产生未定义行为 (UB)。

### 2.4 虚拟设备管理

类似于进程管理，这里使用一个虚拟设备数组来对系统中的虚拟设备进行管理：

```c
//--> kernel/device.c

#define DEV_NR 64               // 设备数量
static dev_t devices[DEV_NR];   // 设备数组
```

获取空设备（即未在虚拟设备数组注册的设备）的逻辑很简单：遍历虚拟设备数组寻找类型为 `DEV_NULL` 的虚拟设备并返回：

```c
// 从设备数组获取一个空设备
static dev_t *get_null_dev() {
    for (size_t i = 0; i < DEV_NR; i++) {
        dev_t *dev = &devices[i];
        if (dev->type == DEV_NULL) {
            return dev;
        }
    }
    panic("no more devices!!!");
}
```

在虚拟设备数组中，根据设备号获取虚拟设备：

```c
// 根据设备号查找设备
dev_t *dev_get(did_t did) {
    assert(did >= 0 && did < DEV_NR);
    dev_t *dev = &devices[did];
    assert(dev->type != DEV_NULL);
    return dev;
}
```

在虚拟设备数组中，根据设备具体类型来查找该类型的第 idx 个设备：

```c
// 根据设备具体类型查找该类型的第 idx 个设备
dev_t *dev_find(dev_subtype_t subtype, size_t idx) {
    size_t count = 0;
    for (size_t i = 0; i < DEV_NR; i++) {
        dev_t *dev = &devices[i];
        if (dev->subtype != subtype) continue;
        if (count++ == idx) {
            return dev;
        }
    }
    return NULL;
}
```

### 2.6 注册虚拟设备

在虚拟设备数组对拥有的具体设备进行注册：

1. 在虚拟设备数组中获取一个空设备
2. 在获取的空设备上进行注册，设置设备名、设备类型、设备功能函数指针、设备具体位置等等
3. 由于设备号在虚拟设备初始化时就已经分配好了，所以无需设置，直接返回该虚拟设备的设备号即可

```c
//--> kernel/device.c

// 安装设备
did_t dev_install(dev_type_t type, dev_subtype_t subtype, void *dev, char *name, 
                  did_t parent, void *ioctl, void *read, void *write) {
    dev_t *vdev = get_null_dev();
    
    strncpy(vdev->name, name, DEV_NAMELEN);
    vdev->type = type;
    vdev->subtype = subtype;
    vdev->parent = 0;
    vdev->dev = dev;
    vdev->ioctl = ioctl;
    vdev->read = read;
    vdev->write = write;

    return vdev->did;
}
```

### 2.7 操作虚拟设备

对虚拟设备的操作本质上为，通过函数指针来调用，该虚拟设备对应的设备相应的功能函数。当然需要进行一些检查防止未定义行为，例如会检查对应的函数指针是否为空，所以相对于直接使用虚拟设备的函数指针来调用功能函数，使用这类操作功能函数会更加安全，同时也节省了调用者自己进行检查的冗余代码：

```c
//--> kernel/device.c

// 控制设备
i32 dev_ioctl(did_t did, dev_cmd_t cmd, void *args, i32 flags) {
    dev_t *dev = dev_get(did);
    if (dev->ioctl == NULL) {
        LOGK("Device %d's ioctl is unimplement...\n", dev->did);
        return EOF;
    }
    return dev->ioctl(dev, cmd, args, flags);
}

// 读设备
i32 dev_read(did_t did, void *buf, size_t count, size_t idx, i32 flags) {
    dev_t *dev = dev_get(did);
    if (dev->read == NULL) {
        LOGK("Device %d's read is unimplement...\n", dev->did);
        return EOF;
    }
    return dev->read(dev, buf, count, idx, flags);
}

// 写设备
i32 dev_write(did_t did, void *buf, size_t count, size_t idx, i32 flags) {
    dev_t *dev = dev_get(did);
    if (dev->write == NULL) {
        LOGK("Device %d's write is unimplement...\n", dev->did);
        return EOF;
    }
    return dev->write(dev, buf, count, idx, flags);
}
```

### 2.8 虚拟设备初始化

虚拟设备初始化时主要是分配设备号，同时需要进行一些设置防止未定义行为：

```c
//--> kernel/device.c

// 初始化虚拟设备
void device_init() {
    for (size_t i = 0; i < DEV_NR; i++) {
        dev_t *dev = &devices[i];
        strncpy(dev->name, "null", DEV_NAMELEN);
        dev->type = DEV_NULL;
        dev->did = i;
        dev->parent = 0;
        dev->dev = NULL;
        dev->ioctl = NULL;
        dev->read = NULL;
        dev->write = NULL;
    }
}
```

在 `main.c` 中调用虚拟设备初始化函数。因为引入虚拟设备这个抽象概念后，控制台、键盘、硬盘这些设备都需要在初始化时进行虚拟设备注册，而虚拟设备注册应当再虚拟设备初始化完成后进行，所以需要将 `device_init()` 放置在所有设备初始化之前调用。

```c
//--> kernel/main.c

...
extern void device_init();

void kernel_init() {
    device_init();
    ...
}
```

### 2.9 设备驱动兼容

正如前面所述，设备的具体操作函数需要与虚拟设备中的函数指针原型一致，这需要对原先的设备，例如控制台、键盘的操作函数原型，进行一些兼容改写（硬盘等块设备的兼容性改写留至下一节）：

```c
//--> kernel/concole.c
- i32 console_write(char *buf, size_t count, u8 attr) {
+ i32 console_write(dev_t *_dev, char *buf, size_t count, size_t _idx, i32 flags) {
+     u8 attr = (u8)flags;
      ...
  }

//--> kernel/keyboard.c
- i32 keyboard_read(char *buf, size_t count) {
+ i32 keyboard_read(dev_t *_dev, char *buf, size_t count, size_t _idx, i32 _flags) {
      ...
  }

//--> kernel/printk.c
  int printk(const char *fmt, ...) {
      ...
-     console_write(buf, i, TEXT);
+     console_write(NULL, buf, i, 0, TEXT);
      ...
  }

//--> kernel/syscall.c
  static i32 sys_write(fd_t fd, const void *buf, size_t len) {
      if (fd == STDOUT || fd == STDERR) {
-         return console_write((char *)buf, len, TEXT);
+         return console_write(NULL, (char *)buf, len, 0, TEXT);
      }
      ...
  }
```

对于控制台、键盘这类“唯一”的设备，即这种类型只会存在一个这样的设备，在虚拟设备中无需设置具体的设备位置。同时这里约定，具体功能函数对于原型中未用到的参数，使用下划线 `_` 进行开头，表示原型需要该参数，但是未使用到。

注册虚拟设备：

```c
//--> kernel/concole.c
void console_init() {
    ...
    dev_install(DEV_CHAR, DEV_CONSOLE, NULL, "console", 0, NULL, NULL, console_write);
}

//--> kernel/keyboard.c
void keyboard_init() {
    ...
    dev_install(DEV_CHAR, DEV_KEYBOARD, NULL, "keyboard", 0, NULL, keyboard_read, NULL);
}
```

## 3. 功能测试

将测试用的系统调用 `sys_test` 逻辑修改为：使用键盘读入用户键入的字符，然后使用控制台将刚刚获取的字符打印出来（使用 DEBUG 对应的绿色），当然都是使用虚拟设备进行操作。

```c
static u32 sys_test() {
    char ch;
    dev_t *device;

    device = dev_find(DEV_KEYBOARD, 0);
    assert(device);
    dev_read(device->did, &ch, 1, 0, 0);

    device = dev_find(DEV_CONSOLE, 0);
    assert(device);
    dev_write(device->did, &ch, 1, 0, DEBUG);

    return 255;
}
```

在测试任务 `test_thread` 中加入调用测试用系统调用 `sys_test` 的逻辑：

```c
void test_thread() {
    irq_enable();
    while (true) {
        test();
    }
}
```

预期为：控制台会以字符为粒度，打印我们通过输入键盘输入的字符，并显示为绿色。

> 可以通过设置断点来观察虚拟设备功能的流程。
