#include <xos/string.h>

// copies one string to another
char *strcpy(char *dest, const char *src) {
    char *ptr = dest;
    while (true) {
        *ptr++ = *src;
        if (*src++ == EOS)
            break;
    }
    return dest;
}

// copies a certain amount of characters from one string to another
char *strncpy(char *dest, const char *src, size_t count) {
    char *ptr = dest;
    for (size_t i = 0; i < count; i++) {
        *ptr++ = *src;
        if (*src++ == EOS)
            break;
    }
    dest[count - 1] = EOS;
    return dest;
}

// concatenates two strings
char *strcat(char* dest, const char *src) {
    char *ptr = dest;
    while (*ptr != EOS) {
        ptr++;
    }
    while (true) {
        *ptr++ = *src;
        if (*src++ == EOS)
            break;
    }
    return dest;
}

// returns the length of a given string
size_t strlen(const char *str) {
    char *ptr = (char *)str;
    while (*ptr != EOS) {
        ptr++;
    }
    return ptr - str;
}

// compares two strings
int strcmp(const char *lhs, const char *rhs) {
    while (*lhs == *rhs && *lhs != EOS && *rhs != EOS) {
        lhs++;
        rhs++;
    }
    return *lhs < *rhs ? -1 : *lhs > *rhs;
}

// finds the first occurrence of a character
char *strchr(const char *str, int ch) {
    char *ptr = (char *)str;
    while (true) {
        // 将该条件置于前面是为了寻找字符为 EOS 的特殊情况
        if (*ptr == ch) {
            break;
        }
        if (*ptr++ == EOS) {
            ptr = NULL;
            break;
        }
    }
    return ptr;
}

// finds the last occurrence of a character
char *strrchr(const char *str, int ch) {
    char *ptr = (char *)str;
    char *last = NULL;
    while (true) {
        if (*ptr == ch) {
            last = ptr;
        }
        if (*ptr++ == EOS) {
            break;
        }
    }
    return last;
}

// compares two buffers
int memcmp(const void *lhs, const void *rhs, size_t count) {
    u8 *lptr = (u8 *)lhs;
    u8 *rptr = (u8 *)rhs;
    while (*lptr == *rptr && --count > 0) {
        lptr++;
        rptr++;
    }
    return *lptr < *rptr ? -1 : *lptr > *rptr;
}

// fills a buffer with a character
void *memset(void *dest, int ch, size_t count) {
    u8 *ptr = (u8 *)dest;
    while (count--) {
        *ptr++ = ch;
    }
    return dest;
}

// copies one buffer to another
void *memcpy(void *dest, const void *src, size_t count) {
    u8 *ptr = (u8 *)dest;
    while (count--) {
        *ptr++ = *(u8 *)(src++);
    }
    return dest;
}

// searches an array for the first occurrence of a character
void *memchr(const void *ptr, int ch, size_t count) {
    u8 *str = (u8 *)ptr;
    while (count--) {
        if (*str == ch) {
            break;
        }
        str++;
    }
    return (void *)str;
}