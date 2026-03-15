// PolandOS — Panika jądra
#pragma once
#include "../../include/types.h"

__attribute__((noreturn)) void kpanic(const char *fmt, ...);
__attribute__((noreturn)) void kpanic_at(const char *file, int line, const char *fmt, ...);
#define PANIKA(fmt, ...) kpanic_at(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
