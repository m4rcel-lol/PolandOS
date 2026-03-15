// PolandOS — Typy podstawowe
// Jądro Orzeł
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;
typedef uintptr_t uptr;
#ifndef NULL
#define NULL ((void*)0)
#endif
#ifndef true
#define true 1
#endif
#ifndef false
#define false 0
#endif
