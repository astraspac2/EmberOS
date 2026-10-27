#include "kernel.h"

#define PIT_FREQ 1193182u

static volatile uint64_t ticks;
static uint32_t frequency;

static void timer_irq(void) {
    ticks++;
}

void timer_init(uint32_t hz) {
    frequency = hz;
    uint32_t divisor = PIT_FREQ / hz;
    outb(0x43, 0x36);  /* channel 0, lobyte/hibyte, square wave */
    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);
    irq_register(0, timer_irq);
}

uint64_t timer_ticks(void) {
    return ticks;
}

uint32_t timer_hz(void) {
    return frequency;
}
