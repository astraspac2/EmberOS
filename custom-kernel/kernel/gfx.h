#ifndef EMBER_GFX_H
#define EMBER_GFX_H

#include "kernel.h"

/* Double-buffered 32-bit graphics. Colors are 0xRRGGBB. All drawing is
 * clipped to the current clip rectangle (the whole screen by default). */

bool gfx_init(struct limine_framebuffer *fb);
int gfx_width(void);
int gfx_height(void);

void gfx_set_clip(int x, int y, int w, int h);
void gfx_clear_clip(void);

void gfx_fill(int x, int y, int w, int h, uint32_t rgb);
void gfx_rect(int x, int y, int w, int h, uint32_t rgb);              /* 1px outline */
void gfx_blend(int x, int y, int w, int h, uint32_t rgb, int alpha);  /* alpha 0..255 */
void gfx_blend_px(int x, int y, uint32_t rgb, int alpha);
void gfx_pixel(int x, int y, uint32_t rgb);
void gfx_vgradient(int x, int y, int w, int h, uint32_t top, uint32_t bottom);
void gfx_glow(int cx, int cy, int core, int glow, uint32_t rgb, int max_alpha);

void gfx_char(int x, int y, char c, int scale, uint32_t rgb);
void gfx_text(int x, int y, const char *s, int scale, uint32_t rgb);
int gfx_text_width(const char *s, int scale);

void gfx_present(void);

#endif
