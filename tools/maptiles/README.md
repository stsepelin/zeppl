# maptiles — compact vector map pipeline

Turns an OpenStreetMap `.osm.pbf` into tiny per-tile vector binaries the cluster
can read straight off the SD card, and previews them as PNGs so we can judge map
quality before writing the on-device rasteriser.

Why vector, not raster: a raster z16 tile is a flat 128 KB of pixels regardless
of content; a vector tile of the same area is ~1 KB of geometry. Central Tallinn
(~5x3.5 km, 203 tiles) is **182 KB vector vs 26 MB raster — ~140x smaller**.
Roads + water for all of Estonia land in ~0.3-0.5 GB.

## Pipeline

```sh
python3 -m venv .venv && .venv/bin/pip install Pillow

PBF=~/Downloads/estonia-260707.osm.pbf
# 1. clip to a bounding box (minlon,minlat,maxlon,maxlat)
osmium extract -b 24.72,59.42,24.80,59.45 "$PBF" -o area.pbf --overwrite
# 2. keep only roads + water
osmium tags-filter area.pbf w/highway w/waterway n/natural=water a/natural=water a/water \
  -o area_rw.pbf --overwrite
# 3. export to line-delimited GeoJSON
osmium export area_rw.pbf --geometry-types=linestring,polygon -f geojsonseq -o area.geojsonl --overwrite
# 4. bake to per-tile binaries  ->  tiles/<z>/<x>/<y>.bin  + manifest.json
.venv/bin/python bake.py area.geojsonl tiles --zoom 16
# 5. preview  ->  tiles/preview_area.png + tiles/preview_device.png
.venv/bin/python preview.py tiles --center 59.437,24.745 --speed 52
```

`osmium` is `brew install osmium-tool`.

## Tile binary format (`ZMT0`)

Little-endian. See the header comment in `bake.py` for the byte layout. Geometry
is clipped per tile and coordinates are quantised to a 0..4096 tile-local grid
(the trick that makes it small). Feature = `type` (0 line / 1 fill) + `style`
(index into `styles.py`) + a point list. The renderer owns style -> colour/width,
so restyling never needs a re-bake.

`styles.py` is the single source of truth for classification + palette, shared by
the preview today and the cluster firmware later.

## Files

- `bake.py` — GeoJSON -> per-tile `.bin` + `manifest.json`
- `preview.py` — `.bin` -> PNG (reads the exact bytes the cluster will read)
- `styles.py` — OSM tag -> style id, style id -> colour/width
