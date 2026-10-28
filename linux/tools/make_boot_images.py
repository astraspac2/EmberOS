#!/usr/bin/env python3
"""Generate the EmberOS boot menu backgrounds (pure Python, no dependencies).

  linux/boot/splash-640x480.png     for the BIOS (syslinux) boot menu
  linux/boot/grub-background.png    for the UEFI (GRUB) boot menu

Both use the charcoal gradient and amber glow of the default wallpaper, with
the EmberOS logo.
"""
import os
import re
import struct
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import make_logo  # noqa: E402

FONT_H = os.path.join(HERE, "..", "..", "kernel", "font.h")
OUT_DIR = os.path.join(HERE, "..", "boot")

TOP = (34, 31, 30)
BOTTOM = (8, 7, 7)
GLOW = (150, 66, 14)


def load_font():
    src = open(FONT_H).read()
    rows = re.findall(r"\{((?:0x[0-9A-Fa-f]{2},?){8})\}", src)
    return [[int(v, 16) for v in r.split(",") if v] for r in rows]


def draw_logo(px, width, x0, y0, side):
    """Paint the logo (amber E, orange flame) with its own alpha."""
    pxu = 2.0 / side
    for j in range(side):
        v = (j + 0.5) / side * 2 - 1
        for i in range(side):
            r, g, b, a = make_logo.pixel((i + 0.5) / side * 2 - 1, v, pxu)
            if a > 0:
                o = ((y0 + j) * width + x0 + i) * 3
                for k, c in enumerate((r, g, b)):
                    px[o + k] = int(px[o + k] * (1 - a) + c * a)


def render(width, height, glow_center, glow_radius, text=None, text_scale=0,
           text_pos=(0, 0), text_color=(255, 150, 40), logo=None):
    font = load_font()
    px = bytearray(width * height * 3)
    gx, gy = glow_center
    r2 = glow_radius * glow_radius
    for y in range(height):
        t = y / (height - 1)
        base = [TOP[i] * (1 - t) + BOTTOM[i] * t for i in range(3)]
        dy2 = (y - gy) ** 2
        row = y * width * 3
        for x in range(width):
            d2 = (x - gx) ** 2 + dy2
            if d2 < r2:
                a = (1 - d2 / r2) ** 2
                c = [base[i] + GLOW[i] * a for i in range(3)]
            else:
                c = base
            o = row + x * 3
            px[o] = min(255, int(c[0]))
            px[o + 1] = min(255, int(c[1]))
            px[o + 2] = min(255, int(c[2]))

    if text:
        x0, y0 = text_pos
        for n, ch in enumerate(text):
            glyph = font[ord(ch) - 0x20]
            for gy_ in range(8):
                for gx_ in range(8):
                    if glyph[gy_] & (1 << gx_):
                        for sy in range(text_scale):
                            for sx in range(text_scale):
                                xx = x0 + n * 8 * text_scale + gx_ * text_scale + sx
                                yy = y0 + gy_ * text_scale + sy
                                o = (yy * width + xx) * 3
                                px[o:o + 3] = bytes(text_color)

    if logo:
        draw_logo(px, width, *logo)

    raw = bytearray()
    for y in range(height):
        raw.append(0)
        raw += px[y * width * 3:(y + 1) * width * 3]

    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")
    return png


def main():
    os.makedirs(OUT_DIR, exist_ok=True)

    text = "EmberOS"
    scale = 5
    width = len(text) * 8 * scale
    tx = (640 - width) // 2
    splash = render(640, 480, (110, 420), 330, text, scale, (tx, 26),
                    logo=(tx - 84, 12, 72))
    with open(os.path.join(OUT_DIR, "splash-640x480.png"), "wb") as f:
        f.write(splash)

    grub = render(1920, 1080, (170, 950), 620, logo=(900, 70, 120))
    with open(os.path.join(OUT_DIR, "grub-background.png"), "wb") as f:
        f.write(grub)

    print("wrote", os.path.normpath(OUT_DIR))


if __name__ == "__main__":
    main()
