"""Generate branded NSIS installer assets for FanTune.

Creates:
  - FanTune.ico   (multi-res icon: 16, 32, 48, 256)
  - header.bmp    (150x57  banner)
  - welcome.bmp   (164x314 sidebar panel)
"""

from PIL import Image, ImageDraw
import math, struct, io, os

# ═══════════════════════════════════════════════════════════════════
#  Colour palette (matches FanTuneLookAndFeel.h)
# ═══════════════════════════════════════════════════════════════════
CHASSIS        = (0x11, 0x11, 0x18)
CHASSIS_DARK   = (0x0A, 0x0A, 0x12)
CYAN_PRIMARY   = (0x00, 0xDC, 0xFF)
CYAN_DIM       = (0x55, 0x00, 0xDC, 0xFF)  # rgba
CYAN_FAINT     = (0x1A, 0x00, 0xDC, 0xFF)  # rgba
TEXT_PRIMARY   = (0xE0, 0xF8, 0xFF)
STEEL_LIGHT    = (0x1C, 0x1C, 0x28)
STEEL_MID      = (0x14, 0x14, 0x20)

def _rgb(c):
    return c[:3]

def _rgba(c, a=255):
    return (c[0], c[1], c[2], a)

def _lerp_color(a, b, t):
    return tuple(int(ac + (bc - ac) * t) for ac, bc in zip(a, b))

def make_fan_blade_path(cx, cy, radius, width, height, offset_angle=0):
    """Return list of (x,y) for a single fan blade as a closed polygon."""
    import math
    pts = []
    n = 16
    sweep = 0.75  # radians each blade occupies
    start_a = offset_angle - sweep * 0.5
    end_a = offset_angle + sweep * 0.5
    for i in range(n + 1):
        t = i / n
        a = start_a + (end_a - start_a) * t
        r = radius * (0.25 + 0.75 * (1 - abs(t - 0.5) * 1.6))
        pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    # close
    return pts

def draw_fan_icon(draw, cx, cy, radius, glow=0.0, angle=0.0):
    """Draw 3-blade fan icon center at (cx,cy)."""
    n_blades = 3
    colors = [
        (0x00, 0xB4, 0xDC),
        (0x00, 0xCA, 0xF0),
        (0x00, 0xDC, 0xFF),
    ]
    for i in range(n_blades):
        blade_angle = angle + (i / n_blades) * 2 * math.pi
        pts = make_fan_blade_path(cx, cy, radius, 0, 0, blade_angle)
        draw.polygon(pts, fill=colors[i % len(colors)])
        # thin edge highlight
        draw.line(pts + [pts[0]], fill=_rgb(CYAN_PRIMARY), width=1)

    shroud_r = radius + 3
    for ring_r in [shroud_r * 0.88, shroud_r * 0.72, shroud_r * 0.56, shroud_r * 0.40, shroud_r * 0.24]:
        draw.ellipse([cx - ring_r, cy - ring_r, cx + ring_r, cy + ring_r],
                     outline=_rgba(CYAN_PRIMARY, 60), width=1)

    # hub
    hub_r = max(4, radius * 0.22)
    draw.ellipse([cx - hub_r, cy - hub_r, cx + hub_r, cy + hub_r],
                 fill=(0x2A, 0x30, 0x40), outline=_rgb(CYAN_PRIMARY), width=2)
    dot_r = max(2, hub_r * 0.4)
    draw.ellipse([cx - dot_r, cy - dot_r, cx + dot_r, cy + dot_r],
                 fill=_rgb(TEXT_PRIMARY))


def make_icon(size, angle=0.0):
    """Create a single-size icon frame."""
    img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    bg_r = size // 2
    draw.rounded_rectangle([0, 0, size-1, size-1], radius=size*0.1,
                           fill=CHASSIS, outline=_rgb(CYAN_PRIMARY))
    margin = size * 0.15
    cx = cy = size // 2
    radius = int(cx - margin)
    draw_fan_icon(draw, cx, cy, radius, angle=angle)
    return img


