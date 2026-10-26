#!/usr/bin/env python3
"""tilepack.py - build zNN.tpk map packs for the ESP32-S3 Touch Map.

Commands
  estimate  Count tiles and estimate size for a bounding box over a zoom range.
  fetch     Download tiles from a provider whose terms allow offline use.
            Refuses the OpenStreetMap public tile servers (bulk use is forbidden).
  pack      Pack a {z}/{x}/{y}.jpg|png directory tree into zNN.tpk files.
  demo      Write small synthetic packs (no network) for bring-up tests T-03..T-06.
  inspect   Print the header of a .tpk and verify its index.

File format: see components/kernel/include/k_tpk.h (keep both in sync).
"""
import argparse
import io
import math
import os
import struct
import sys
import urllib.parse
import urllib.request

HEADER_BYTES = 96
ATTR_BYTES = 60
MAX_TILE = 192 * 1024
MAX_FILE = 4 * 1024 ** 3 - 1
FORBIDDEN_HOSTS = ("tile.openstreetmap.org", "openstreetmap.org")

# Ontario, generous bounding box (west, south, east, north)
ONTARIO = (-95.2, 41.6, -74.3, 56.9)


# ---- tile math -------------------------------------------------------------
def lonlat_to_tile(lon, lat, z):
    n = 2 ** z
    lat = max(min(lat, 85.05112878), -85.05112878)
    x = int((lon + 180.0) / 360.0 * n)
    r = math.radians(lat)
    y = int((1.0 - math.asinh(math.tan(r)) / math.pi) / 2.0 * n)
    return min(max(x, 0), n - 1), min(max(y, 0), n - 1)


def bbox_tiles(bbox, z):
    w, s, e, n = bbox
    x0, y1 = lonlat_to_tile(w, s, z)
    x1, y0 = lonlat_to_tile(e, n, z)
    return x0, y0, x1, y1


def parse_bbox(text):
    if text == "ontario":
        return ONTARIO
    parts = [float(v) for v in text.split(",")]
    if len(parts) != 4:
        sys.exit("bbox must be 'west,south,east,north' or 'ontario'")
    return tuple(parts)


# ---- header ----------------------------------------------------------------
def make_header(zoom, x0, y0, w, h, data_off, attribution):
    attr = attribution.encode("utf-8")[:ATTR_BYTES - 1]
    hdr = struct.pack("<4sHBBIIIIHHII", b"TPK1", 1, zoom, 0, x0, y0, w, h, 256, 0,
                      HEADER_BYTES, data_off)
    hdr += attr.ljust(ATTR_BYTES, b"\0")
    assert len(hdr) == HEADER_BYTES, len(hdr)
    return hdr


def parse_header(raw):
    magic, ver, zoom, _flags, x0, y0, w, h, ts, _res, idx_off, data_off = struct.unpack("<4sHBBIIIIHHII", raw[:36])
    attr = raw[36:96].split(b"\0", 1)[0].decode("utf-8", "replace")
    return dict(magic=magic, version=ver, zoom=zoom, x0=x0, y0=y0, w=w, h=h,
                tile_size=ts, index_off=idx_off, data_off=data_off, attribution=attr)


# ---- JPEG helpers ------------------------------------------------------------
def is_baseline_jpeg(data):
    """True if the first SOF marker is baseline (SOF0). Progressive (SOF2) will not decode."""
    if data[:2] != b"\xff\xd8":
        return False
    i = 2
    while i + 4 < len(data):
        if data[i] != 0xFF:
            i += 1
            continue
        marker = data[i + 1]
        if marker in (0xC0, 0xC1):
            return True
        if marker == 0xC2:
            return False
        if marker in (0xD8, 0x01) or 0xD0 <= marker <= 0xD7:
            i += 2
            continue
        seglen = struct.unpack(">H", data[i + 2:i + 4])[0]
        i += 2 + seglen
    return False


def to_baseline_jpeg(path, quality):
    """Re-encode any image (PNG, progressive JPEG, ...) as a 256x256 baseline JPEG."""
    try:
        from PIL import Image
    except ImportError:
        sys.exit(f"{path}: needs re-encoding but Pillow is not installed (pip install pillow)")
    im = Image.open(path).convert("RGB")
    if im.size != (256, 256):
        im = im.resize((256, 256))
    buf = io.BytesIO()
    im.save(buf, "JPEG", quality=quality, progressive=False, optimize=False)
    return buf.getvalue()


