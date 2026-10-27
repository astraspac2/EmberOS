#include "kernel.h"
#include "font.h"

#define SCALE   2
#define CELL_W  (8 * SCALE)
#define CELL_H  (8 * SCALE)

/* Row 0 is the status bar; text scrolls in rows 1..rows-1. */
static struct limine_framebuffer *fb;
static size_t cols, rows;
static size_t cur_x, cur_y = 1;
static uint32_t fg_rgb = COLOR_FG;

static uint32_t pack(uint32_t rgb) {
    uint32_t r = (rgb >> 16) & 0xFF;
    uint32_t g = (rgb >> 8) & 0xFF;
    uint32_t b = rgb & 0xFF;
    return (r << fb->red_mask_shift) | (g << fb->green_mask_shift) | (b << fb->blue_mask_shift);
}

static uint32_t *pixel_row(size_t y) {
    return (uint32_t *)((uint8_t *)fb->address + y * fb->pitch);
}

static void draw_glyph(size_t col, size_t row, char c, uint32_t fg, uint32_t bg) {
    const uint8_t *glyph = font8x8[(c >= 0x20 && c <= 0x7E) ? c - 0x20 : 0];
    size_t px = col * CELL_W;
    size_t py = row * CELL_H;
    uint32_t pfg = pack(fg);
    uint32_t pbg = pack(bg);

    for (size_t gy = 0; gy < 8; gy++) {
        for (size_t sy = 0; sy < SCALE; sy++) {
            uint32_t *p = pixel_row(py + gy * SCALE + sy) + px;
            for (size_t gx = 0; gx < 8; gx++) {
                uint32_t color = (glyph[gy] >> gx) & 1 ? pfg : pbg;
                for (size_t sx = 0; sx < SCALE; sx++) {
                    *p++ = color;
                }
            }
        }
    }
}

static void fill_cell_rows(size_t row_start, size_t row_count, uint32_t rgb) {
    uint32_t px = pack(rgb);
    for (size_t y = row_start * CELL_H; y < (row_start + row_count) * CELL_H; y++) {
        uint32_t *p = pixel_row(y);
        for (size_t x = 0; x < fb->width; x++) {
            p[x] = px;
        }
    }
}

static void draw_cursor(bool on) {
    uint32_t px = pack(on ? COLOR_ACCENT : COLOR_BG);
    size_t x0 = cur_x * CELL_W;
    size_t y0 = cur_y * CELL_H + CELL_H - SCALE;
    for (size_t y = y0; y < y0 + SCALE; y++) {
        uint32_t *p = pixel_row(y);
        for (size_t x = x0; x < x0 + CELL_W; x++) {
            p[x] = px;
        }
    }
}

static void scroll(void) {
    size_t line_bytes = fb->width * 4;
    size_t first = CELL_H;               /* top of row 1 */
    size_t last = (rows - 1) * CELL_H;   /* pixel lines to move */
    for (size_t y = first; y < last; y++) {
        memcpy(pixel_row(y), pixel_row(y + CELL_H), line_bytes);
    }
    fill_cell_rows(rows - 1, 1, COLOR_BG);
}

void console_init(struct limine_framebuffer *framebuffer) {
    fb = framebuffer;
    cols = fb->width / CELL_W;
    rows = fb->height / CELL_H;
    console_clear();
}

void console_clear(void) {
    fill_cell_rows(0, rows, COLOR_BG);
    cur_x = 0;
    cur_y = 1;
    draw_cursor(true);
}

void console_set_fg(uint32_t rgb) {
    fg_rgb = rgb;
}

void console_status(const char *text) {
    for (size_t c = 0; c < cols; c++) {
        char ch = (*text) ? *text++ : ' ';
        draw_glyph(c, 0, ch, COLOR_ACCENT, COLOR_BAR);
    }
}

static void newline(void) {
    cur_x = 0;
    if (cur_y + 1 >= rows) {
        scroll();
    } else {
        cur_y++;
    }
}

