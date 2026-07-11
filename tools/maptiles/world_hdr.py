#!/usr/bin/env python3
"""
world.hdr writer - the manifest for a GPS-paged cell-grid map (see
firmware/main/map/map_world.h for the reader and the authoritative byte layout).

world.hdr is the small always-resident metadata the cell manager reads at boot:
the zoom, the cell size, and exactly which lat/lon cells were baked (the "present
set"), so it can answer coverage and pick neighbour cells without statting the
card. One ZMTA archive per cell lives alongside it under <lat>/<lon>.zmt.

Layout, little-endian:

  Header (20 bytes)
    char[4] magic          'ZMTW'
    u16     version        = VERSION
    u16     zoom
    u16     cell_size_256  cell edge in 1/256 deg (256 = 1 deg)
    u16     ncells
    i16     min_lat, min_lon, max_lat, max_lon   present-set cell-index bbox
  Body
    (i16 lat, i16 lon) * ncells   present cells, sorted ascending by (lat,lon)

A cell index maps to its SW corner as idx * cell_size_256 in 1/256 deg, matching
firmware/main/map/map_cells.c (cell_of).
"""
import struct

MAGIC = b"ZMTW"
VERSION = 1


def cell_of(lon_e7, lat_e7, cell_size_256):
    """The (lat, lon) cell index a 1e-7-deg position falls in (floor division)."""
    den = 10_000_000 * cell_size_256
    return (lat_e7 * 256 // den, lon_e7 * 256 // den)


def pack_world_hdr(zoom, cell_size_256, cells):
    """Serialise world.hdr bytes. `cells` is an iterable of (lat, lon) indices."""
    cs = sorted(set(cells))
    if cs:
        lats = [c[0] for c in cs]
        lons = [c[1] for c in cs]
        bbox = (min(lats), min(lons), max(lats), max(lons))
    else:
        bbox = (0, 0, 0, 0)
    out = bytearray()
    out += MAGIC
    out += struct.pack("<HHHH", VERSION, zoom, cell_size_256, len(cs))
    out += struct.pack("<hhhh", *bbox)
    for lat, lon in cs:
        out += struct.pack("<hh", lat, lon)
    return bytes(out)


def write_world_hdr(path, zoom, cell_size_256, cells):
    with open(path, "wb") as f:
        f.write(pack_world_hdr(zoom, cell_size_256, cells))
