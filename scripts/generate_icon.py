"""Generate app-icon.ico from scratch using only Python stdlib.

Creates a multi-resolution ICO with embedded PNGs at 16, 32, 48, 256 pixels.
No external libraries required.
"""

import struct
import zlib


def make_png_chunk(chunk_type: bytes, data: bytes) -> bytes:
    """Build a single PNG chunk."""
    chunk = chunk_type + data
    crc = struct.pack(">I", zlib.crc32(chunk) & 0xFFFFFFFF)
    length = struct.pack(">I", len(data))
    return length + chunk + crc


def make_png(width: int, height: int, pixels: bytes) -> bytes:
    """Create a valid PNG file from raw RGBA pixel data."""
    signature = b"\x89PNG\r\n\x1a\n"

    ihdr_data = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    ihdr = make_png_chunk(b"IHDR", ihdr_data)

    raw = b""
    for y in range(height):
        raw += b"\x00"  # filter: None
        raw += pixels[y * width * 4:(y + 1) * width * 4]

    idat = make_png_chunk(b"IDAT", zlib.compress(raw))
    iend = make_png_chunk(b"IEND", b"")
    return signature + ihdr + idat + iend


def rgba(r: int, g: int, b: int, a: int = 255) -> bytes:
    return struct.pack("BBBB", r, g, b, a)


def fill_rect(px: bytearray, w: int, x: int, y: int, rw: int, rh: int, color: bytes):
    for row in range(y, min(y + rh, w)):
        for col in range(x, min(x + rw, w)):
            offset = (row * w + col) * 4
            if offset + 3 < len(px):
                px[offset:offset + 4] = color


def rounded_rect_fill(px: bytearray, size: int, rx: int, ry: int,
                      rw: int, rh: int, radius: int, color: bytes):
    """Fill a rounded rectangle. For small sizes, use regular rectangle."""
    if radius < 2 or size < 32:
        fill_rect(px, size, rx, ry, rw, rh, color)
        return
    # Draw full rectangle body
    fill_rect(px, size, rx + radius, ry, rw - 2 * radius, rh, color)
    fill_rect(px, size, rx, ry + radius, rw, rh - 2 * radius, color)
    # Simple corner circles
    rr = radius
    for dy in range(-rr, rr + 1):
        for dx in range(-rr, rr + 1):
            if dx * dx + dy * dy <= rr * rr:
                for cx, cy in [(rx + rr, ry + rr), (rx + rw - rr - 1, ry + rr),
                               (rx + rr, ry + rh - rr - 1), (rx + rw - rr - 1, ry + rh - rr - 1)]:
                    px_x = cx + dx
                    py_y = cy + dy
                    if 0 <= px_x < size and 0 <= py_y < size:
                        offset = (py_y * size + px_x) * 4
                        px[offset:offset + 4] = color


