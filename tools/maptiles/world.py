#!/usr/bin/env python3
"""
world.py - bake a whole continent into a GPS-paged cell-grid map for the cluster.

Where region.py makes ONE .zmta for a small area whose entire tile index must fit
in PSRAM, world.py tiles the input into a grid of lat/lon cells - one ZMTA per
cell - so the firmware keeps only the cells near the rider resident and pages the
rest off the SD card as it moves (firmware/docs/map-worldwide-plan.md). This is
what puts Europe (and later the world) on the card.

Output tree (copy the whole thing to /sdcard/map, or match CONFIG_VROD_MAP_SD_DIR):

  <out>/world.hdr        manifest: zoom, cell size, present-cell set (world_hdr.py)
  <out>/N59/E024.zmt     one ZMTA per cell; dir = lat cell, file = lon cell
  <out>/N59/E025.zmt     letter = hemisphere of the cell INDEX (not the degree),
  <out>/S34/W001.zmt     so the runtime rebuilds the path from a cell index alone.

Pipeline (one filter, then ONE streaming export - the whole continent is read
exactly once, never once per cell/batch, which would take ~15 h):

  1. osmium tags-filter: keep roads + water + building footprints -> rw.pbf
     (shrinks 32 GB -> a few GB; --no-buildings drops buildings, ~3x smaller
     tiles, for a continent bake that would overflow the card).
  2. osmium export rw.pbf -f geojsonseq | bucket: stream every feature once and
     append it to the on-disk geojsonl of each cell its bbox touches (bounded RAM,
     an LRU of open file handles). No per-cell osmium extract, so no repeated reads
     and no OOM from buffering hundreds of outputs.
  3. Per touched cell: bake.py -> pack.py -> <lat>/<lon>.zmt, then delete the
     cell geojsonl. Bounded RAM: one cell at a time.
  4. world.hdr from the cells that produced tiles.

Disk discipline: point --out (and so the scratch dir) at the SD card / an external
disk - the filtered pbf + per-cell pbfs are the peak, never the GeoJSON explosion.

Examples:
  # The three Baltics from a Europe extract (Stage 3 bring-up target):
  python3 world.py --pbf ~/Downloads/europe-latest.osm.pbf \
      --bbox 20.9,53.8,28.3,59.8 --out /Volumes/SD/map

  # Whole input extent, 1 deg cells, z16 (Europe: large + slow):
  python3 world.py --pbf europe-latest.osm.pbf --out /Volumes/SD/map

Needs `osmium` (brew install osmium-tool). bake.py/pack.py are stdlib-only.
"""
import argparse
import collections
import json
import math
import os
import shutil
import subprocess
import sys
import tempfile

from world_hdr import write_world_hdr

HERE = os.path.dirname(os.path.abspath(__file__))


def die(msg):
    sys.exit(f"world.py: {msg}")


def run(cmd, **kw):
    print("+", " ".join(str(c) for c in cmd))
    subprocess.run(cmd, check=True, **kw)


def cell_dir(lat_idx):
    return ("N" if lat_idx >= 0 else "S") + str(abs(lat_idx))


def cell_lon_name(lon_idx):
    return ("E" if lon_idx >= 0 else "W") + str(abs(lon_idx))


def source_bbox(pbf):
    """The data bounding box of a pbf. Prefer the PBF header box (instant - it is
    stored in the OSMHeader); only fall back to a full `-e` scan if the header has
    none (a 32 GB continent extract takes hours to scan, seconds to read the box)."""
    out = subprocess.run(["osmium", "fileinfo", "-j", pbf],
                         check=True, capture_output=True, text=True).stdout
    boxes = json.loads(out).get("header", {}).get("boxes") or []
    if boxes and len(boxes[0]) == 4:
        return tuple(boxes[0])  # [minlon, minlat, maxlon, maxlat]
    out = subprocess.run(["osmium", "fileinfo", "-e", "-j", pbf],
                         check=True, capture_output=True, text=True).stdout
    return tuple(json.loads(out)["data"]["bbox"])


