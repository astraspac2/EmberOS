#!/usr/bin/env python3
"""Generate the EmberOS logo: an angular "E" with a pixel flame burning in its
open side (pure Python, no dependencies). Writes
  - the PNG used for the installer icon, the menu button and the installer logo
  - the block-character art that fastfetch shows."""
import math
import os
import struct
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
INC = os.path.join(HERE, "..", "config", "includes.chroot")
PNG_OUT = os.path.join(INC, "usr", "share", "pixmaps", "emberos-install.png")
TXT_OUT = os.path.join(INC, "usr", "share", "emberos", "logo.txt")

SIZE = 256
AMBER = (255, 150, 40)
FLAME = (255, 128, 32)
CORE = (255, 222, 140)

# E, in unit coordinates (-1..1); shifted left to leave room for the flame
SHIFT, SCALE = 0.2, 0.86
TOP = [(-0.5, -0.66), (0.62, -0.66), (0.3, -0.4), (-0.26, -0.4)]
MID = [(-0.26, -0.13), (0.42, -0.13), (0.22, 0.13), (-0.26, 0.13)]
BOT = [(-0.5, 0.66), (0.62, 0.66), (0.3, 0.4), (-0.26, 0.4)]
STEM = [(-0.5, -0.66), (-0.26, -0.4), (-0.26, 0.4), (-0.5, 0.66)]
E_SHAPES = [[(x * SCALE - SHIFT, y * SCALE) for x, y in s] for s in (STEM, TOP, MID, BOT)]

# pixel flame: grid cells inside a box
FX0, FY0, FX1, FY1 = 0.22, -0.5, 0.86, 0.5
FW, FH = 10, 14


def poly_sd(x, y, pts):
    d = (x - pts[0][0]) ** 2 + (y - pts[0][1]) ** 2
    s = 1.0
    j = len(pts) - 1
    for i in range(len(pts)):
        ax, ay = pts[i]
        bx, by = pts[j]
        ex, ey = bx - ax, by - ay
        wx, wy = x - ax, y - ay
        t = max(0.0, min(1.0, (wx * ex + wy * ey) / (ex * ex + ey * ey)))
        qx, qy = wx - ex * t, wy - ey * t
        d = min(d, qx * qx + qy * qy)
        c = (y >= ay, y < by, ex * wy > ey * wx)
        if all(c) or not any(c):
            s = -s
        j = i
    return s * d ** 0.5


def flame_sd(x, y):
    """Teardrop with its tip leaning right."""
    cy, r, tip = 0.2, 0.55, -0.78
    if y >= cy:
        return math.hypot(x, y - cy) - r
    s = (cy - y) / (cy - tip)
    if s >= 1:
        return math.hypot(x - 0.2, y - tip)
    return abs(x - 0.2 * s * s) - r * (1 - s) ** 1.5


def flame_cell(i, j, core):
    u = (i + 0.5) / FW * 2 - 1
    v = (j + 0.5) / FH * 2 - 1
    x, y = u * 0.8 + 0.1, v * 0.8 - 0.02
    if core:
        k = 0.5
        return flame_sd(x / k, (y - 0.3) / k) * k < 0
    return flame_sd(x, y) < 0


def flame_at(u, v):
    """'c' (core), 'f' (flame) or None for the point (u, v)."""
    if not (FX0 <= u < FX1 and FY0 <= v < FY1):
        return None
    i = int((u - FX0) / (FX1 - FX0) * FW)
    j = int((v - FY0) / (FY1 - FY0) * FH)
    if flame_cell(i, j, True):
        return "c"
    return "f" if flame_cell(i, j, False) else None


def e_dist(u, v):
    return min(poly_sd(u, v, s) for s in E_SHAPES)


def pixel(u, v, px):
    f = flame_at(u, v)
    if f:
        return (CORE if f == "c" else FLAME) + (1.0,)
    a = max(0.0, min(1.0, 0.5 - e_dist(u, v) / px))
    return AMBER + (a,) if a > 0 else (0, 0, 0, 0.0)


def render(size):
    px = 2.0 / size
    return [pixel((i + 0.5) / size * 2 - 1, (j + 0.5) / size * 2 - 1, px)
            for j in range(size) for i in range(size)]


def png_bytes(w, h, pix):
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        for x in range(w):
            r, g, b, a = pix[y * w + x]
            raw += bytes((int(r), int(g), int(b), int(a * 255)))

    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b""))


def block_art(cols=32):
    """fastfetch logo: half-block characters, $1 = E, $2 = flame, $3 = core."""
    rows = cols // 2
    lines = []
    for r in range(rows):
        line = ""
        last = None
        for c in range(cols):
            u = (c + 0.5) / cols * 2 - 1
            halves = []
            for sub in (0, 1):
                v = (r * 2 + sub + 0.5) / (rows * 2) * 2 - 1
                f = flame_at(u, v)
                if f:
                    halves.append(f)
                elif e_dist(u, v) < 0:
                    halves.append("e")
                else:
                    halves.append(None)
            top, bot = halves
            if not top and not bot:
                line += " "
                continue
            cls = top or bot
            if top and bot and top != bot:
                cls = "c" if "c" in (top, bot) else ("f" if "f" in (top, bot) else "e")
            mark = {"e": "$1", "f": "$2", "c": "$3"}[cls]
            ch = "█" if top and bot else ("▀" if top else "▄")
            if mark != last:
                line += mark
                last = mark
            line += ch
        lines.append(line)
    while lines and not lines[0].strip():
        lines.pop(0)
    while lines and not lines[-1].strip():
        lines.pop()
    return "\n".join(lines) + "\n"


def main():
    pix = render(SIZE)
    os.makedirs(os.path.dirname(PNG_OUT), exist_ok=True)
    with open(PNG_OUT, "wb") as f:
        f.write(png_bytes(SIZE, SIZE, pix))
    print("wrote", os.path.normpath(PNG_OUT))
    os.makedirs(os.path.dirname(TXT_OUT), exist_ok=True)
    with open(TXT_OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write(block_art())
    print("wrote", os.path.normpath(TXT_OUT))


if __name__ == "__main__":
    main()
