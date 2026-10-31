#include "kernel.h"

/* The compiler may emit calls to these even in freestanding code. */

void *memcpy(void *dst, const void *src, size_t n) {
    void *ret = dst;
    __asm__ volatile ("rep movsb" : "+D"(dst), "+S"(src), "+c"(n) : : "memory");
    return ret;
}

void *memmove(void *dst, const void *src, size_t n) {
    uint8_t *d = dst;
    const uint8_t *s = src;
    if (d == s || n == 0) {
        return dst;
    }
    if (d < s) {
        return memcpy(dst, src, n);
    }
    for (size_t i = n; i > 0; i--) {
        d[i - 1] = s[i - 1];
    }
    return dst;
}

void *memset(void *dst, int c, size_t n) {
    void *ret = dst;
    __asm__ volatile ("rep stosb" : "+D"(dst), "+c"(n) : "a"((uint8_t)c) : "memory");
    return ret;
}

int memcmp(const void *a, const void *b, size_t n) {
    const uint8_t *x = a;
    const uint8_t *y = b;
    for (size_t i = 0; i < n; i++) {
        if (x[i] != y[i]) {
            return x[i] < y[i] ? -1 : 1;
        }
    }
    return 0;
}

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

size_t kstrlcpy(char *dst, const char *src, size_t size) {
    size_t n = 0;
    while (src[n] && n + 1 < size) {
        dst[n] = src[n];
        n++;
    }
    if (size) {
        dst[n] = '\0';
    }
    return n;
}

size_t strlen(const char *s) {
    size_t n = 0;
    while (s[n]) {
        n++;
    }
    return n;
}