def cells_in_bbox(minlon, minlat, maxlon, maxlat, cell_size_256):
    """Every (lat_idx, lon_idx) cell whose square intersects the bbox."""
    deg = cell_size_256 / 256.0
    lat0 = math.floor(minlat / deg)
    lat1 = math.floor(maxlat / deg)
    lon0 = math.floor(minlon / deg)
    lon1 = math.floor(maxlon / deg)
    for lat in range(lat0, lat1 + 1):
        for lon in range(lon0, lon1 + 1):
            yield (lat, lon)


def geom_bbox(geom):
    """(minlon, minlat, maxlon, maxlat) over a GeoJSON geometry, or None if empty."""
    coords = geom.get("coordinates")
    if not coords:
        return None
    mnx = mny = 1e18
    mxx = mxy = -1e18
    stack = [coords]
    while stack:
        v = stack.pop()
        if v and isinstance(v[0], (int, float)):  # a [lon, lat] point
            x, y = v[0], v[1]
            mnx = min(mnx, x); mxx = max(mxx, x)
            mny = min(mny, y); mxy = max(mxy, y)
        else:
            stack.extend(v)
    return None if mxx < mnx else (mnx, mny, mxx, mxy)


def bake_geojson(geo, out_zmt, zoom, python):
    """A cell's geojsonl -> bake -> pack -> out_zmt. False if the cell baked empty."""
    work = tempfile.mkdtemp(prefix="cell_")
    try:
        tiles = os.path.join(work, "tiles")
        run([python, os.path.join(HERE, "bake.py"), geo, tiles, "--zoom", str(zoom)],
            cwd=HERE)
        manifest = json.load(open(os.path.join(tiles, "manifest.json")))
        if manifest.get("tiles", 0) <= 0:
            return False
        os.makedirs(os.path.dirname(out_zmt), exist_ok=True)
        run([python, os.path.join(HERE, "pack.py"), tiles, out_zmt], cwd=HERE)
        return True
    finally:
        shutil.rmtree(work, ignore_errors=True)


