// PolandOS — Implementacja biblioteki stringów
#include "string.h"

void *memset(void *dst, int c, size_t n) {
    unsigned char *p = (unsigned char *)dst;
    unsigned char val = (unsigned char)c;
    while (n--) *p++ = val;
    return dst;
}

void *memcpy(void *dst, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dst;
}

void *memmove(void *dst, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else if (d > s) {
        d += n;
        s += n;
        while (n--) *--d = *--s;
    }
    return dst;
}

int memcmp(const void *a, const void *b, size_t n) {
    const unsigned char *pa = (const unsigned char *)a;
    const unsigned char *pb = (const unsigned char *)b;
    while (n--) {
        if (*pa != *pb) return (int)*pa - (int)*pb;
        pa++; pb++;
    }
    return 0;
}

size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

int strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n) {
    while (n && *a && (*a == *b)) { a++; b++; n--; }
    if (n == 0) return 0;
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

char *strcpy(char *dst, const char *src) {
    char *orig = dst;
    while ((*dst++ = *src++) != '\0');
    return orig;
}

char *strncpy(char *dst, const char *src, size_t n) {
    char *orig = dst;
    while (n && (*dst++ = *src++) != '\0') n--;
    while (n--) *dst++ = '\0';
    return orig;
}

char *strcat(char *dst, const char *src) {
    char *orig = dst;
    while (*dst) dst++;
    while ((*dst++ = *src++) != '\0');
    return orig;
}

char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    if (c == '\0') return (char *)s;
    return (char *)0;
}

char *strrchr(const char *s, int c) {
    const char *last = (char *)0;
    while (*s) {
        if (*s == (char)c) last = s;
        s++;
    }
    if (c == '\0') return (char *)s;
    return (char *)last;
}

int atoi(const char *s) {
    int result = 0;
    int sign = 1;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') { s++; }
    while (*s >= '0' && *s <= '9') {
        result = result * 10 + (*s - '0');
        s++;
    }
    return sign * result;
}

static const char digits_lower[] = "0123456789abcdef";
static const char digits_upper[] = "0123456789ABCDEF";

void utoa(u64 val, char *buf, int base) {
    char tmp[65];
    int i = 0;
    if (val == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    while (val > 0) {
        tmp[i++] = digits_lower[val % (u64)base];
        val /= (u64)base;
    }
    // reverse
    int j = 0;
    while (i > 0) buf[j++] = tmp[--i];
    buf[j] = '\0';
}

void itoa(i64 val, char *buf, int base) {
    if (base == 10 && val < 0) {
        *buf++ = '-';
        utoa((u64)(-val), buf, base);
    } else {
        utoa((u64)val, buf, base);
    }
}

// strtok: NOT reentrant — uses static state; do not call from interrupt handlers
char *strtok(char *str, const char *delim) {
    static char *saved = (char *)0;
    if (str) saved = str;
    if (!saved || !*saved) return (char *)0;

    // skip leading delimiters
    while (*saved) {
        const char *d = delim;
        int found = 0;
        while (*d) { if (*saved == *d) { found = 1; break; } d++; }
        if (!found) break;
        saved++;
    }
    if (!*saved) return (char *)0;

    char *token_start = saved;
    while (*saved) {
        const char *d = delim;
        while (*d) {
            if (*saved == *d) {
                *saved++ = '\0';
                return token_start;
            }
            d++;
        }
        saved++;
    }
    return token_start;
}
