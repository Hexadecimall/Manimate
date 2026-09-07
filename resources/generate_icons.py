"""Render the Manimate app icon to PNG/ICNS/ICO with no external deps."""
import math, os, struct, subprocess, sys, zlib

BG      = (0x14, 0x16, 0x1A)
ACCENT  = (0x58, 0xC4, 0xDD)   # Manim blue
DANGER  = (0xFC, 0x62, 0x55)   # Manim red
SS      = 4                    # supersampling factor


def rounded_rect_sd(x, y, cx, cy, half, r):
    """Signed distance to a rounded square centred on (cx, cy). Negative inside."""
    dx = abs(x - cx) - (half - r)
    dy = abs(y - cy) - (half - r)
    ax, ay = max(dx, 0.0), max(dy, 0.0)
    return math.hypot(ax, ay) + min(max(dx, dy), 0.0) - r


def seg_dist(px, py, ax, ay, bx, by):
    vx, vy = bx - ax, by - ay
    wx, wy = px - ax, py - ay
    denom = vx * vx + vy * vy
    t = 0.0 if denom == 0 else max(0.0, min(1.0, (wx * vx + wy * vy) / denom))
    return math.hypot(px - (ax + t * vx), py - (ay + t * vy))


def over(dst, src, a):
    return tuple(int(round(s * a + d * (1 - a))) for d, s in zip(dst, src))


def render(size):
    n = size * SS
    cx = cy = n / 2

    # A margin round the plate, as platform icon guidelines expect.
    half = n * 0.44
    radius = half * 0.46

    ring_r = n * 0.285
    ring_w = n * 0.070

    tri_r = n * 0.160
    tri_w = n * 0.064
    tri = [(cx + tri_r * math.cos(math.radians(a)), cy + tri_r * math.sin(math.radians(a)))
           for a in (-90, 30, 150)]

    rows = []
    for py in range(size):
        row = bytearray()
        for px in range(size):
            acc = [0.0, 0.0, 0.0, 0.0]
            for sy in range(SS):
                for sx in range(SS):
                    x = px * SS + sx + 0.5
                    y = py * SS + sy + 0.5

                    if rounded_rect_sd(x, y, cx, cy, half, radius) >= 0:
                        continue

                    colour = BG
                    ring = abs(math.hypot(x - cx, y - cy) - ring_r)
                    if ring < ring_w / 2:
                        colour = ACCENT
                    else:
                        edge = min(seg_dist(x, y, *tri[i], *tri[(i + 1) % 3]) for i in range(3))
                        if edge < tri_w / 2:
                            colour = DANGER

                    acc[0] += colour[0]
                    acc[1] += colour[1]
                    acc[2] += colour[2]
                    acc[3] += 1.0

            samples = SS * SS
            if acc[3] == 0:
                row += bytes((0, 0, 0, 0))
            else:
                alpha = acc[3] / samples
                row += bytes((int(round(acc[0] / acc[3])), int(round(acc[1] / acc[3])),
                              int(round(acc[2] / acc[3])), int(round(alpha * 255))))
        rows.append(bytes(row))
    return rows


def png_bytes(size, rows):
    raw = b"".join(b"\x00" + r for r in rows)

    def chunk(tag, data):
        body = tag + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    return (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 9))
            + chunk(b"IEND", b""))


def ico_bytes(entries):
    """entries: list of (size, png bytes). ICO with PNG payloads (Vista+)."""
    header = struct.pack("<HHH", 0, 1, len(entries))
    offset = 6 + 16 * len(entries)
    directory, payload = b"", b""
    for size, data in entries:
        directory += struct.pack("<BBBBHHII", size % 256, size % 256, 0, 0, 1, 32, len(data), offset)
        payload += data
        offset += len(data)
    return header + directory + payload


def main(out_dir):
    os.makedirs(out_dir, exist_ok=True)
    sizes = [16, 32, 48, 64, 128, 256, 512, 1024]
    pngs = {}
    for size in sizes:
        data = png_bytes(size, render(size))
        pngs[size] = data
        with open(os.path.join(out_dir, f"icon_{size}.png"), "wb") as f:
            f.write(data)
        print("png", size)

    with open(os.path.join(out_dir, "icon.ico"), "wb") as f:
        f.write(ico_bytes([(s, pngs[s]) for s in (16, 32, 48, 64, 128, 256)]))
    print("ico")

    iconset = os.path.join(out_dir, "Manimate.iconset")
    os.makedirs(iconset, exist_ok=True)
    mapping = {
        "icon_16x16.png": 16, "icon_16x16@2x.png": 32,
        "icon_32x32.png": 32, "icon_32x32@2x.png": 64,
        "icon_128x128.png": 128, "icon_128x128@2x.png": 256,
        "icon_256x256.png": 256, "icon_256x256@2x.png": 512,
        "icon_512x512.png": 512, "icon_512x512@2x.png": 1024,
    }
    for name, size in mapping.items():
        with open(os.path.join(iconset, name), "wb") as f:
            f.write(pngs[size])
    subprocess.run(["iconutil", "-c", "icns", iconset,
                    "-o", os.path.join(out_dir, "icon.icns")], check=True)
    print("icns")


if __name__ == "__main__":
    main(sys.argv[1])
