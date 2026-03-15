// PolandOS — Wyjście formatowane
#pragma once
#include "../../include/types.h"
#include <stdarg.h>

int kprintf(const char *fmt, ...);
int kvprintf(const char *fmt, va_list ap);
int ksnprintf(char *buf, size_t size, const char *fmt, ...);
int kvsnprintf(char *buf, size_t size, const char *fmt, va_list ap);
