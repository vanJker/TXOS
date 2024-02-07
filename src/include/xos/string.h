#ifndef XOS_STRING_H
#define XOS_STRING_H

#include <xos/types.h>

char *strcpy(char *dest, const char *src); // copies one string to another
char *strncpy(char *dest, const char *src, size_t count); // copies a certain amount of characters from one string to another
char *strcat(char* dest, const char *src); // concatenates two strings
size_t strlen(const char *str);            // returns the length of a given string
int strcmp(const char *lhs, const char *rhs); // compares two strings
char *strchr(const char *str, int ch);     // finds the first occurrence of a character
char *strrchr(const char *str, int ch);    // finds the last occurrence of a character

int memcmp(const void *lhs, const void *rhs, size_t count); // compares two buffers
void *memset(void *dest, int ch, size_t count);          // fills a buffer with a character
void *memcpy(void *dest, const void *src, size_t count); // copies one buffer to another
void *memchr(const void *ptr, int ch, size_t count);     // searches an array for the first occurrence of a character

#endif