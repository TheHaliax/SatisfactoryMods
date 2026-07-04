# SPDX-FileCopyrightText: 2026 Haliax
# SPDX-License-Identifier: GPL-3.0-or-later
"""Generate the mod icons (1024/512/128) from one layered control mask.

The mask encodes three roles by pure channel colour so artwork and text placement
live in a single registered file: blue = lineart, red = mod-name box, green =
version box. Name and version are read from the .uplugin so the icon can never
drift from the shipped version. Everything composites at full size then downscales,
which keeps thin lineart, the outline, and the duotone shadow crisp at 128.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

OUTPUT_SIZES = (1024, 512, 128)
AUTHORED_SIZE = 1024       # outline width and shadow offset/blur are authored at 1024, then scaled

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent
MOD_DIR = REPO_ROOT / "StructuralPower"


def hex_rgb(value: str) -> tuple[int, int, int]:
    v = value.lstrip("#")
    return tuple(int(v[i:i + 2], 16) for i in (0, 2, 4))


def channel_box(rgb: Image.Image, index: int):
    # Mask colours are pure, so one dominant channel uniquely isolates each role.
    return rgb.split()[index].point(lambda p: 255 if p > 128 else 0)


def fit_font(font_path: str, text: str, box_w: int, box_h: int) -> ImageFont.FreeTypeFont:
    for size in range(box_h, 7, -2):
        font = ImageFont.truetype(font_path, size)
        left, top, right, bottom = font.getbbox(text)
        if (right - left) <= box_w * 0.9 and (bottom - top) <= box_h * 0.82:
            return font
    return ImageFont.truetype(font_path, 8)


def text_alpha(size, text, font_path, box, valign="middle") -> Image.Image:
    # Uniform fit, cropped to the ink so vertical placement is exact — bottom-align lands the
    # glyph bottom on the box bottom (which coincides with the lineart bottom).
    x0, y0, x1, y1 = box
    box_w, box_h = x1 - x0, y1 - y0
    font = fit_font(font_path, text, box_w, box_h)
    left, top, right, bottom = font.getbbox(text)
    glyphs = Image.new("L", (right - left, bottom - top), 0)
    ImageDraw.Draw(glyphs).text((-left, -top), text, font=font, fill=255)
    glyphs = glyphs.crop(glyphs.getbbox())

    gw, gh = glyphs.size
    px = x0 + (box_w - gw) // 2
    py = y1 - gh if valign == "bottom" else y0 + (box_h - gh) // 2
    layer = Image.new("L", size, 0)
    layer.paste(glyphs, (px, py))
    return layer


def vertical_gradient(size, top, bottom) -> Image.Image:
    _, h = size
    column = Image.new("RGB", (1, h))
    for y in range(h):
        t = y / (h - 1)
        column.putpixel((0, y), tuple(round(top[i] * (1 - t) + bottom[i] * t) for i in range(3)))
    return column.resize(size)


def expand(alpha: Image.Image, px: int) -> Image.Image:
    # Blur-then-threshold is a rounded dilation (~px radius) with Pillow alone — no SciPy disk.
    if px <= 0:
        return alpha
    return alpha.filter(ImageFilter.GaussianBlur(px)).point(lambda p: 255 if p >= 40 else 0)


def colored_shadow(alpha, size, color, offset, blur, opacity) -> Image.Image:
    faded = alpha.point(lambda p: int(p * opacity))
    canvas = Image.new("L", size, 0)
    canvas.paste(faded, offset)            # paste (not ImageChops.offset) so it can't wrap edges
    canvas = canvas.filter(ImageFilter.GaussianBlur(blur))
    shadow = Image.new("RGBA", size, color + (0,))
    shadow.putalpha(canvas)
    return shadow


def build(args) -> None:
    manifest = json.loads(Path(args.uplugin).read_text(encoding="utf-8"))
    name = args.name or manifest["FriendlyName"]
    # Icon shows major.minor only (e.g. V2.0); the .uplugin keeps full semver for ficsit.
    version = args.version or "V" + ".".join(str(manifest["SemVersion"]).split(".")[:2])

    control = Image.open(args.mask).convert("RGB")
    size = control.size
    background = Image.open(args.background).convert("RGBA").resize(size)

    lineart = channel_box(control, 2)                  # blue strokes
    name_box = channel_box(control, 0).getbbox()       # red box -> mod name
    version_box = channel_box(control, 1).getbbox()    # green box -> version
    if name_box is None or version_box is None:
        raise SystemExit("mask is missing the red (name) or green (version) box")

    names = text_alpha(size, name, args.font, name_box)
    versions = text_alpha(size, version, args.font, version_box, valign="bottom")
    silhouette = ImageChops.lighter(ImageChops.lighter(lineart, names), versions)

    fill = vertical_gradient(size, hex_rgb(args.fill_top), hex_rgb(args.fill_bottom))
    fill_layer = fill.convert("RGBA")
    fill_layer.putalpha(silhouette)

    # Outline width is absolute at authored size: dense lineart sits <~10px apart, so scaling a
    # "2px@128" width up to 16px@1024 would merge adjacent strokes and flood the gaps.
    outline_px = max(1, round(args.outline_px * size[0] / AUTHORED_SIZE))
    ring = ImageChops.subtract(expand(silhouette, outline_px), silhouette)
    outline_layer = ImageChops.invert(fill).convert("RGBA")  # inverted gradient = max contrast
    outline_layer.putalpha(ring)

    # Shadow is cast by the whole top layer (silhouette + outline ring).
    top_alpha = ImageChops.lighter(silhouette, ring)
    scale = size[0] / AUTHORED_SIZE
    off = round(args.shadow_offset * scale)
    blur = args.shadow_blur * scale
    cyan = colored_shadow(top_alpha, size, hex_rgb(args.shadow_cyan), (-off, 0), blur, args.shadow_opacity)
    red = colored_shadow(top_alpha, size, hex_rgb(args.shadow_red), (off, 0), blur, args.shadow_opacity)

    icon = background
    for layer in (cyan, red, outline_layer, fill_layer):
        icon = Image.alpha_composite(icon, layer)

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    for px in OUTPUT_SIZES:
        icon.resize((px, px), Image.LANCZOS).save(out_dir / f"Icon{px}.png")
        print(f"wrote Icon{px}.png")
    print(f"name='{name}'  version='{version}'  name_box={name_box}  version_box={version_box}")


def main() -> None:
    p = argparse.ArgumentParser(description="Generate StructuralPower mod icons from a control mask.")
    p.add_argument("--background", default=str(SCRIPT_DIR / "BaseIcon1024.png"))
    p.add_argument("--mask", default=str(SCRIPT_DIR / "Mask1024.png"))
    p.add_argument("--font", default=r"C:\Windows\Fonts\unispace bd.ttf")
    p.add_argument("--uplugin", default=str(MOD_DIR / "StructuralPower.uplugin"))
    p.add_argument("--out-dir", default=str(MOD_DIR / "Resources"))
    p.add_argument("--name", default=None, help="override; defaults to uplugin FriendlyName")
    p.add_argument("--version", default=None, help="override; defaults to 'V'+uplugin major.minor")
    p.add_argument("--fill-top", default="#820000")
    p.add_argument("--fill-bottom", default="#000000")
    p.add_argument("--outline-px", type=float, default=3.0, help="outline width at 1024px composite")
    p.add_argument("--shadow-offset", type=float, default=12.0, help="duotone split offset at 1024px")
    p.add_argument("--shadow-blur", type=float, default=7.0)
    p.add_argument("--shadow-opacity", type=float, default=0.6)
    p.add_argument("--shadow-red", default="#820000")
    p.add_argument("--shadow-cyan", default="#7DFFFF")
    build(p.parse_args())


if __name__ == "__main__":
    main()