def load_tile(path, quality):
    with open(path, "rb") as f:
        data = f.read()
    if path.lower().endswith((".jpg", ".jpeg")) and is_baseline_jpeg(data) and len(data) <= MAX_TILE:
        return data
    data = to_baseline_jpeg(path, quality)
    if len(data) > MAX_TILE:
        data = to_baseline_jpeg(path, max(40, quality - 25))
    if len(data) > MAX_TILE:
        sys.exit(f"{path}: tile larger than {MAX_TILE} bytes even at low quality")
    return data


# ---- writer ----------------------------------------------------------------
def write_pack(out_path, zoom, rect, tile_bytes, attribution):
    """rect = (x0, y0, x1, y1) inclusive; tile_bytes(x, y) -> bytes | None."""
    x0, y0, x1, y1 = rect
    w, h = x1 - x0 + 1, y1 - y0 + 1
    index_bytes = w * h * 8
    data_off = HEADER_BYTES + index_bytes
    index = bytearray(index_bytes)
    cursor = data_off
    tmp = out_path + ".part"
    with open(tmp, "wb") as f:
        f.write(b"\0" * data_off)          # placeholder for header + index
        for ty in range(y0, y1 + 1):
            for tx in range(x0, x1 + 1):
                data = tile_bytes(tx, ty)
                if not data:
                    continue
                if cursor + len(data) > MAX_FILE:
                    sys.exit(f"{out_path}: would exceed the 4 GiB limit; use a smaller area")
                cell = (ty - y0) * w + (tx - x0)
                struct.pack_into("<II", index, cell * 8, cursor, len(data))
                f.write(data)
                cursor += len(data)
        f.seek(0)
        f.write(make_header(zoom, x0, y0, w, h, data_off, attribution))
        f.write(index)
    os.replace(tmp, out_path)
    return w, h, cursor


# ---- commands ----------------------------------------------------------------
def cmd_estimate(a):
    bbox = parse_bbox(a.bbox)
    print(f"bbox {bbox}, {a.kb} KB per tile (estimate)")
    print(f"{'zoom':>4} {'tiles':>12} {'size':>12}  note")
    for z in range(a.zmin, a.zmax + 1):
        x0, y0, x1, y1 = bbox_tiles(bbox, z)
        n = (x1 - x0 + 1) * (y1 - y0 + 1)
        size = n * a.kb * 1024
        note = "over the 4 GiB file limit" if size > MAX_FILE else ("near the limit" if size > 0.8 * MAX_FILE else "")
        print(f"{z:>4} {n:>12,} {size / 1e9:>9.2f} GB  {note}")


def cmd_fetch(a):
    host = urllib.parse.urlparse(a.url.replace("{z}", "0").replace("{x}", "0").replace("{y}", "0")).hostname or ""
    if any(host == h or host.endswith("." + h) for h in FORBIDDEN_HOSTS):
        sys.exit("Refusing: the OpenStreetMap public tile servers forbid bulk downloading.\n"
                 "Use a provider whose terms allow offline use, render your own tiles, or buy a regional set.")
    bbox = parse_bbox(a.bbox)
    count = 0
    for z in range(a.zmin, a.zmax + 1):
        x0, y0, x1, y1 = bbox_tiles(bbox, z)
        for x in range(x0, x1 + 1):
            for y in range(y0, y1 + 1):
                dst = os.path.join(a.out, str(z), str(x), f"{y}.jpg")
                if os.path.exists(dst):
                    continue
                os.makedirs(os.path.dirname(dst), exist_ok=True)
                url = a.url.format(z=z, x=x, y=y)
                req = urllib.request.Request(url, headers={"User-Agent": a.user_agent})
                with urllib.request.urlopen(req, timeout=30) as r, open(dst, "wb") as f:
                    f.write(r.read())
                count += 1
    print(f"downloaded {count} tiles into {a.out}")


def cmd_pack(a):
    os.makedirs(a.out, exist_ok=True)
    for z in range(a.zmin, a.zmax + 1):
        zdir = os.path.join(a.src, str(z))
        if not os.path.isdir(zdir):
            print(f"z{z}: no directory, skipped")
            continue
        xs = [int(d) for d in os.listdir(zdir) if d.isdigit()]
        ys = [int(os.path.splitext(f)[0]) for d in xs for f in os.listdir(os.path.join(zdir, str(d)))
              if os.path.splitext(f)[0].isdigit()]
        if not xs or not ys:
            continue
        rect = (min(xs), min(ys), max(xs), max(ys))

        def tile_bytes(x, y, zdir=zdir):
            for ext in ("jpg", "jpeg", "png"):
                p = os.path.join(zdir, str(x), f"{y}.{ext}")
                if os.path.exists(p):
                    return load_tile(p, a.quality)
            return None

        out = os.path.join(a.out, f"z{z:02d}.tpk")
        w, h, size = write_pack(out, z, rect, tile_bytes, a.attribution)
        print(f"{out}: {w}x{h} cells, {size / 1e6:.1f} MB")


