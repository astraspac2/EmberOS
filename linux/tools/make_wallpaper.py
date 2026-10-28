#!/usr/bin/env python3
"""Generate the EmberOS wallpaper (pure Python, no dependencies).

  make_wallpaper.py --sheet     preview every style in logo-examples/wallpapers/
  make_wallpaper.py [--all|N]   write the wallpapers the chooser app offers;
                                style 1 is also the default (emberos-wallpaper.png)

Each style is a dark vertical gradient, a soft glow, optional pixel embers
drifting up (like the pixel flame of the logo) and a faint logo watermark.
"""
import os
import random
import struct
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import make_logo  # noqa: E402

OUT = os.path.join(HERE, "..", "config", "includes.chroot", "usr", "share",
                   "backgrounds", "emberos", "emberos-wallpaper.png")
SHEET_DIR = os.path.join(HERE, "..", "..", "logo-examples", "wallpapers")
DEFAULT_STYLE = "1"

AMBER = (255, 150, 40)

STYLES = {
    "1": dict(name="charcoal", top=(34, 31, 30), bottom=(9, 8, 8),
              glow=(150, 66, 14), center=(0.14, 0.9), radius=0.42, wm=(52, 44, 38), embers=0),
    "2": dict(name="ember-dusk", top=(44, 15, 10), bottom=(8, 3, 3),
              glow=(205, 82, 20), center=(0.5, 1.12), radius=0.62, wm=None, embers=70),
    "3": dict(name="midnight", top=(15, 22, 38), bottom=(4, 6, 12),
              glow=(170, 80, 20), center=(0.12, 0.92), radius=0.34, wm=(32, 40, 60), embers=0),
    "4": dict(name="black-embers", top=(11, 8, 7), bottom=(2, 2, 2),
              glow=(110, 46, 10), center=(0.5, 1.15), radius=0.55, wm=(30, 20, 14), embers=120),
    "5": dict(name="green-embers", top=(12, 34, 24), bottom=(4, 10, 7),
              glow=(30, 120, 70), center=(0.14, 0.9), radius=0.32, wm=(22, 58, 41), embers=60),
}


def render(style, w, h):
    top, bottom = style["top"], style["bottom"]
    gx, gy = style["center"][0] * w, style["center"][1] * h
    r = style["radius"] * w
    r2 = r * r
    glow = style["glow"]

    px = bytearray(w * h * 3)
    for y in range(h):
        t = y / (h - 1)
        base = [top[i] * (1 - t) + bottom[i] * t for i in range(3)]
        dy2 = (y - gy) ** 2
        row = y * w * 3
        for x in range(w):
            d2 = (x - gx) ** 2 + dy2
            if d2 < r2:
                a = 1 - d2 / r2
                a *= a
                c = [base[i] + glow[i] * a for i in range(3)]
            else:
                c = base
            o = row + x * 3
            px[o] = min(255, int(c[0]))
            px[o + 1] = min(255, int(c[1]))
            px[o + 2] = min(255, int(c[2]))

    # faint logo in the middle
    if style["wm"]:
        side = int(h * 0.62)
        x0, y0 = (w - side) // 2, (h - side) // 2 - int(h * 0.03)
        pxu = 2.0 / side
        for j in range(side):
            v = (j + 0.5) / side * 2 - 1
            for i in range(side):
                a = make_logo.pixel((i + 0.5) / side * 2 - 1, v, pxu)[3]
                if a > 0:
                    o = ((y0 + j) * w + x0 + i) * 3
                    for k in range(3):
                        px[o + k] = int(px[o + k] * (1 - a) + style["wm"][k] * a)

    # pixel embers, thicker near the bottom
    rnd = random.Random(7)
    cell = max(2, w // 200)
    for _ in range(style["embers"]):
        ex = rnd.randrange(w)
        ey = int(h * (1 - rnd.random() ** 1.8))
        size = cell * rnd.choice((1, 1, 1, 2))
        strength = 0.25 + 0.75 * (ey / h)
        for dy in range(size):
            for dx in range(size):
                xx, yy = ex + dx, ey + dy
                if 0 <= xx < w and 0 <= yy < h:
                    o = (yy * w + xx) * 3
                    for k in range(3):
                        px[o + k] = int(px[o + k] * (1 - strength) + AMBER[k] * strength)
    return px


def png(w, h, px):
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        raw += px[y * w * 3:(y + 1) * w * 3]

    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b""))


def sheet():
    w, h, pad = 640, 360, 16
    cols, rows = 3, 2
    sw, sh = pad + cols * (w + pad), pad + rows * (h + pad)
    out = bytearray(bytes((13, 13, 13)) * (sw * sh))
    for n, (key, style) in enumerate(STYLES.items()):
        px = render(style, w, h)
        ox, oy = pad + (n % cols) * (w + pad), pad + (n // cols) * (h + pad)
        for y in range(h):
            o = ((oy + y) * sw + ox) * 3
            out[o:o + w * 3] = px[y * w * 3:(y + 1) * w * 3]
        print("drew", key, style["name"])
    os.makedirs(SHEET_DIR, exist_ok=True)
    path = os.path.join(SHEET_DIR, "sheet.png")
    with open(path, "wb") as f:
        f.write(png(sw, sh, out))
    print("wrote", os.path.normpath(path))


def main():
    arg = sys.argv[1] if len(sys.argv) > 1 else "--all"
    if arg == "--sheet":
        sheet()
        return
    w, h = 1920, 1080
    out_dir = os.path.dirname(OUT)
    os.makedirs(out_dir, exist_ok=True)
    for key, style in STYLES.items():
        if arg not in ("--all", key):
            continue
        data = png(w, h, render(style, w, h))
        targets = [os.path.join(out_dir, f"{key}-{style['name']}.png")]
        if key == DEFAULT_STYLE:
            targets.append(OUT)  # the wallpaper every new user starts with
        for path in targets:
            with open(path, "wb") as f:
                f.write(data)
            print(f"wrote {os.path.normpath(path)} ({len(data) / 1024:.0f} KiB)")


if __name__ == "__main__":
    main()
