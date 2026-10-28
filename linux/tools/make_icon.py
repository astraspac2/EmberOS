#!/usr/bin/env python3
"""Generate the EmberOS installer icon: a glowing mint-green orb on a
transparent background (pure Python, no dependencies)."""
import os
import struct
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "config", "includes.chroot", "usr", "share",
                   "pixmaps", "emberos-install.png")

SIZE = 256
C = SIZE / 2
CORE = 58
GLOW = 124
ACCENT = (61, 220, 132)
HIGHLIGHT = (200, 255, 224)


def main():
    raw = bytearray()
    for y in range(SIZE):
        raw.append(0)
        for x in range(SIZE):
            dx, dy = x + 0.5 - C, y + 0.5 - C
            d = (dx * dx + dy * dy) ** 0.5
            if d <= CORE:
                # lit from the upper left: blend towards a highlight
                hx, hy = dx + 22, dy + 22
                h = max(0.0, 1 - (hx * hx + hy * hy) ** 0.5 / (CORE * 1.1))
                r, g, b = [int(ACCENT[i] * (1 - h) + HIGHLIGHT[i] * h) for i in range(3)]
                edge = min(1.0, (CORE - d) / 1.5)  # blend the rim into the glow, no visible ring
                raw += bytes((r, g, b, int(200 + 55 * edge)))
            elif d < GLOW:
                a = (1 - (d - CORE) / (GLOW - CORE)) ** 2
                raw += bytes((ACCENT[0], ACCENT[1], ACCENT[2], int(200 * a)))
            else:
                raw += bytes((0, 0, 0, 0))

    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", SIZE, SIZE, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "wb") as f:
        f.write(png)
    print(f"wrote {os.path.normpath(OUT)} ({len(png) / 1024:.1f} KiB)")


if __name__ == "__main__":
    main()
