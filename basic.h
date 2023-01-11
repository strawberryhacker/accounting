#ifndef BASIC_H
#define BASIC_H

#include "stdint.h"
#include "stdio.h"
#include "stdarg.h"

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

#define diff(a, b) (((a) < (b)) ? (b - a) : (a - b))
#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))
#define limit(a, lower, upper)  (((a) < (lower)) ? (lower) : ((a) > (upper)) ? (upper) : (a))

static inline int get_digit_count(double n) {
    if (n < 0) return get_digit_count(-n) + 1;
    if (n < 10) return 1;
    return 1 + get_digit_count(n / 10);
}

static inline int debug(const char* message, ...) {
    va_list arguments;
    va_start(arguments, message);
    FILE* file = fopen(REDIRECT, "w");
    int size = vfprintf(file, message, arguments);
    fclose(file);
    return size;
}

static inline int compute_offset(int cursor, int offset, int width, int left_margin, int right_margin) {
    int adjust = (offset + left_margin) - cursor;
    if (adjust > 0) offset = max(0, offset - adjust);
    adjust = cursor - (offset + width - right_margin);
    if (adjust > 0) offset += adjust;
    return offset;
}

#endif
