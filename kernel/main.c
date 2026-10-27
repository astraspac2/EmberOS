#include <stddef.h>
#include <stdint.h>

#include "limine.h"
#include "font.h"

#define COLOR_BG     0x0D0A08
#define COLOR_EMBER  0xFF6A1A
#define COLOR_DIM    0x8A5A3C

__attribute__((used, section(".limine_requests")))
static volatile uint64_t base_revision[3] = LIMINE_BASE_REVISION(3);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0,
};

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t requests_start[4] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t requests_end[2] = LIMINE_REQUESTS_END_MARKER;

static struct limine_framebuffer *fb;

static void halt(void) {
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

/* Pack 0xRRGGBB into whatever pixel layout the framebuffer uses. */
static uint32_t pack(uint32_t rgb) {
    uint32_t r = (rgb >> 16) & 0xFF;
    uint32_t g = (rgb >> 8) & 0xFF;
    uint32_t b = rgb & 0xFF;
    return (r << fb->red_mask_shift) | (g << fb->green_mask_shift) | (b << fb->blue_mask_shift);
}

static void fill_rect(size_t x, size_t y, size_t w, size_t h, uint32_t rgb) {
    uint32_t px = pack(rgb);
    for (size_t j = 0; j < h && y + j < fb->height; j++) {
        uint32_t *row = (uint32_t *)((uint8_t *)fb->address + (y + j) * fb->pitch);
        for (size_t i = 0; i < w && x + i < fb->width; i++) {
            row[x + i] = px;
        }
    }
}

static void draw_char(size_t x, size_t y, char c, size_t scale, uint32_t rgb) {
    if (c < 0x20 || c > 0x7E) {
        return;
    }
    const uint8_t *glyph = font8x8[c - 0x20];
    for (size_t row = 0; row < 8; row++) {
        for (size_t col = 0; col < 8; col++) {
            if (glyph[row] & (1u << col)) {
                fill_rect(x + col * scale, y + row * scale, scale, scale, rgb);
            }
        }
    }
}

static void draw_text(size_t x, size_t y, const char *s, size_t scale, uint32_t rgb) {
    for (; *s; s++) {
        draw_char(x, y, *s, scale, rgb);
        x += 8 * scale;
    }
}

static size_t text_width(const char *s, size_t scale) {
    size_t n = 0;
    while (s[n]) {
        n++;
    }
    return n * 8 * scale;
}

static void draw_centered(size_t y, const char *s, size_t scale, uint32_t rgb) {
    draw_text((fb->width - text_width(s, scale)) / 2, y, s, scale, rgb);
}

void kmain(void) {
    if (!LIMINE_BASE_REVISION_SUPPORTED(base_revision)) {
        halt();
    }

    if (framebuffer_request.response == NULL ||
        framebuffer_request.response->framebuffer_count < 1) {
        halt();
    }

    fb = framebuffer_request.response->framebuffers[0];
    if (fb->bpp != 32) {
        halt();
    }

    fill_rect(0, 0, fb->width, fb->height, COLOR_BG);

    size_t mid = fb->height / 2;
    draw_centered(mid - 40, "Hello from EmberOS", 4, COLOR_EMBER);
    draw_centered(mid + 20, "kernel v0.1 - step 1: boot", 2, COLOR_DIM);

    halt();
}
