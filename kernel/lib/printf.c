// PolandOS — Wyjście formatowane jądra
// Obsługuje: %d %i %u %x %X %o %s %c %p %% %zu %lu %llu %ld %lld
// Flagi: '-' (wyrównaj do lewej), '0' (uzupełnij zerami)
// Szerokość i precyzja
#include "printf.h"
#include "string.h"
#include "../drivers/serial.h"

// Declared in fb.c — output one char to framebuffer console
extern void fb_putchar(char c);

// ─── Internal buffer writer ──────────────────────────────────────────────────
typedef struct {
    char   *buf;
    size_t  size;
    size_t  pos;
    int     is_buf; // 1 = write to buffer, 0 = write to output devices
} PrintCtx;

static void ctx_putchar(PrintCtx *ctx, char c) {
    if (ctx->is_buf) {
        if (ctx->pos + 1 < ctx->size) {
            ctx->buf[ctx->pos++] = c;
        }
    } else {
        serial_write_char(c);
        fb_putchar(c);
    }
}

// Write a string to ctx, with width/precision/flags
static int ctx_write_str(PrintCtx *ctx, const char *s, int width, int prec, int left_align, int zero_pad) {
    if (!s) s = "(null)";
    int len = (int)strlen(s);
    if (prec >= 0 && prec < len) len = prec;

    int pad = (width > len) ? (width - len) : 0;
    int count = 0;

    char pad_char = (zero_pad && !left_align) ? '0' : ' ';

    if (!left_align) {
        for (int i = 0; i < pad; i++) { ctx_putchar(ctx, pad_char); count++; }
    }
    for (int i = 0; i < len; i++) { ctx_putchar(ctx, s[i]); count++; }
    if (left_align) {
        for (int i = 0; i < pad; i++) { ctx_putchar(ctx, ' '); count++; }
    }
    return count;
}

// Format an integer
static int fmt_int(PrintCtx *ctx, u64 uval, int is_signed, int neg,
                   int base, int upper, int width, int prec,
                   int left_align, int zero_pad, int alt_form) {
    char tmp[66];
    int  ti = 0;
    const char *hex = upper ? "0123456789ABCDEF" : "0123456789abcdef";

    if (uval == 0) {
        tmp[ti++] = '0';
    } else {
        u64 v = uval;
        while (v > 0) {
            tmp[ti++] = hex[v % (u64)base];
            v /= (u64)base;
        }
    }

    // prefix
    char prefix[4];
    int  prefix_len = 0;
    if (is_signed && neg)         prefix[prefix_len++] = '-';
    else if (alt_form && base == 16) { prefix[prefix_len++] = '0'; prefix[prefix_len++] = upper ? 'X' : 'x'; }
    else if (alt_form && base ==  8 && tmp[0] != '0') prefix[prefix_len++] = '0';

    // prec: minimum digit count
    int digits = ti;
    int extra_zeros = (prec > digits) ? (prec - digits) : 0;

    int content_len = prefix_len + extra_zeros + digits;
    int pad = (width > content_len) ? (width - content_len) : 0;

    char pad_char = (zero_pad && !left_align && prec < 0) ? '0' : ' ';
    int count = 0;

    if (!left_align) {
        if (zero_pad && prec < 0) {
            // prefix first, then zero pad
            for (int i = 0; i < prefix_len; i++) { ctx_putchar(ctx, prefix[i]); count++; }
            for (int i = 0; i < pad; i++) { ctx_putchar(ctx, '0'); count++; }
        } else {
            for (int i = 0; i < pad; i++) { ctx_putchar(ctx, pad_char); count++; }
            for (int i = 0; i < prefix_len; i++) { ctx_putchar(ctx, prefix[i]); count++; }
        }
    } else {
        for (int i = 0; i < prefix_len; i++) { ctx_putchar(ctx, prefix[i]); count++; }
    }
    for (int i = 0; i < extra_zeros; i++) { ctx_putchar(ctx, '0'); count++; }
    for (int i = ti - 1; i >= 0; i--) { ctx_putchar(ctx, tmp[i]); count++; }
    if (left_align) {
        for (int i = 0; i < pad; i++) { ctx_putchar(ctx, ' '); count++; }
    }
    return count;
}

