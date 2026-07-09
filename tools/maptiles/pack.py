#!/usr/bin/env python3
"""
Pack a baked tiles/ tree into a single ZMTA archive for embedding in firmware
flash (no SD card needed for a demo). Layout, little-endian:

  Header
    char[4] magic     'ZMTA'
    u16     zoom
    u16     reserved
    u32     tile_count
  Index  (tile_count entries, 16 bytes each)
    u32 tx, u32 ty, u32 offset, u32 len   (offset from file start; blob is a
                                           full ZMT0 tile; 4-byte aligned)
  Tile blobs

The firmware memory-maps this from flash and parses tiles in place - zero RAM
copy - via map_tileset_load_mem().

Usage:  pack.py <tiles_dir> <out.zmta>
"""
import json
import os
import struct
import sys


def main():
    tiles_dir, out = sys.argv[1], sys.argv[2]
    zoom = json.load(open(os.path.join(tiles_dir, "manifest.json")))["zoom"]

    blobs = []  # (tx, ty, bytes)
    zdir = os.path.join(tiles_dir, str(zoom))
    for xd in sorted(os.listdir(zdir), key=lambda s: int(s) if s.isdigit() else 0):
        xp = os.path.join(zdir, xd)
        if not os.path.isdir(xp):
            continue
        for yf in sorted(os.listdir(xp)):
            if not yf.endswith(".bin"):
                continue
            ty = int(yf[:-4])
            blobs.append((int(xd), ty, open(os.path.join(xp, yf), "rb").read()))

    header = struct.pack("<4sHHI", b"ZMTA", zoom, 0, len(blobs))
    index_size = len(blobs) * 16
    offset = len(header) + index_size
    offset = (offset + 3) & ~3

    index = bytearray()
    body = bytearray()
    cur = offset
    for tx, ty, data in blobs:
        index += struct.pack("<IIII", tx, ty, cur, len(data))
        body += data
        pad = (-len(data)) & 3
        body += b"\x00" * pad
        cur += len(data) + pad

    with open(out, "wb") as f:
        f.write(header)
        f.write(index)
        f.write(b"\x00" * (offset - len(header) - index_size))
        f.write(body)
    print(f"{len(blobs)} tiles z{zoom} -> {out} ({(len(header)+index_size+len(body))/1024:.1f} KB)")


if __name__ == "__main__":
    main()
