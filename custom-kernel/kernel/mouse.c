#include "kernel.h"

/* PS/2 mouse on the auxiliary port (IRQ 12), standard 3-byte packets. */

#define PS2_DATA   0x60
#define PS2_CMD    0x64
#define QUEUE_SIZE 128

static int screen_w, screen_h;
static int pos_x, pos_y;
static uint8_t packet[3];
static int phase;

static struct mouse_event queue[QUEUE_SIZE];
static volatile size_t q_head, q_tail;

static void wait_write(void) {
    for (int i = 0; i < 100000; i++) {
        if (!(inb(PS2_CMD) & 0x02)) {
            return;
        }
    }
}

static void wait_read(void) {
    for (int i = 0; i < 100000; i++) {
        if (inb(PS2_CMD) & 0x01) {
            return;
        }
    }
}

static void mouse_write(uint8_t v) {
    wait_write();
    outb(PS2_CMD, 0xD4);  /* next byte goes to the mouse */
    wait_write();
    outb(PS2_DATA, v);
}

static uint8_t mouse_read(void) {
    wait_read();
    return inb(PS2_DATA);
}

static void push_event(int buttons) {
    size_t next = (q_head + 1) % QUEUE_SIZE;
    if (next == q_tail) {
        return;
    }
    queue[q_head].x = pos_x;
    queue[q_head].y = pos_y;
    queue[q_head].buttons = buttons;
    q_head = next;
}

static void mouse_irq(void) {
    if (!(inb(PS2_CMD) & 0x20)) {
        return;  /* not mouse data */
    }
    uint8_t b = inb(PS2_DATA);

    if (phase == 0) {
        if (!(b & 0x08)) {
            return;  /* out of sync, wait for a first byte */
        }
        packet[0] = b;
        phase = 1;
    } else if (phase == 1) {
        packet[1] = b;
        phase = 2;
    } else {
        packet[2] = b;
        phase = 0;

        if (packet[0] & 0xC0) {
            return;  /* overflow, drop */
        }
        int dx = (int)packet[1] - ((packet[0] << 4) & 0x100);
        int dy = (int)packet[2] - ((packet[0] << 3) & 0x100);

        pos_x += dx;
        pos_y -= dy;
        if (pos_x < 0) pos_x = 0;
        if (pos_y < 0) pos_y = 0;
        if (pos_x >= screen_w) pos_x = screen_w - 1;
        if (pos_y >= screen_h) pos_y = screen_h - 1;

        push_event(packet[0] & 0x07);
    }
}

void mouse_init(int w, int h) {
    screen_w = w;
    screen_h = h;
    pos_x = w / 2;
    pos_y = h / 2;

    wait_write();
    outb(PS2_CMD, 0xA8);        /* enable the auxiliary port */
    wait_write();
    outb(PS2_CMD, 0x20);        /* read controller config */
    wait_read();
    uint8_t cfg = inb(PS2_DATA);
    cfg |= 0x02;                /* enable IRQ 12 */
    cfg &= (uint8_t)~0x20;      /* enable the mouse clock */
    wait_write();
    outb(PS2_CMD, 0x60);        /* write controller config */
    wait_write();
    outb(PS2_DATA, cfg);

    mouse_write(0xF6);          /* defaults */
    mouse_read();
    mouse_write(0xF4);          /* start streaming */
    mouse_read();

    irq_register(12, mouse_irq);
}

bool mouse_poll(struct mouse_event *ev) {
    if (q_tail == q_head) {
        return false;
    }
    *ev = queue[q_tail];
    q_tail = (q_tail + 1) % QUEUE_SIZE;
    return true;
}