def cmd_demo(a):
    """Synthetic map: grid of coloured tiles with the tile coordinates written on them."""
    try:
        from PIL import Image, ImageDraw
    except ImportError:
        sys.exit("demo needs Pillow: pip install pillow")
    os.makedirs(a.out, exist_ok=True)
    bbox = parse_bbox(a.bbox)

    def make(z):
        def tile(x, y):
            hue = ((x * 37 + y * 101 + z * 17) % 255)
            base = (200 + (hue % 40), 215 + (hue % 30), 190 + ((hue * 3) % 50))
            im = Image.new("RGB", (256, 256), base)
            d = ImageDraw.Draw(im)
            for k in range(0, 256, 64):
                d.line([(k, 0), (k, 255)], fill=(225, 225, 225))
                d.line([(0, k), (255, k)], fill=(225, 225, 225))
            d.rectangle([0, 0, 255, 255], outline=(120, 120, 130))
            d.line([(0, 128), (255, 128)], fill=(250, 210, 140), width=9)
            d.line([(128, 0), (128, 255)], fill=(250, 210, 140), width=9)
            d.text((8, 8), f"z{z}  {x},{y}", fill=(40, 40, 60))
            buf = io.BytesIO()
            im.save(buf, "JPEG", quality=70, progressive=False)
            return buf.getvalue()
        return tile

    for z in range(a.zmin, a.zmax + 1):
        rect = bbox_tiles(bbox, z)
        out = os.path.join(a.out, f"z{z:02d}.tpk")
        w, h, size = write_pack(out, z, rect, make(z), "(c) Demo tiles, synthetic")
        print(f"{out}: {w}x{h} cells, {size / 1e6:.2f} MB")


def cmd_inspect(a):
    with open(a.file, "rb") as f:
        raw = f.read(HEADER_BYTES)
        size = os.fstat(f.fileno()).st_size
        h = parse_header(raw)
        print(h)
        ok = h["magic"] == b"TPK1" and h["tile_size"] == 256
        cells = h["w"] * h["h"]
        f.seek(h["index_off"])
        filled = bad = 0
        for _ in range(cells):
            off, ln = struct.unpack("<II", f.read(8))
            if ln:
                filled += 1
                if ln > MAX_TILE or off + ln > size or off < h["data_off"]:
                    bad += 1
        print(f"cells {cells}, tiles {filled}, bad entries {bad}, file {size} bytes")
        sys.exit(0 if ok and bad == 0 else 1)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)

    def common(p, zmin=5, zmax=12):
        p.add_argument("--bbox", default="ontario", help="'ontario' or west,south,east,north")
        p.add_argument("--zmin", type=int, default=zmin)
        p.add_argument("--zmax", type=int, default=zmax)

    p = sub.add_parser("estimate"); common(p)
    p.add_argument("--kb", type=float, default=15.0, help="assumed KB per tile")
    p.set_defaults(fn=cmd_estimate)

    p = sub.add_parser("fetch"); common(p)
    p.add_argument("--url", required=True, help="template with {z} {x} {y}")
    p.add_argument("--out", required=True)
    p.add_argument("--user-agent", default="esp32s3-touchmap-tilepack/1.0")
    p.set_defaults(fn=cmd_fetch)

    p = sub.add_parser("pack"); common(p)
    p.add_argument("--src", required=True, help="directory containing {z}/{x}/{y}.jpg")
    p.add_argument("--out", required=True, help="output directory (copy to /maps on the card)")
    p.add_argument("--attribution", required=True, help="text shown on the map, max 59 bytes")
    p.add_argument("--quality", type=int, default=75)
    p.set_defaults(fn=cmd_pack)

    p = sub.add_parser("demo", help="synthetic test packs")
    common(p, 10, 14)
    p.set_defaults(bbox="-79.55,43.55,-79.25,43.80")      # a patch around Toronto
    p.add_argument("--out", default="sdcard/maps")
    p.set_defaults(fn=cmd_demo)

    p = sub.add_parser("inspect"); p.add_argument("file"); p.set_defaults(fn=cmd_inspect)

    a = ap.parse_args()
    a.fn(a)


if __name__ == "__main__":
    main()