def main():
    ap = argparse.ArgumentParser(
        description="OSM .osm.pbf -> GPS-paged cell-grid map tree.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    ap.add_argument("--pbf", required=True, help="local .osm.pbf extract")
    ap.add_argument("--bbox", help="minlon,minlat,maxlon,maxlat (default: whole input)")
    ap.add_argument("--zoom", type=int, default=16)
    ap.add_argument("--cell-deg", type=float, default=1.0,
                    help="cell edge in degrees (default 1.0; 0.5 for dense metros)")
    ap.add_argument("--out", required=True, help="output map dir (-> /sdcard/map)")
    ap.add_argument("--no-buildings", action="store_true",
                    help="roads + water only (buildings ~3x the tile bytes; drop for a "
                         "continent-scale bake that would overflow the card)")
    ap.add_argument("--rw-pbf",
                    help="skip the tags-filter step and use this already-filtered pbf "
                         "as rw.pbf (re-run a big bake without re-filtering)")
    ap.add_argument("--python", default=sys.executable, help="python for bake/pack")
    ap.add_argument("--keep", action="store_true", help="keep the scratch dir")
    args = ap.parse_args()

    if not shutil.which("osmium"):
        die("osmium not found - `brew install osmium-tool`")

    cell_size_256 = int(round(args.cell_deg * 256))
    if cell_size_256 <= 0:
        die("--cell-deg must be positive")

    pbf = os.path.abspath(os.path.expanduser(args.pbf))
    if not os.path.exists(pbf):
        die(f"no such file: {pbf}")

    if args.bbox:
        bbox = tuple(float(x) for x in args.bbox.split(","))
        if len(bbox) != 4:
            die("--bbox wants minlon,minlat,maxlon,maxlat")
    else:
        print("no --bbox: reading the input's data extent")
        bbox = source_bbox(pbf)
    allowed = set(cells_in_bbox(*bbox, cell_size_256))  # only bake cells inside the bbox
    print(f"{len(allowed)} candidate cells at {args.cell_deg} deg over bbox {bbox}")

    out = os.path.abspath(os.path.expanduser(args.out))
    os.makedirs(out, exist_ok=True)
    # Scratch next to the output so the big files land on the same (roomy) disk.
    work    = tempfile.mkdtemp(prefix="world_", dir=out)
    present = []
    try:
        # 1. filter once: roads + water (skippable with a pre-filtered --rw-pbf).
        if args.rw_pbf:
            rw = os.path.abspath(os.path.expanduser(args.rw_pbf))
            print(f"using pre-filtered {rw} (skipping tags-filter)")
        else:
            rw   = os.path.join(work, "rw.pbf")
            keep = ["w/highway", "w/waterway", "n/natural=water", "a/natural=water", "a/water"]
            if not args.no_buildings:
                keep.append("a/building")
            run(["osmium", "tags-filter", pbf, *keep, "-o", rw, "--overwrite"])

        # 2. ONE streaming export, bucketing each feature into its cell(s) on disk.
        # Reading the multi-GB pbf once (osmium export) instead of once per cell
        # batch (osmium extract) is the difference between ~1 h and ~15 h over a
        # continent - and it sidesteps osmium extract's per-batch node index + the
        # OOM from buffering hundreds of outputs at once. A feature that straddles a
        # cell border is written to every cell it touches; bake.py re-clips per tile.
        celldir = os.path.join(work, "cells")
        os.makedirs(celldir)

        def cell_geo(lat, lon):
            return os.path.join(celldir, f"{cell_dir(lat)}_{cell_lon_name(lon)}.geojsonl")

        handles = collections.OrderedDict()  # (lat,lon) -> open append file; LRU-capped
        touched = set()
        HMAX    = 8192  # > a continent's cell count, so no eviction/reopen thrash
                        # (FD limit is ~1M); LRU only kicks in for pathological grids

        def writer(lat, lon):
            key = (lat, lon)
            h   = handles.get(key)
            if h is None:
                if len(handles) >= HMAX:
                    handles.popitem(last=False)[1].close()
                h = open(cell_geo(lat, lon), "a")
                touched.add(key)
            else:
                handles.move_to_end(key)
            handles[key] = h
            return h

        proc = subprocess.Popen(
            ["osmium", "export", rw, "--geometry-types=linestring,polygon",
             "-f", "geojsonseq", "-o", "-"],
            stdout=subprocess.PIPE, text=True)
        nfeat = 0
        for line in proc.stdout:
            line = line.strip().rstrip("\x1e")
            if not line:
                continue
            try:
                geom = json.loads(line).get("geometry")
            except json.JSONDecodeError:
                continue
            bb = geom_bbox(geom) if geom else None
            if bb is None:
                continue
            for cell in cells_in_bbox(*bb, cell_size_256):
                if cell in allowed:
                    writer(*cell).write(line + "\n")
            nfeat += 1
            if nfeat % 2_000_000 == 0:
                print(f"  bucketed {nfeat // 1_000_000}M features, {len(touched)} cells touched")
        proc.stdout.close()
        if proc.wait() != 0:
            die("osmium export failed")
        for h in handles.values():
            h.close()
        print(f"bucketed {nfeat} features into {len(touched)} cells; baking...")

        # 3. bake each touched cell's geojsonl -> .zmt (delete the geojsonl after).
        for i, (lat, lon) in enumerate(sorted(touched)):
            geo     = cell_geo(lat, lon)
            out_zmt = os.path.join(out, cell_dir(lat), cell_lon_name(lon) + ".zmt")
            try:
                if bake_geojson(geo, out_zmt, args.zoom, args.python):
                    present.append((lat, lon))
                    if len(present) % 50 == 0:
                        print(f"  baked {len(present)} cells ({i + 1}/{len(touched)} scanned)")
            except Exception as e:
                # One bad cell must not abort a multi-hour continent bake; log + skip.
                print(f"  SKIP {cell_dir(lat)}/{cell_lon_name(lon)}: {e}")
            finally:
                try:
                    os.remove(geo)
                except OSError:
                    pass

        # 4. manifest over the cells that actually produced tiles.
        write_world_hdr(os.path.join(out, "world.hdr"),
                        args.zoom, cell_size_256, present)
    finally:
        if args.keep:
            print(f"kept scratch: {work}")
        else:
            shutil.rmtree(work, ignore_errors=True)

    total = sum(os.path.getsize(os.path.join(dp, fn))
                for dp, _, fns in os.walk(out) for fn in fns if fn.endswith(".zmt"))
    print(f"\nDONE  {len(present)} cells, {total / 1e6:.1f} MB -> {out}")
    print("Copy the tree to the SD card as /sdcard/map (or match CONFIG_VROD_MAP_SD_DIR).")


if __name__ == "__main__":
    main()