def draw_icon(size: int) -> bytes:
    """Draw a workflow icon at the given square size."""
    px = bytearray(size * size * 4)

    bg_color = rgba(37, 99, 235)  # #2563EB blue
    node_bg = rgba(255, 255, 255)
    node_strip = rgba(37, 99, 235)
    green = rgba(22, 163, 74)
    amber = rgba(217, 119, 6)

    margin = max(1, size // 20)
    radius = max(2, size // 12)
    node_h = max(4, size // 6)
    node_w_ratio = 0.42
    node_w = max(6, int(size * node_w_ratio))
    strip_w = max(1, size // 30)

    # Background rounded rect
    rounded_rect_fill(px, size, margin, margin, size - 2 * margin,
                      size - 2 * margin, radius + 2, bg_color)

    if size <= 16:
        # Very simplified: two dots connected by a line
        center_x = size // 2
        top_y = size // 4
        bot_y = 3 * size // 4
        dot_r = max(1, size // 10)
        for dy in range(-dot_r, dot_r + 1):
            for dx in range(-dot_r, dot_r + 1):
                if dx * dx + dy * dy <= dot_r * dot_r:
                    for (cx, cy) in [(center_x, top_y), (center_x, bot_y)]:
                        off = ((cy + dy) * size + (cx + dx)) * 4
                        if 0 <= off < len(px) - 3:
                            px[off:off + 4] = rgba(255, 255, 255)
        # Connecting line
        for y in range(top_y + dot_r, bot_y - dot_r + 1):
            off = (y * size + center_x) * 4
            if 0 <= off < len(px) - 3:
                px[off:off + 4] = rgba(255, 255, 255, 150)
    elif size <= 32:
        # Simple: top node, line, bottom node
        top_y = 3
        bot_y = size - 3 - node_h
        cx = (size - node_w) // 2
        # Top node
        rounded_rect_fill(px, size, cx, top_y, node_w, node_h, 2, node_bg)
        fill_rect(px, size, cx, top_y, strip_w, node_h, node_strip)
        # Bottom node
        rounded_rect_fill(px, size, cx, bot_y, node_w, node_h, 2, node_bg)
        fill_rect(px, size, cx, bot_y, strip_w, node_h, node_strip)
        # Arrow line
        mid_y1 = top_y + node_h
        mid_y2 = bot_y
        mid_x = size // 2
        for y in range(mid_y1, mid_y2):
            off = (y * size + mid_x) * 4
            if 0 <= off < len(px) - 3:
                px[off:off + 4] = rgba(255, 255, 255, 200)
        # Arrow head
        arrow_tip_y = bot_y
        for i in range(3):
            for j in range(-i, i + 1):
                off = ((arrow_tip_y + i) * size + (mid_x + j)) * 4
                if 0 <= off < len(px) - 3:
                    px[off:off + 4] = rgba(255, 255, 255)
    else:
        # Full workflow DAG: top node → two branch nodes → bottom node
        h_margin = max(2, size // 10)
        top_y = size // 10
        mid_y = size // 2 - node_h // 2
        bot_y = size - size // 8 - node_h

        center_x = (size - node_w) // 2
        left_x = h_margin
        right_x = size - h_margin - node_w

        # Top source node
        rounded_rect_fill(px, size, center_x, top_y, node_w, node_h, radius, node_bg)
        fill_rect(px, size, center_x, top_y, strip_w, node_h, node_strip)

        # Left branch node (green strip)
        rounded_rect_fill(px, size, left_x, mid_y, node_w, node_h, radius, node_bg)
        fill_rect(px, size, left_x, mid_y, strip_w, node_h, green)

        # Right branch node (amber strip)
        rounded_rect_fill(px, size, right_x, mid_y, node_w, node_h, radius, node_bg)
        fill_rect(px, size, right_x, mid_y, strip_w, node_h, amber)

        # Bottom join node
        rounded_rect_fill(px, size, center_x, bot_y, node_w, node_h, radius, node_bg)
        fill_rect(px, size, center_x, bot_y, strip_w, node_h, node_strip)

        # Connection dots
        dot_r = max(1, size // 40)
        port_x = center_x + node_w // 2
        port_y = top_y + node_h + dot_r
        for dy in range(-dot_r, dot_r + 1):
            for dx in range(-dot_r, dot_r + 1):
                if dx * dx + dy * dy <= dot_r * dot_r:
                    off = ((port_y + dy) * size + (port_x + dx)) * 4
                    if 0 <= off < len(px) - 3:
                        px[off:off + 4] = rgba(255, 255, 255)

    return bytes(px)


def build_ico(sizes=(16, 32, 48, 256)) -> bytes:
    """Build an ICO file with embedded PNG images at each size."""
    png_data_list = []
    for size in sizes:
        pixels = draw_icon(size)
        png = make_png(size, size, pixels)
        png_data_list.append(png)

    # ICO header
    header = struct.pack("<HHH", 0, 1, len(sizes))

    offset = 6 + 16 * len(sizes)
    directory = b""
    for size, png in zip(sizes, png_data_list):
        w = size if size < 256 else 0
        h = size if size < 256 else 0
        directory += struct.pack("<BBBBHHII", w, h, 0, 0, 1, 32, len(png), offset)
        offset += len(png)

    body = b"".join(png_data_list)
    return header + directory + body


def main():
    ico_path = "resources/icons/app-icon.ico"
    ico_data = build_ico((16, 32, 48, 256))
    with open(ico_path, "wb") as f:
        f.write(ico_data)
    print(f"Wrote {ico_path} ({len(ico_data)} bytes)")


if __name__ == "__main__":
    main()
