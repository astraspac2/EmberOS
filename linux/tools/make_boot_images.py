#!/usr/bin/env python3
"""Generate the EmberOS boot menu backgrounds (pure Python, no dependencies).

  linux/boot/splash-640x480.png     for the BIOS (syslinux) boot menu
  linux/boot/grub-background.png    for the UEFI (GRUB) boot menu

Both use the same dark green gradient and soft glow as the wallpaper.
"""
import os
import re
import struct
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_H = os.path.join(HERE, "..", "..", "kernel", "font.h")
OUT_DIR = os.path.join(HERE, "..", "boot")

TOP = (12, 34, 24)
BOTTOM = (3, 8, 6)
GLOW = (30, 120, 70)


def load_font():
    src = open(FONT_H).read()
    rows = re.findall(r"\{((?:0x[0-9A-Fa-f]{2},?){8})\}", src)
    return [[int(v, 16) for v in r.split(",") if v] for r in rows]


def render(width, height, glow_center, glow_radius, text=None, text_scale=0,
           text_pos=(0, 0), text_color=(61, 220, 132)):
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
    splash = render(640, 480, (110, 420), 330, text, scale, ((640 - width) // 2, 26))
    with open(os.path.join(OUT_DIR, "splash-640x480.png"), "wb") as f:
        f.write(splash)

    grub = render(1920, 1080, (170, 950), 620)
    with open(os.path.join(OUT_DIR, "grub-background.png"), "wb") as f:
        f.write(grub)

    print("wrote", os.path.normpath(OUT_DIR))


if __name__ == "__main__":
    main()
