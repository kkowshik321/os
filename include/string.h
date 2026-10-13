/* string.h - NanoBoot-local string/memory helpers (no libc). */
#ifndef NANOBOOT_STRING_H
#define NANOBOOT_STRING_H

#include "types.h"

size_t strlen(const char *s);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);
void  *memcpy(void *dst, const void *src, size_t n);
void  *memset(void *dst, int c, size_t n);

#endif /* NANOBOOT_STRING_H */
