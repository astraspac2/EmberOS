#include "kernel.h"

static uint8_t cmos(uint8_t reg) {
    outb(0x70, reg);
    return inb(0x71);
}

static int bcd(int v) {
    return (v & 0x0F) + (v >> 4) * 10;
}

void rtc_read(struct rtc_time *t) {
    for (int i = 0; i < 1000 && (cmos(0x0A) & 0x80); i++) {
        /* wait out an update in progress */
    }
    int sec = cmos(0x00), min = cmos(0x02), hour = cmos(0x04);
    int day = cmos(0x07), mon = cmos(0x08), year = cmos(0x09);
    uint8_t regb = cmos(0x0B);

    bool pm = hour & 0x80;
    hour &= 0x7F;

    if (!(regb & 0x04)) {  /* values are BCD */
        sec = bcd(sec);
        min = bcd(min);
        hour = bcd(hour);
        day = bcd(day);
        mon = bcd(mon);
        year = bcd(year);
    }
    if (!(regb & 0x02) && pm) {  /* 12-hour clock */
        hour = (hour % 12) + 12;
    }

    t->second = sec;
    t->minute = min;
    t->hour = hour;
    t->day = day;
    t->month = mon;
    t->year = 2000 + year;
}
