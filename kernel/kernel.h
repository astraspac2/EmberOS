#ifndef EMBER_KERNEL_H
#define EMBER_KERNEL_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "limine.h"

/* ---- port I/O ---- */

static inline void outb(uint16_t port, uint8_t v) {
    __asm__ volatile ("outb %0, %1" : : "a"(v), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t v;
    __asm__ volatile ("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static inline void io_wait(void) {
    outb(0x80, 0);
}

__attribute__((noreturn)) static inline void cpu_halt_forever(void) {
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

/* ---- lib.c ---- */

void *memcpy(void *dst, const void *src, size_t n);
void *memmove(void *dst, const void *src, size_t n);
void *memset(void *dst, int c, size_t n);
int memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);

/* ---- console.c ---- */

#define COLOR_BG      0x08100B
#define COLOR_FG      0xD4EBDD
#define COLOR_ACCENT  0x3DDC84
#define COLOR_DIM     0x4F8A66
#define COLOR_BAR     0x0F2A1A
#define COLOR_OK      0x7CFF9E
#define COLOR_ERR     0xFF4040

void console_init(struct limine_framebuffer *fb);
void console_clear(void);
void console_set_fg(uint32_t rgb);
void console_putc(char c);
void console_status(const char *text);

void kvprintf(const char *fmt, va_list ap);
void kprintf(const char *fmt, ...);
int ksnprintf(char *buf, size_t size, const char *fmt, ...);
__attribute__((noreturn)) void panic(const char *fmt, ...);

/* ---- cpu.c: GDT, IDT, PIC, exceptions ---- */

struct regs {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no, err;
    uint64_t rip, cs, rflags, rsp, ss;
};

typedef void (*irq_handler_t)(void);

void gdt_init(void);
void idt_init(void);
void pic_init(void);
void irq_register(int irq, irq_handler_t handler);
void irq_enable(int irq);

static inline void interrupts_enable(void) {
    __asm__ volatile ("sti");
}

/* ---- timer.c ---- */

void timer_init(uint32_t hz);
uint64_t timer_ticks(void);
uint32_t timer_hz(void);

/* ---- keyboard.c ---- */

void keyboard_init(void);
int keyboard_getchar(void); /* -1 when no key is waiting */

/* ---- mm.c: physical memory and kernel heap ---- */

#define PAGE_SIZE 4096

void pmm_init(struct limine_memmap_response *map, uint64_t hhdm_offset);
uint64_t pmm_alloc_page(void);
uint64_t pmm_alloc_contiguous(size_t pages);
void pmm_free_page(uint64_t phys);
uint64_t pmm_total_pages(void);
uint64_t pmm_free_pages(void);
uint64_t hhdm_phys_to_virt(uint64_t phys);

void heap_init(void);
void *kmalloc(size_t size);
void kfree(void *ptr);
size_t heap_free_bytes(void);

#endif