// ─── Core formatter ──────────────────────────────────────────────────────────
static int vfmt(PrintCtx *ctx, const char *fmt, va_list ap) {
    int total = 0;

    while (*fmt) {
        if (*fmt != '%') {
            ctx_putchar(ctx, *fmt++);
            total++;
            continue;
        }
        fmt++; // consume '%'

        // Flags
        int left_align = 0, zero_pad = 0, alt_form = 0, space_sign = 0, plus_sign = 0;
        for (;;) {
            if      (*fmt == '-') { left_align = 1; fmt++; }
            else if (*fmt == '0') { zero_pad   = 1; fmt++; }
            else if (*fmt == '#') { alt_form   = 1; fmt++; }
            else if (*fmt == ' ') { space_sign = 1; fmt++; }
            else if (*fmt == '+') { plus_sign  = 1; fmt++; }
            else break;
        }
        (void)space_sign; (void)plus_sign;

        // Width
        int width = 0;
        if (*fmt == '*') { width = va_arg(ap, int); fmt++; }
        else while (*fmt >= '0' && *fmt <= '9') { width = width * 10 + (*fmt++ - '0'); }

        // Precision
        int prec = -1;
        if (*fmt == '.') {
            fmt++;
            prec = 0;
            if (*fmt == '*') { prec = va_arg(ap, int); fmt++; }
            else while (*fmt >= '0' && *fmt <= '9') { prec = prec * 10 + (*fmt++ - '0'); }
        }

        // Length modifier
        int is_long = 0, is_long_long = 0, is_size = 0;
        if (*fmt == 'l') {
            fmt++;
            if (*fmt == 'l') { is_long_long = 1; fmt++; }
            else { is_long = 1; }
        } else if (*fmt == 'z') {
            is_size = 1; fmt++;
        } else if (*fmt == 'h') {
            fmt++; // short — still read int
            if (*fmt == 'h') fmt++;
        }

        char spec = *fmt++;
        switch (spec) {
            case 'd': case 'i': {
                i64 v;
                if (is_long_long) v = va_arg(ap, long long);
                else if (is_long || is_size) v = va_arg(ap, long);
                else v = (i32)va_arg(ap, int);
                int neg = (v < 0);
                u64 uv = neg ? (u64)(-v) : (u64)v;
                total += fmt_int(ctx, uv, 1, neg, 10, 0, width, prec, left_align, zero_pad, 0);
                break;
            }
            case 'u': {
                u64 v;
                if (is_long_long) v = va_arg(ap, unsigned long long);
                else if (is_long || is_size) v = va_arg(ap, unsigned long);
                else v = (u32)va_arg(ap, unsigned int);
                total += fmt_int(ctx, v, 0, 0, 10, 0, width, prec, left_align, zero_pad, 0);
                break;
            }
            case 'x': case 'X': {
                u64 v;
                if (is_long_long) v = va_arg(ap, unsigned long long);
                else if (is_long || is_size) v = va_arg(ap, unsigned long);
                else v = (u32)va_arg(ap, unsigned int);
                total += fmt_int(ctx, v, 0, 0, 16, spec == 'X', width, prec, left_align, zero_pad, alt_form);
                break;
            }
            case 'o': {
                u64 v;
                if (is_long_long) v = va_arg(ap, unsigned long long);
                else if (is_long || is_size) v = va_arg(ap, unsigned long);
                else v = (u32)va_arg(ap, unsigned int);
                total += fmt_int(ctx, v, 0, 0, 8, 0, width, prec, left_align, zero_pad, alt_form);
                break;
            }
            case 'p': {
                u64 v = (u64)(uintptr_t)va_arg(ap, void *);
                // Always 0x prefix
                ctx_putchar(ctx, '0'); ctx_putchar(ctx, 'x'); total += 2;
                total += fmt_int(ctx, v, 0, 0, 16, 0, (width > 2 ? width - 2 : 0), prec, left_align, 1, 0);
                break;
            }
            case 's': {
                const char *s = va_arg(ap, const char *);
                total += ctx_write_str(ctx, s, width, prec, left_align, 0);
                break;
            }
            case 'c': {
                char c = (char)va_arg(ap, int);
                char buf[2] = { c, '\0' };
                total += ctx_write_str(ctx, buf, width, -1, left_align, 0);
                break;
            }
            case '%': {
                ctx_putchar(ctx, '%');
                total++;
                break;
            }
            case 'n': {
                int *p = va_arg(ap, int *);
                *p = total;
                break;
            }
            default: {
                // Unknown specifier — emit literally
                ctx_putchar(ctx, '%');
                ctx_putchar(ctx, spec);
                total += 2;
                break;
            }
        }
    }

    if (ctx->is_buf && ctx->size > 0)
        ctx->buf[ctx->pos] = '\0';

    return total;
}

// ─── Public API ──────────────────────────────────────────────────────────────
int kvprintf(const char *fmt, va_list ap) {
    PrintCtx ctx = { .buf = (char *)0, .size = 0, .pos = 0, .is_buf = 0 };
    return vfmt(&ctx, fmt, ap);
}

int kprintf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = kvprintf(fmt, ap);
    va_end(ap);
    return r;
}

int kvsnprintf(char *buf, size_t size, const char *fmt, va_list ap) {
    PrintCtx ctx = { .buf = buf, .size = size, .pos = 0, .is_buf = 1 };
    return vfmt(&ctx, fmt, ap);
}

int ksnprintf(char *buf, size_t size, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = kvsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return r;
}
