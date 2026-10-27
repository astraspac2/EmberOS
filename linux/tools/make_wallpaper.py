#!/usr/bin/env python3
"""Generate the EmberOS wallpaper (pure Python, no dependencies).

A dark green gradient, a soft glow in the bottom-left corner (where the
custom desktop's launcher orb lives) and a faint "EmberOS" watermark drawn
with the same 8x8 font as the hobby kernel.
"""
import os
import re
import struct
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_H = os.path.join(HERE, "..", "..", "kernel", "font.h")
OUT = os.path.join(HERE, "..", "config", "includes.chroot", "usr", "share",
                   "backgrounds", "emberos", "emberos-wallpaper.png")

W, H = 1920, 1080
TOP = (12, 34, 24)
BOTTOM = (4, 10, 7)
GLOW = (30, 120, 70)
GLOW_CENTER = (170, H - 130)
GLOW_RADIUS = 620
TEXT = "EmberOS"
TEXT_SCALE = 20
TEXT_COLOR = (20, 52, 37)


def load_font():
    src = open(FONT_H).read()
    rows = re.findall(r"\{((?:0x[0-9A-Fa-f]{2},?){8})\}", src)
    return [[int(v, 16) for v in r.split(",") if v] for r in rows]


def main():
    font = load_font()
    assert len(font) == 95, len(font)

    px = bytearray(W * H * 3)
    gx, gy = GLOW_CENTER
    r2 = GLOW_RADIUS * GLOW_RADIUS
    for y in range(H):
        t = y / (H - 1)
        base = [TOP[i] * (1 - t) + BOTTOM[i] * t for i in range(3)]
        dy2 = (y - gy) ** 2
        row = y * W * 3
        for x in range(W):
            d2 = (x - gx) ** 2 + dy2
            if d2 < r2:
                a = 1 - d2 / r2
                a = a * a
                c = [base[i] + GLOW[i] * a for i in range(3)]
            else:
                c = base
            o = row + x * 3
            px[o] = min(255, int(c[0]))
            px[o + 1] = min(255, int(c[1]))
            px[o + 2] = min(255, int(c[2]))

    text_w = len(TEXT) * 8 * TEXT_SCALE
    x0 = (W - text_w) // 2
    y0 = H // 2 - 4 * TEXT_SCALE
    for n, ch in enumerate(TEXT):
        glyph = font[ord(ch) - 0x20]
        for gyi in range(8):
            for gxi in range(8):
                if glyph[gyi] & (1 << gxi):
                    for sy in range(TEXT_SCALE):
                        yy = y0 + gyi * TEXT_SCALE + sy
                        for sx in range(TEXT_SCALE):
                            xx = x0 + n * 8 * TEXT_SCALE + gxi * TEXT_SCALE + sx
                            o = (yy * W + xx) * 3
                            px[o:o + 3] = bytes(TEXT_COLOR)

    raw = bytearray()
    for y in range(H):
        raw.append(0)
        raw += px[y * W * 3:(y + 1) * W * 3]

    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "wb") as f:
        f.write(png)
    print(f"wrote {os.path.normpath(OUT)} ({len(png) / 1024:.0f} KiB)")


if __name__ == "__main__":
    main()
