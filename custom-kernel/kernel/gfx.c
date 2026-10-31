#include "gfx.h"
#include "font.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

static struct limine_framebuffer *fb;
static uint32_t *back;
static int W, H;
static int clip_x0, clip_y0, clip_x1, clip_y1;
static unsigned rsh, gsh, bsh;

static uint32_t pack(uint32_t rgb) {
    return (((rgb >> 16) & 0xFF) << rsh) | (((rgb >> 8) & 0xFF) << gsh) | ((rgb & 0xFF) << bsh);
}

bool gfx_init(struct limine_framebuffer *f) {
    if (f->bpp != 32) {
        return false;
    }
    fb = f;
    W = (int)f->width;
    H = (int)f->height;
    rsh = f->red_mask_shift;
    gsh = f->green_mask_shift;
    bsh = f->blue_mask_shift;

    size_t bytes = (size_t)W * H * 4;
    uint64_t phys = pmm_alloc_contiguous((bytes + PAGE_SIZE - 1) / PAGE_SIZE);
    if (!phys) {
        return false;
    }
    back = (uint32_t *)hhdm_phys_to_virt(phys);
    memset(back, 0, bytes);
    gfx_clear_clip();
    return true;
}

int gfx_width(void) { return W; }
int gfx_height(void) { return H; }

void gfx_set_clip(int x, int y, int w, int h) {
    clip_x0 = MAX(x, 0);
    clip_y0 = MAX(y, 0);
    clip_x1 = MIN(x + w, W);
    clip_y1 = MIN(y + h, H);
}

void gfx_clear_clip(void) {
    clip_x0 = 0;
    clip_y0 = 0;
    clip_x1 = W;
    clip_y1 = H;
}

void gfx_fill(int x, int y, int w, int h, uint32_t rgb) {
    int x0 = MAX(x, clip_x0), y0 = MAX(y, clip_y0);
    int x1 = MIN(x + w, clip_x1), y1 = MIN(y + h, clip_y1);
    if (x0 >= x1 || y0 >= y1) {
        return;
    }
    uint32_t p = pack(rgb);
    for (int j = y0; j < y1; j++) {
        uint32_t *dst = back + (size_t)j * W + x0;
        size_t n = (size_t)(x1 - x0);
        __asm__ volatile ("rep stosl" : "+D"(dst), "+c"(n) : "a"(p) : "memory");
    }
}

void gfx_rect(int x, int y, int w, int h, uint32_t rgb) {
    gfx_fill(x, y, w, 1, rgb);
    gfx_fill(x, y + h - 1, w, 1, rgb);
    gfx_fill(x, y, 1, h, rgb);
    gfx_fill(x + w - 1, y, 1, h, rgb);
}

void gfx_pixel(int x, int y, uint32_t rgb) {
    if (x >= clip_x0 && x < clip_x1 && y >= clip_y0 && y < clip_y1) {
        back[(size_t)y * W + x] = pack(rgb);
    }
}

static uint32_t mix(uint32_t dst, unsigned sr, unsigned sg, unsigned sb, int a) {
    unsigned dr = (dst >> rsh) & 0xFF;
    unsigned dg = (dst >> gsh) & 0xFF;
    unsigned db = (dst >> bsh) & 0xFF;
    unsigned nr = (sr * a + dr * (255 - a)) / 255;
    unsigned ng = (sg * a + dg * (255 - a)) / 255;
    unsigned nb = (sb * a + db * (255 - a)) / 255;
    return (nr << rsh) | (ng << gsh) | (nb << bsh);
}

void gfx_blend(int x, int y, int w, int h, uint32_t rgb, int alpha) {
    int x0 = MAX(x, clip_x0), y0 = MAX(y, clip_y0);
    int x1 = MIN(x + w, clip_x1), y1 = MIN(y + h, clip_y1);
    if (x0 >= x1 || y0 >= y1 || alpha <= 0) {
        return;
    }
    if (alpha > 255) {
        alpha = 255;
    }
    unsigned sr = (rgb >> 16) & 0xFF, sg = (rgb >> 8) & 0xFF, sb = rgb & 0xFF;
    for (int j = y0; j < y1; j++) {
        uint32_t *row = back + (size_t)j * W;
        for (int i = x0; i < x1; i++) {
            row[i] = mix(row[i], sr, sg, sb, alpha);
        }
    }
}

void gfx_blend_px(int x, int y, uint32_t rgb, int alpha) {
    if (x < clip_x0 || x >= clip_x1 || y < clip_y0 || y >= clip_y1 || alpha <= 0) {
        return;
    }
    if (alpha > 255) {
        alpha = 255;
    }
    uint32_t *p = back + (size_t)y * W + x;
    *p = mix(*p, (rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF, alpha);
}

void gfx_vgradient(int x, int y, int w, int h, uint32_t top, uint32_t bottom) {
    for (int j = 0; j < h; j++) {
        int t = h > 1 ? j * 255 / (h - 1) : 0;
        uint32_t r = (((top >> 16) & 0xFF) * (255 - t) + ((bottom >> 16) & 0xFF) * t) / 255;
        uint32_t g = (((top >> 8) & 0xFF) * (255 - t) + ((bottom >> 8) & 0xFF) * t) / 255;
        uint32_t b = ((top & 0xFF) * (255 - t) + (bottom & 0xFF) * t) / 255;
        gfx_fill(x, y + j, w, 1, (r << 16) | (g << 8) | b);
    }
}

void gfx_glow(int cx, int cy, int core, int glow, uint32_t rgb, int max_alpha) {
    int core2 = core * core;
    int glow2 = glow * glow;
    for (int y = cy - glow; y <= cy + glow; y++) {
        for (int x = cx - glow; x <= cx + glow; x++) {
            int dx = x - cx, dy = y - cy;
            int d2 = dx * dx + dy * dy;
            if (d2 <= core2) {
                gfx_blend_px(x, y, rgb, 255);
            } else if (d2 < glow2) {
                int a = (glow2 - d2) * max_alpha / (glow2 - core2);
                a = a * a / 255;
                gfx_blend_px(x, y, rgb, a);
            }
        }
    }
}

void gfx_char(int x, int y, char c, int scale, uint32_t rgb) {
    if (c < 0x20 || c > 0x7E) {
        return;
    }
    const uint8_t *glyph = font8x8[c - 0x20];
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            if (glyph[row] & (1u << col)) {
                gfx_fill(x + col * scale, y + row * scale, scale, scale, rgb);
            }
        }
    }
}

void gfx_text(int x, int y, const char *s, int scale, uint32_t rgb) {
    for (; *s; s++) {
        gfx_char(x, y, *s, scale, rgb);
        x += 8 * scale;
    }
}

int gfx_text_width(const char *s, int scale) {
    return (int)strlen(s) * 8 * scale;
}

void gfx_present(void) {
    for (int y = 0; y < H; y++) {
        memcpy((uint8_t *)fb->address + (size_t)y * fb->pitch, back + (size_t)y * W, (size_t)W * 4);
    }
}
