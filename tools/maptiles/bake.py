#!/usr/bin/env python3
"""
Bake line-delimited GeoJSON (from `osmium export`) into compact per-tile vector
binaries - the on-device map format for the cluster.

Each output file is `<out>/<z>/<x>/<y>.bin`, little-endian:

  Header
    char[4]  magic       'ZMT0'
    u16      zoom
    u32      tile_x
    u32      tile_y
    u16      feature_count
  Feature * feature_count
    u8       type         0 = polyline (stroke), 1 = polygon (fill)
    u8       style        index into the shared style table (see styles.py)
    u16      point_count
    (u16 x, u16 y) * point_count   tile-local coords, 0..EXTENT

Geometry is clipped per tile: a segment/ring that crosses a tile boundary is
cut at the boundary so every point sits inside 0..EXTENT. Coords are quantised
to a 12-bit-ish grid (EXTENT=4096), the same trick MVT uses - that quantisation
is what makes the format tiny.

Usage:  bake.py <input.geojsonl> <out_dir> [--zoom 16]
"""
import argparse
import json
import math
import os
import struct
import sys
from collections import defaultdict

from styles import classify

EXTENT = 4096
MAGIC = b"ZMT0"


def lonlat_to_tilef(lon, lat, z):
    """Fractional slippy-tile coordinate (x,y) for a lon/lat at zoom z."""
    n = 2 ** z
    x = (lon + 180.0) / 360.0 * n
    lat_r = math.radians(lat)
    y = (1.0 - math.asinh(math.tan(lat_r)) / math.pi) / 2.0 * n
    return x, y


def clip_segment(x0, y0, x1, y1, xmin, ymin, xmax, ymax):
    """Liang-Barsky clip of a segment to an axis-aligned box. None if outside."""
    dx = x1 - x0
    dy = y1 - y0
    p = [-dx, dx, -dy, dy]
    q = [x0 - xmin, xmax - x0, y0 - ymin, ymax - y0]
    t0, t1 = 0.0, 1.0
    for pi, qi in zip(p, q):
        if pi == 0:
            if qi < 0:
                return None
        else:
            t = qi / pi
            if pi < 0:
                if t > t1:
                    return None
                if t > t0:
                    t0 = t
            else:
                if t < t0:
                    return None
                if t < t1:
                    t1 = t
    return (x0 + t0 * dx, y0 + t0 * dy, x0 + t1 * dx, y0 + t1 * dy)


def clip_polygon(ring, xmin, ymin, xmax, ymax):
    """Sutherland-Hodgman clip of a polygon ring to a box. Returns a ring."""
    def clip_edge(poly, inside, intersect):
        out = []
        if not poly:
            return out
        prev = poly[-1]
        prev_in = inside(prev)
        for cur in poly:
            cur_in = inside(cur)
            if cur_in:
                if not prev_in:
                    out.append(intersect(prev, cur))
                out.append(cur)
            elif prev_in:
                out.append(intersect(prev, cur))
            prev, prev_in = cur, cur_in
        return out

    def isect(a, b, get, bound):
        (ax, ay), (bx, by) = a, b
        av, bv = get(a), get(b)
        t = (bound - av) / (bv - av)
        return (ax + t * (bx - ax), ay + t * (by - ay))

    poly = ring
    poly = clip_edge(poly, lambda p: p[0] >= xmin, lambda a, b: isect(a, b, lambda p: p[0], xmin))
    poly = clip_edge(poly, lambda p: p[0] <= xmax, lambda a, b: isect(a, b, lambda p: p[0], xmax))
    poly = clip_edge(poly, lambda p: p[1] >= ymin, lambda a, b: isect(a, b, lambda p: p[1], ymin))
    poly = clip_edge(poly, lambda p: p[1] <= ymax, lambda a, b: isect(a, b, lambda p: p[1], ymax))
    return poly


def quant(v_frac_local):
    """Tile-fractional [0,1] -> integer 0..EXTENT, clamped."""
    q = int(round(v_frac_local * EXTENT))
    return max(0, min(EXTENT, q))


