#include "kernel.h"

__attribute__((used, section(".limine_requests")))
static volatile uint64_t base_revision[3] = LIMINE_BASE_REVISION(3);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0,
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0,
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0,
};

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t requests_start[4] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t requests_end[2] = LIMINE_REQUESTS_END_MARKER;

static void ok(const char *msg) {
    console_set_fg(COLOR_DIM);
    kprintf("[ ");
    console_set_fg(COLOR_OK);
    kprintf("OK");
    console_set_fg(COLOR_DIM);
    kprintf(" ] ");
    console_set_fg(COLOR_FG);
    kprintf("%s\n", msg);
}

static void update_status(void) {
    char line[128];
    uint64_t secs = timer_ticks() / timer_hz();
    ksnprintf(line, sizeof(line), " EmberOS 0.2   up %lus   mem %lu/%lu MiB free   heap %lu KiB free",
              secs, pmm_free_pages() * PAGE_SIZE / (1024 * 1024),
              pmm_total_pages() * PAGE_SIZE / (1024 * 1024),
              (uint64_t)heap_free_bytes() / 1024);
    console_status(line);
}

void kmain(void) {
    if (!LIMINE_BASE_REVISION_SUPPORTED(base_revision) ||
        framebuffer_request.response == NULL ||
        framebuffer_request.response->framebuffer_count < 1 ||
        framebuffer_request.response->framebuffers[0]->bpp != 32 ||
        hhdm_request.response == NULL ||
        memmap_request.response == NULL) {
        cpu_halt_forever();
    }

    console_init(framebuffer_request.response->framebuffers[0]);

    console_set_fg(COLOR_ACCENT);
    kprintf("EmberOS 0.2\n");
    console_set_fg(COLOR_DIM);
    kprintf("kernel core: memory, interrupts, timer, keyboard\n\n");

    gdt_init();
    ok("GDT loaded");

    idt_init();
    pic_init();
    ok("IDT and PIC ready (exceptions 0-31, IRQs 32-47)");

    pmm_init(memmap_request.response, hhdm_request.response->offset);
    ok("physical memory manager");
    console_set_fg(COLOR_DIM);
    kprintf("      %lu MiB usable, %lu MiB free\n",
            pmm_total_pages() * PAGE_SIZE / (1024 * 1024),
            pmm_free_pages() * PAGE_SIZE / (1024 * 1024));

    heap_init();
    char *test = kmalloc(100);
    ok(test ? "kernel heap (kmalloc test passed)" : "kernel heap (kmalloc FAILED)");
    kfree(test);

    timer_init(100);
    ok("timer at 100 Hz");

    keyboard_init();
    ok("PS/2 keyboard");

    interrupts_enable();

    console_set_fg(COLOR_ACCENT);
    kprintf("\nType something (shift, caps lock and backspace work):\n");
    console_set_fg(COLOR_FG);

    uint64_t last_sec = (uint64_t)-1;
    for (;;) {
        int c;
        while ((c = keyboard_getchar()) >= 0) {
            console_putc((char)c);
        }
        uint64_t sec = timer_ticks() / timer_hz();
        if (sec != last_sec) {
            last_sec = sec;
            update_status();
        }
        __asm__ volatile ("hlt");
    }
}
