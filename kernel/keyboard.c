#include "kernel.h"

/* PS/2 keyboard, scancode set 1, US layout. */

#define KBD_DATA   0x60
#define KBD_STATUS 0x64
#define BUF_SIZE   256

static const char map_plain[58] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ',
};

static const char map_shift[58] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|',
    'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ',
};

static volatile char buffer[BUF_SIZE];
static volatile size_t head, tail;
static bool shift, caps, extended;

static void push(char c) {
    size_t next = (head + 1) % BUF_SIZE;
    if (next != tail) {
        buffer[head] = c;
        head = next;
    }
}

static void keyboard_irq(void) {
    uint8_t sc = inb(KBD_DATA);

    if (sc == 0xE0) {
        extended = true;
        return;
    }
    if (extended) {  /* arrows, home/end, etc. come later */
        extended = false;
        return;
    }

    bool release = sc & 0x80;
    uint8_t code = sc & 0x7F;

    if (code == 0x2A || code == 0x36) {
        shift = !release;
        return;
    }
    if (release) {
        return;
    }
    if (code == 0x3A) {
        caps = !caps;
        return;
    }
    if (code >= sizeof(map_plain)) {
        return;
    }

    char c = shift ? map_shift[code] : map_plain[code];
    if (caps && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))) {
        c ^= 0x20;
    }
    if (c) {
        push(c);
    }
}

void keyboard_init(void) {
    while (inb(KBD_STATUS) & 1) {  /* drain stale bytes */
        inb(KBD_DATA);
    }
    irq_register(1, keyboard_irq);
}

int keyboard_getchar(void) {
    if (tail == head) {
        return -1;
    }
    char c = buffer[tail];
    tail = (tail + 1) % BUF_SIZE;
    return (unsigned char)c;
}