def iter_lines(geom):
    """Yield each ring/line as a coord list, tagged is_polygon."""
    t = geom["type"]
    c = geom["coordinates"]
    if t == "LineString":
        yield c, False
    elif t == "MultiLineString":
        for ls in c:
            yield ls, False
    elif t == "Polygon":
        yield c[0], True                    # exterior ring only (spike)
    elif t == "MultiPolygon":
        for poly in c:
            yield poly[0], True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("input")
    ap.add_argument("out_dir")
    ap.add_argument("--zoom", type=int, default=16)
    args = ap.parse_args()
    z = args.zoom

    # tile (tx,ty) -> list of (type, style, [ (x,y), ... ]) in EXTENT coords.
    tiles = defaultdict(list)
    n_feat = 0
    n_skip = 0
    minx = miny = 1 << 30
    maxx = maxy = -1

    with open(args.input) as f:
        for line in f:
            line = line.strip().rstrip("\x1e")   # geojsonseq record separator
            if not line:
                continue
            feat = json.loads(line)
            geom = feat.get("geometry")
            if not geom:
                continue
            for coords, is_poly in iter_lines(geom):
                style = classify(feat.get("properties", {}), is_poly)
                if style is None:
                    n_skip += 1
                    continue
                n_feat += 1
                pts = [lonlat_to_tilef(lon, lat, z) for lon, lat in coords]
                txs = [int(math.floor(p[0])) for p in pts]
                tys = [int(math.floor(p[1])) for p in pts]
                for tx in range(min(txs), max(txs) + 1):
                    for ty in range(min(tys), max(tys) + 1):
                        minx, miny = min(minx, tx), min(miny, ty)
                        maxx, maxy = max(maxx, tx), max(maxy, ty)
                        _emit(tiles, tx, ty, pts, is_poly, style)

    # An empty cell (ocean / no roads) produces no tile subdirs, so make sure the
    # out dir exists before writing the manifest - the caller reads tiles==0 and skips.
    os.makedirs(args.out_dir, exist_ok=True)
    _write(tiles, args.out_dir, z)
    manifest = dict(zoom=z, extent=EXTENT,
                    min_x=minx, min_y=miny, max_x=maxx, max_y=maxy,
                    tiles=len(tiles), features=n_feat, skipped=n_skip)
    with open(os.path.join(args.out_dir, "manifest.json"), "w") as m:
        json.dump(manifest, m, indent=2)
    total = sum(os.path.getsize(os.path.join(dp, fn))
                for dp, _, fns in os.walk(args.out_dir) for fn in fns if fn.endswith(".bin"))
    print(f"features {n_feat} kept / {n_skip} skipped, "
          f"{len(tiles)} tiles, {total/1024:.1f} KB "
          f"({total/max(1,len(tiles)):.0f} B/tile), "
          f"tile range x[{minx}..{maxx}] y[{miny}..{maxy}]")


def _emit(tiles, tx, ty, pts, is_poly, style):
    if is_poly:
        ring = clip_polygon(pts, tx, ty, tx + 1, ty + 1)
        if len(ring) >= 3:
            local = [(quant(x - tx), quant(y - ty)) for x, y in ring]
            tiles[(tx, ty)].append((1, style, local))
        return
    # polyline: clip each segment, coalesce consecutive visible ones.
    run = []
    for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
        seg = clip_segment(x0, y0, x1, y1, tx, ty, tx + 1, ty + 1)
        if seg is None:
            if len(run) >= 2:
                tiles[(tx, ty)].append((0, style, run))
            run = []
            continue
        cx0, cy0, cx1, cy1 = seg
        p0 = (quant(cx0 - tx), quant(cy0 - ty))
        p1 = (quant(cx1 - tx), quant(cy1 - ty))
        if not run:
            run = [p0, p1]
        elif run[-1] == p0:
            run.append(p1)
        else:
            if len(run) >= 2:
                tiles[(tx, ty)].append((0, style, run))
            run = [p0, p1]
    if len(run) >= 2:
        tiles[(tx, ty)].append((0, style, run))


def _write(tiles, out_dir, z):
    for (tx, ty), feats in tiles.items():
        d = os.path.join(out_dir, str(z), str(tx))
        os.makedirs(d, exist_ok=True)
        with open(os.path.join(d, f"{ty}.bin"), "wb") as fp:
            fp.write(MAGIC)
            fp.write(struct.pack("<HIIH", z, tx, ty, len(feats)))
            for ftype, style, local in feats:
                fp.write(struct.pack("<BBH", ftype, style, len(local)))
                for x, y in local:
                    fp.write(struct.pack("<HH", x, y))


if __name__ == "__main__":
    sys.exit(main())