void console_putc(char c) {
    draw_cursor(false);

    if (c == '\n') {
        newline();
    } else if (c == '\r') {
        cur_x = 0;
    } else if (c == '\b') {
        if (cur_x > 0) {
            cur_x--;
        } else if (cur_y > 1) {
            cur_y--;
            cur_x = cols - 1;
        }
        draw_glyph(cur_x, cur_y, ' ', fg_rgb, COLOR_BG);
    } else if (c == '\t') {
        size_t next = (cur_x + 4) & ~(size_t)3;
        while (cur_x < next && cur_x < cols) {
            draw_glyph(cur_x++, cur_y, ' ', fg_rgb, COLOR_BG);
        }
        if (cur_x >= cols) {
            newline();
        }
    } else {
        draw_glyph(cur_x, cur_y, c, fg_rgb, COLOR_BG);
        if (++cur_x >= cols) {
            newline();
        }
    }

    draw_cursor(true);
}

/* ---- printf ---- */

typedef void (*out_fn)(char c, void *ctx);

static void format(out_fn out, void *ctx, const char *fmt, va_list ap) {
    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            out(*fmt, ctx);
            continue;
        }
        fmt++;

        bool zero = false;
        if (*fmt == '0') {
            zero = true;
            fmt++;
        }
        int width = 0;
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }
        int longs = 0;
        while (*fmt == 'l') {
            longs++;
            fmt++;
        }

        char tmp[24];
        const char *str = tmp;
        size_t len = 0;
        bool negative = false;

        switch (*fmt) {
        case 'c':
            tmp[0] = (char)va_arg(ap, int);
            len = 1;
            break;
        case 's':
            str = va_arg(ap, const char *);
            if (!str) {
                str = "(null)";
            }
            len = strlen(str);
            break;
        case 'd':
        case 'i':
        case 'u':
        case 'x':
        case 'X':
        case 'p': {
            uint64_t v;
            unsigned base = (*fmt == 'd' || *fmt == 'i' || *fmt == 'u') ? 10 : 16;
            if (*fmt == 'p') {
                v = (uint64_t)va_arg(ap, void *);
                zero = true;
                width = 16;
            } else if (*fmt == 'd' || *fmt == 'i') {
                int64_t s = longs ? va_arg(ap, int64_t) : va_arg(ap, int);
                negative = s < 0;
                v = negative ? (uint64_t)(-s) : (uint64_t)s;
            } else {
                v = longs ? va_arg(ap, uint64_t) : va_arg(ap, unsigned);
            }
            const char *digits = (*fmt == 'X') ? "0123456789ABCDEF" : "0123456789abcdef";
            char rev[24];
            size_t n = 0;
            do {
                rev[n++] = digits[v % base];
                v /= base;
            } while (v);
            if (negative) {
                rev[n++] = '-';
            }
            for (size_t i = 0; i < n; i++) {
                tmp[i] = rev[n - 1 - i];
            }
            len = n;
            break;
        }
        case '%':
            tmp[0] = '%';
            len = 1;
            break;
        default:
            tmp[0] = '?';
            len = 1;
            break;
        }

        for (size_t i = len; i < (size_t)width; i++) {
            out(zero ? '0' : ' ', ctx);
        }
        for (size_t i = 0; i < len; i++) {
            out(str[i], ctx);
        }
    }
}

static void out_console(char c, void *ctx) {
    (void)ctx;
    console_putc(c);
}

void kvprintf(const char *fmt, va_list ap) {
    format(out_console, NULL, fmt, ap);
}

void kprintf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
}

struct buf_ctx {
    char *buf;
    size_t size;
    size_t len;
};

static void out_buf(char c, void *ctx) {
    struct buf_ctx *b = ctx;
    if (b->len + 1 < b->size) {
        b->buf[b->len] = c;
    }
    b->len++;
}

int ksnprintf(char *buf, size_t size, const char *fmt, ...) {
    struct buf_ctx b = { buf, size, 0 };
    va_list ap;
    va_start(ap, fmt);
    format(out_buf, &b, fmt, ap);
    va_end(ap);
    if (size) {
        buf[b.len < size ? b.len : size - 1] = '\0';
    }
    return (int)b.len;
}

void panic(const char *fmt, ...) {
    __asm__ volatile ("cli");
    console_set_fg(COLOR_ERR);
    kprintf("\n*** KERNEL PANIC ***\n");
    va_list ap;
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
    kprintf("\nSystem halted.\n");
    cpu_halt_forever();
}
