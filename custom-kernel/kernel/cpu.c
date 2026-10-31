#include "kernel.h"

/* ---- GDT ---- */

static uint64_t gdt[3] = {
    0x0000000000000000ULL,  /* null */
    0x00AF9A000000FFFFULL,  /* 0x08: kernel code, 64-bit */
    0x00CF92000000FFFFULL,  /* 0x10: kernel data */
};

struct table_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

void gdt_init(void) {
    struct table_ptr gdtr = { sizeof(gdt) - 1, (uint64_t)gdt };
    __asm__ volatile (
        "lgdt %0\n"
        "pushq $0x08\n"
        "leaq 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        "movw $0x10, %%ax\n"
        "movw %%ax, %%ds\n"
        "movw %%ax, %%es\n"
        "movw %%ax, %%ss\n"
        "xorw %%ax, %%ax\n"
        "movw %%ax, %%fs\n"
        "movw %%ax, %%gs\n"
        : : "m"(gdtr) : "rax", "memory");
}

/* ---- IDT ---- */

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

extern uint64_t isr_stub_table[48];
static struct idt_entry idt[256];

void idt_init(void) {
    for (int i = 0; i < 48; i++) {
        uint64_t addr = isr_stub_table[i];
        idt[i].offset_low = addr & 0xFFFF;
        idt[i].selector = 0x08;
        idt[i].ist = 0;
        idt[i].type_attr = 0x8E;  /* present, ring 0, interrupt gate */
        idt[i].offset_mid = (addr >> 16) & 0xFFFF;
        idt[i].offset_high = (uint32_t)(addr >> 32);
        idt[i].zero = 0;
    }
    struct table_ptr idtr = { sizeof(idt) - 1, (uint64_t)idt };
    __asm__ volatile ("lidt %0" : : "m"(idtr));
}

/* ---- PIC (legacy 8259) ---- */

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

static uint8_t pic1_mask = 0xFF;
static uint8_t pic2_mask = 0xFF;

void pic_init(void) {
    outb(PIC1_CMD, 0x11); io_wait();
    outb(PIC2_CMD, 0x11); io_wait();
    outb(PIC1_DATA, 0x20); io_wait();  /* IRQ 0-7  -> vectors 0x20-0x27 */
    outb(PIC2_DATA, 0x28); io_wait();  /* IRQ 8-15 -> vectors 0x28-0x2F */
    outb(PIC1_DATA, 0x04); io_wait();  /* slave on IRQ2 */
    outb(PIC2_DATA, 0x02); io_wait();
    outb(PIC1_DATA, 0x01); io_wait();
    outb(PIC2_DATA, 0x01); io_wait();

    /* Everything masked except the cascade line; drivers enable their own. */
    pic1_mask = 0xFB;
    pic2_mask = 0xFF;
    outb(PIC1_DATA, pic1_mask);
    outb(PIC2_DATA, pic2_mask);
}

void irq_enable(int irq) {
    if (irq < 8) {
        pic1_mask &= ~(1u << irq);
        outb(PIC1_DATA, pic1_mask);
    } else {
        pic2_mask &= ~(1u << (irq - 8));
        outb(PIC2_DATA, pic2_mask);
    }
}

static void pic_eoi(int irq) {
    if (irq >= 8) {
        outb(PIC2_CMD, 0x20);
    }
    outb(PIC1_CMD, 0x20);
}

static irq_handler_t irq_handlers[16];

void irq_register(int irq, irq_handler_t handler) {
    irq_handlers[irq] = handler;
    irq_enable(irq);
}

/* ---- dispatch ---- */

static const char *const exception_names[32] = {
    "Divide error", "Debug", "Non-maskable interrupt", "Breakpoint",
    "Overflow", "Bound range exceeded", "Invalid opcode", "Device not available",
    "Double fault", "Coprocessor segment overrun", "Invalid TSS", "Segment not present",
    "Stack-segment fault", "General protection fault", "Page fault", "Reserved",
    "x87 floating-point", "Alignment check", "Machine check", "SIMD floating-point",
    "Virtualization", "Control protection", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Hypervisor injection", "VMM communication", "Security exception", "Reserved",
};

static void exception(struct regs *r) {
    uint64_t cr2 = 0;
    if (r->int_no == 14) {
        __asm__ volatile ("movq %%cr2, %0" : "=r"(cr2));
    }
    panic("Exception %lu: %s\nerror=%lx rip=%lx cs=%lx\nrflags=%lx rsp=%lx cr2=%lx",
          r->int_no, exception_names[r->int_no], r->err, r->rip, r->cs,
          r->rflags, r->rsp, cr2);
}

void isr_handler(struct regs *r);

void isr_handler(struct regs *r) {
    if (r->int_no < 32) {
        exception(r);
        return;
    }
    int irq = (int)r->int_no - 32;
    if (irq_handlers[irq]) {
        irq_handlers[irq]();
    }
    pic_eoi(irq);
}