# ═══════════════════════════════════════════════════════════════════
#  1. FanTune.ico
# ═══════════════════════════════════════════════════════════════════
def create_ico(filepath, sizes=(16, 32, 48, 64, 128, 256)):
    """Create .ico file with multiple resolutions."""
    images = []
    for s in sizes:
        angle = (6 - s / 256 * 6) * 0.08  # subtle rotation variation per size
        img = make_icon(s, angle=angle)
        images.append(img)

    # ICO format
    buf = io.BytesIO()
    # ICO header
    buf.write(struct.pack('<HHH', 0, 1, len(images)))

    offsets = []
    # directory entries (write later, we need offsets of actual image data)
    dir_entries = []
    for img in images:
        w, h = img.size
        bpp = 32
        # convert to BGRA for ICO
        r, g, b, a = img.split()
        bgra = Image.merge('RGBA', (b, g, r, a))
        data = bgra.tobytes()
        # BMP info header (40 bytes) + pixel data
        bmp_header = struct.pack('<IiiHHIIiiII',
            40,                # sizeof(BITMAPINFOHEADER)
            w, h * 2,          # width, height * 2 (ICO stores height double)
            1,                 # planes
            bpp,               # bit count
            0,                 # compression
            len(data),         # size of raw data
            0, 0, 0, 0)        # resolution, colors, important colors
        pixel_data = bmp_header + data
        dir_entries.append((w, h, len(pixel_data), 0))
        offsets.append(None)

    # write directory entries first to know offsets
    data_start = 6 + 16 * len(images)
    current_offset = data_start
    for i, (w, h, size, _) in enumerate(dir_entries):
        # In ICO, 256 is stored as 0 (single byte can't hold 256)
        w_byte = 0 if w == 256 else w
        h_byte = 0 if h == 256 else h
        buf.write(struct.pack('<BBBBHHII', w_byte, h_byte, 0, 0, 1, 32, size, current_offset))
        current_offset += size

    # write image data
    for img in images:
        w, h = img.size
        r, g, b, a = img.split()
        bgra = Image.merge('RGBA', (b, g, r, a))
        data = bgra.tobytes()
        bmp_header = struct.pack('<IiiHHIIiiII',
            40, w, h * 2, 1, 32, 0, len(data), 0, 0, 0, 0)
        buf.write(bmp_header + data)

    with open(filepath, 'wb') as f:
        f.write(buf.getvalue())
    print(f"  Created {filepath} ({len(images)} sizes)")


