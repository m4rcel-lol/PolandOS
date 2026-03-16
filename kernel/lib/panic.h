// PolandOS — Panika jądra — Comprehensive panic system
#pragma once
#include "../../include/types.h"

// Main panic functions
__attribute__((noreturn)) void kpanic(const char *fmt, ...);
__attribute__((noreturn)) void kpanic_at(const char *file, int line, const char *fmt, ...);

// Assertion helper
void kassert_failed(const char *expr, const char *file, int line, const char *func);

// Early panic (before framebuffer init)
__attribute__((noreturn)) void early_panic(const char *msg);

// Convenience macro for panics with location
#define PANIKA(fmt, ...) kpanic_at(__FILE__, __LINE__, fmt, ##__VA_ARGS__)

// Assertion macro
#define KASSERT(expr) \
    do { \
        if (!(expr)) { \
            kassert_failed(#expr, __FILE__, __LINE__, __func__); \
        } \
    } while (0)

