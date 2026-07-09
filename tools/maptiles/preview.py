#!/usr/bin/env python3
"""
Render baked .bin tiles back to PNGs, so we can eyeball the map quality of the
compact format before writing the on-device rasteriser. This reads the exact
same binary the cluster firmware will read, so what you see here is what the
cluster has to draw.

Outputs:
  preview_area.png    the whole baked area, tiles laid in a grid
  preview_device.png  an 800x800 round viewport (the cluster's screen) centred
                      on the area, with a speed-in-a-circle overlay

Usage:  preview.py <tiles_dir> [--center LAT,LON] [--px-per-tile 256]
"""
import argparse
import json
import math
import os
import struct

from PIL import Image, ImageDraw, ImageFont

from styles import STYLE_TABLE, BACKGROUND, S_WATER

EXTENT = 4096
MAGIC = b"ZMT0"


def read_tile(path):
    with open(path, "rb") as f:
        data = f.read()
    assert data[:4] == MAGIC, f"bad magic in {path}"
    z, tx, ty, n = struct.unpack_from("<HIIH", data, 4)
    off = 4 + struct.calcsize("<HIIH")
    feats = []
    for _ in range(n):
        ftype, style, npts = struct.unpack_from("<BBH", data, off)
        off += 4
        pts = [struct.unpack_from("<HH", data, off + i * 4) for i in range(npts)]
        off += npts * 4
        feats.append((ftype, style, pts))
    return (tx, ty), feats


def load_all(tiles_dir, z):
    out = {}
    zdir = os.path.join(tiles_dir, str(z))
    for xd in os.listdir(zdir):
        for yf in os.listdir(os.path.join(zdir, xd)):
            if yf.endswith(".bin"):
                key, feats = read_tile(os.path.join(zdir, xd, yf))
                out[key] = feats
    return out


def draw_feats(draw, feats, to_screen, ppt):
    """Draw one tile's features. `to_screen(lx,ly)->(sx,sy)`; ppt = px/tile."""
    scale = ppt / EXTENT
    # Fills first (water under roads), then roads minor->major.
    order = sorted(feats, key=lambda f: (0 if f[0] == 1 else 1, f[1]))
    for ftype, style, pts in order:
        rgb, width = STYLE_TABLE.get(style, ((255, 0, 255), 1))
        scr = [to_screen(x, y) for x, y in pts]
        if ftype == 1:
            draw.polygon(scr, fill=rgb)
        else:
            w = max(1, round(width * ppt / 256))
            draw.line(scr, fill=rgb, width=w, joint="curve")


def render_area(tiles, minx, miny, maxx, maxy, ppt, out):
    W = (maxx - minx + 1) * ppt
    H = (maxy - miny + 1) * ppt
    img = Image.new("RGB", (W, H), BACKGROUND)
    d = ImageDraw.Draw(img)
    for (tx, ty), feats in tiles.items():
        ox = (tx - minx) * ppt
        oy = (ty - miny) * ppt
        draw_feats(d, feats, lambda x, y: (ox + x * ppt / EXTENT, oy + y * ppt / EXTENT), ppt)
    img.save(out)
    return W, H


def render_device(tiles, center_tx, center_ty, ppt, speed_mph, out, size=800):
    img = Image.new("RGB", (size, size), BACKGROUND)
    d = ImageDraw.Draw(img)
    cx = cy = size / 2

    def to_screen_for(tx, ty):
        def f(lx, ly):
            gx = tx + lx / EXTENT
            gy = ty + ly / EXTENT
            return (cx + (gx - center_tx) * ppt, cy + (gy - center_ty) * ppt)
        return f

    for (tx, ty), feats in tiles.items():
        draw_feats(d, feats, to_screen_for(tx, ty), ppt)

    # Round bezel: black out everything outside the display circle.
    mask = Image.new("L", (size, size), 0)
    ImageDraw.Draw(mask).ellipse([0, 0, size, size], fill=255)
    round_img = Image.new("RGB", (size, size), (0, 0, 0))
    round_img.paste(img, (0, 0), mask)
    d = ImageDraw.Draw(round_img)

    # Heading marker + speed-in-a-circle overlay.
    d.ellipse([cx - 6, cy - 6, cx + 6, cy + 6], fill=(0xFF, 0x40, 0x40))
    r = 150
    dial = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    ImageDraw.Draw(dial).ellipse([cx - r, cy - r, cx + r, cy + r],
                                 fill=(16, 17, 22, 205), outline=(0xE6, 0xB8, 0x4B), width=4)
    round_img.paste(Image.alpha_composite(round_img.convert("RGBA"), dial).convert("RGB"), (0, 0))
    d = ImageDraw.Draw(round_img)
    big = _font(150)
    small = _font(40)
    s = str(speed_mph)
    _centered(d, s, cx, cy - 20, big, (0xF5, 0xF5, 0xF5))
    _centered(d, "MPH", cx, cy + 78, small, (0xB9, 0xB9, 0xB9))
    round_img.save(out)


def _font(sz):
    for p in ("/System/Library/Fonts/SFNSRounded.ttf",
              "/System/Library/Fonts/Helvetica.ttc",
              "/Library/Fonts/Arial.ttf"):
        if os.path.exists(p):
            try:
                return ImageFont.truetype(p, sz)
            except OSError:
                pass
    return ImageFont.load_default()


def _centered(d, text, cx, cy, font, fill):
    l, t, r, b = d.textbbox((0, 0), text, font=font)
    d.text((cx - (r - l) / 2 - l, cy - (b - t) / 2 - t), text, font=font, fill=fill)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("tiles_dir")
    ap.add_argument("--center", help="LAT,LON for the device mock; default = area centre")
    ap.add_argument("--px-per-tile", type=int, default=256)
    ap.add_argument("--speed", type=int, default=52)
    args = ap.parse_args()

    man = json.load(open(os.path.join(args.tiles_dir, "manifest.json")))
    z = man["zoom"]
    tiles = load_all(args.tiles_dir, z)
    minx, miny, maxx, maxy = man["min_x"], man["min_y"], man["max_x"], man["max_y"]

    aw, ah = render_area(tiles, minx, miny, maxx, maxy, args.px_per_tile,
                         os.path.join(args.tiles_dir, "preview_area.png"))

    if args.center:
        lat, lon = (float(v) for v in args.center.split(","))
        n = 2 ** z
        ctx = (lon + 180.0) / 360.0 * n
        cty = (1.0 - math.asinh(math.tan(math.radians(lat))) / math.pi) / 2.0 * n
    else:
        ctx = (minx + maxx + 1) / 2.0
        cty = (miny + maxy + 1) / 2.0
    render_device(tiles, ctx, cty, args.px_per_tile, args.speed,
                  os.path.join(args.tiles_dir, "preview_device.png"))
    print(f"area {aw}x{ah}px -> preview_area.png ; device 800x800 -> preview_device.png")


if __name__ == "__main__":
    main()