# ═══════════════════════════════════════════════════════════════════
#  2. header.bmp  (150 x 57)
# ═══════════════════════════════════════════════════════════════════
def create_header_bmp(filepath, width=150, height=57):
    """NSIS header image — dark gradient with brand text + mini fan icon."""
    img = Image.new('RGBA', (width, height))
    draw = ImageDraw.Draw(img)

    # dark gradient left→right
    for x in range(width):
        t = x / width
        c = _lerp_color(STEEL_MID, CHASSIS_DARK, t)
        draw.line([(x, 0), (x, height-1)], fill=c)

    # top/bottom subtle cyan border
    draw.line([(0, 0), (width-1, 0)], fill=_rgba(CYAN_PRIMARY, 40), width=1)
    draw.line([(0, height-1), (width-1, height-1)], fill=_rgba(CYAN_PRIMARY, 30), width=1)

    # Right side: mini fan icon
    icon_r = 16
    icon_cx = width - 30
    icon_cy = height // 2
    draw.ellipse([icon_cx - icon_r, icon_cy - icon_r, icon_cx + icon_r, icon_cy + icon_r],
                 outline=_rgba(CYAN_PRIMARY, 80), width=1)
    draw_fan_icon(draw, icon_cx, icon_cy, icon_r * 0.8, angle=0.3)

    # Left side: "FANTUNE" text (draw pixel text manually since no font file)
    text = "FANTUNE"
    try:
        draw.text((16, height//2 - 8), text, fill=_rgb(TEXT_PRIMARY))
        draw.text((15, height//2 - 9), text, fill=_rgba(CYAN_PRIMARY, 60))  # glow
    except:
        pass  # fallback: just draw with default font

    # subtitle
    try:
        draw.text((16, height//2 + 8), "BOX FAN PITCH CORRECTION", fill=_rgba(TEXT_PRIMARY, 120))
    except:
        pass

    # Save as 24-bit BMP
    rgb_img = Image.new('RGB', img.size, (0,0,0))
    rgb_img.paste(img, mask=img.split()[3] if img.mode == 'RGBA' else None)
    rgb_img.save(filepath, 'BMP')
    print(f"  Created {filepath} ({width}x{height})")


# ═══════════════════════════════════════════════════════════════════
#  3. welcome.bmp  (164 x 314)
# ═══════════════════════════════════════════════════════════════════
def create_welcome_bmp(filepath, width=164, height=314):
    """NSIS welcome/finish page sidebar — fan art scene."""
    img = Image.new('RGBA', (width, height))
    draw = ImageDraw.Draw(img)

    # vertical gradient (top→bottom)
    for y in range(height):
        t = y / height
        c = _lerp_color((0x0E, 0x0E, 0x1A), (0x18, 0x18, 0x28), t)
        draw.line([(0, y), (width-1, y)], fill=c)

    # right border glow
    draw.line([(width-1, 0), (width-1, height-1)], fill=_rgba(CYAN_PRIMARY, 50), width=2)

    # Large fan icon center
    cx, cy = width // 2, height // 2 - 20
    fan_r = min(width, height) // 3

    # Glow behind fan
    glow_r = fan_r + 20
    for gr in range(glow_r, fan_r, -1):
        t = (gr - fan_r) / (glow_r - fan_r)
        alpha = int(30 * (1 - t))
        draw.ellipse([cx - gr, cy - gr, cx + gr, cy + gr],
                     outline=_rgba(CYAN_PRIMARY, alpha), width=1)

    draw_fan_icon(draw, cx, cy, fan_r, glow=0.5, angle=math.pi/6)

    # Text below fan
    brand_y = cy + fan_r + 20
    try:
        draw.text((width//2, brand_y), "FanTune", fill=_rgb(TEXT_PRIMARY), anchor="mt")
        draw.text((width//2, brand_y + 2), "FanTune", fill=_rgba(CYAN_PRIMARY, 80), anchor="mt")
    except:
        pass

    try:
        draw.text((width//2, brand_y + 16),
                  "Box Fan Pitch", fill=_rgba(TEXT_PRIMARY, 160), anchor="mt")
        draw.text((width//2, brand_y + 29),
                  "Correction", fill=_rgba(TEXT_PRIMARY, 140), anchor="mt")
    except:
        pass

    # Bottom decorative line
    line_y = height - 18
    draw.line([(20, line_y), (width-20, line_y)], fill=_rgba(CYAN_PRIMARY, 60), width=1)
    try:
        draw.text((width//2, line_y + 4), "v1.0.0", fill=_rgba(CYAN_PRIMARY, 100), anchor="mt")
    except:
        pass

    # Save as 24-bit BMP
    rgb_img = Image.new('RGB', img.size, (0,0,0))
    rgb_img.paste(img, mask=img.split()[3] if img.mode == 'RGBA' else None)
    rgb_img.save(filepath, 'BMP')
    print(f"  Created {filepath} ({width}x{height})")


# ═══════════════════════════════════════════════════════════════════
#  Main
# ═══════════════════════════════════════════════════════════════════
if __name__ == "__main__":
    out_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                           "resources", "installer")
    os.makedirs(out_dir, exist_ok=True)

    print("Generating installer assets in:", out_dir)

    create_ico(os.path.join(out_dir, "FanTune.ico"))
    create_header_bmp(os.path.join(out_dir, "header.bmp"))
    create_welcome_bmp(os.path.join(out_dir, "welcome.bmp"))

    print("\nDone. Assets ready for CPack/NSIS build.")
