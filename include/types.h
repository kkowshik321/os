/* types.h - fixed-width types for a freestanding RV64 kernel (no <stdint.h> needed). */
#ifndef NANOBOOT_TYPES_H
#define NANOBOOT_TYPES_H

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long      u64;   /* LP64: long is 64 bits on RV64 */

typedef signed char        s8;
typedef signed short       s16;
typedef signed int         s32;
typedef signed long        s64;

typedef u64                usize; /* size/address-sized integer */
typedef usize              size_t;
#define SIZE_MAX ((size_t)~(size_t)0)

#define NULL ((void *)0)

#endif /* NANOBOOT_TYPES_H */
