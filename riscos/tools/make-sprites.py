#!/usr/bin/env python3
# RISC OS port, copyright David Sharp 2026. GPL-3.0 (see dhewm3/COPYING.txt).
"""Make the !dhewm3 application sprites from riscos/icons/dhewm3-icon.png (adapted from the OpenTS port).

Writes two RISC OS sprite files (filetype &FF9) of 32-bit sprites with a mask, each holding
"!dhewm3" and the small "sm!dhewm3":
  !Sprites22  34x34 and 18x18 at 90x90 dpi (square-pixel modes)
  !Sprites    34x17 and 18x9 at 90x45 dpi (rectangular-pixel modes)
dhewm3 needs RISC OS 5, which plots 32-bit sprites, and their alpha masks, in any mode.
ImageMagick downscales.

The picture is drawn by make-icon.py (a stylised Mars). Any other square picture whose
background is transparent works too.

Usage: make-sprites.py [picture.png [output directory]]
  (default: riscos/icons/dhewm3-icon.png into riscos/app/!dhewm3)
  PREVIEW=<dir> also writes enlarged PNGs of each sprite for checking.
  GREY=1 writes 1-bit masks with the edge blended with the Wimp's grey, instead of the
  default 8-bit alpha masks (RISC OS 5 blends those with whatever is behind the icon).
  SPRITE_NAME=<name> names the sprites <name> and sm<name> (for test apps).

Adapted from the Vanilla Conquer RISC OS port's tool of the same name.
"""
import os
import struct
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PICTURE = os.path.join(ROOT, "riscos", "icons", "dhewm3-icon.png")
NAME = os.environ.get("SPRITE_NAME", "!dhewm3")


def pack_rows(values, w, h, bpp):
    """Pack pixel values into RISC OS sprite rows (whole words, pixel 0 in the low bits)."""
    words = (w * bpp + 31) // 32
    out = bytearray()
    for y in range(h):
        bits = 0
        for x in range(w):
            bits |= values[y * w + x] << (x * bpp)
        out += bits.to_bytes(words * 4, "little")
    return words, bytes(out)


def sprite_file(sprites):
    body = b"".join(sprites)
    # A sprite file is a sprite area without its first (size) word.
    return struct.pack("<III", len(sprites), 16, 16 + len(body)) + body


def pixels(path, w, h):
    raw = subprocess.run(
        ["magick", path, "-filter", "Lanczos", "-resize", "%dx%d!" % (w, h), "-unsharp", "0x0.6+0.6+0",
         "-depth", "8", "rgba:-"], check=True, capture_output=True).stdout
    return [tuple(raw[i:i + 4]) for i in range(0, len(raw), 4)]


# Edge pixels the picture only partly covers are blended with the Wimp's light grey (&DDDDDD,
# the icon bar's and Filer windows' background), so the outline is antialiased there although
# the mask is 1 bit. Pixels covered less than this much stay transparent.
GREY = (0xDD, 0xDD, 0xDD)
EDGE = 64


def blend(p):
    a = p[3] / 255
    return tuple(int(p[i] * a + GREY[i] * (1 - a) + 0.5) for i in range(3)) + (255 if p[3] >= EDGE else 0,)


def alpha_sprite(name, pixels, w, h, xdpi, ydpi):
    """A 32bpp sprite with an 8bpp alpha mask (mode word bit 31; RISC OS 5's SpriteExtend
    blends it with whatever is behind). Colours are un-premultiplied."""
    image = b"".join(struct.pack("<I", (p[2] << 16) | (p[1] << 8) | p[0]) for p in pixels)
    words = (w + 3) // 4
    mask = b"".join(bytes(p[3] for p in pixels[y * w:(y + 1) * w]).ljust(words * 4, b"\0") for y in range(h))
    mode = (1 << 31) | (6 << 27) | (ydpi << 14) | (xdpi << 1) | 1
    header = 44
    total = header + len(image) + len(mask)
    return struct.pack("<I12sIIIIIII", total, name.encode("latin-1"), w - 1, h - 1, 0, 31,
                       header, header + len(image), mode) + image + mask


def sprite(name, pixels, w, h, xdpi, ydpi):
    """A 32bpp sprite (0xXXBBGGRR, red in the low byte) with a 1bpp mask."""
    if not os.environ.get("GREY"):
        return alpha_sprite(name, pixels, w, h, xdpi, ydpi)
    pixels = [blend(p) for p in pixels]
    image = b"".join(struct.pack("<I", (p[2] << 16) | (p[1] << 8) | p[0]) if p[3] >= 128 else b"\0\0\0\0" for p in pixels)
    mask_words, mask = pack_rows([1 if p[3] >= 128 else 0 for p in pixels], w, h, 1)
    mode = (6 << 27) | (ydpi << 14) | (xdpi << 1) | 1
    header = 44
    total = header + len(image) + len(mask)
    return struct.pack("<I12sIIIIIII", total, name.encode("latin-1"), w - 1, h - 1, 0, 31,
                       header, header + len(image), mode) + image + mask


def main():
    source = sys.argv[1] if len(sys.argv) > 1 else PICTURE
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, "riscos", "app", "!dhewm3")
    files = {"!Sprites22,ff9": [(NAME, 34, 34), ("sm" + NAME, 18, 18)],
             "!Sprites,ff9": [(NAME, 34, 17), ("sm" + NAME, 18, 9)]}
    for fname, shapes in files.items():
        spr = []
        for name, w, h in shapes:
            px = pixels(source, w, h)
            spr.append(sprite(name, px, w, h, 90, 90 if h == w else 45))
            if os.environ.get("PREVIEW"):
                raw = bytes(c for p in map(blend, px) for c in (p[:3] if p[3] >= 128 else GREY))
                subprocess.run(["magick", "-size", "%dx%d" % (w, h), "-depth", "8", "rgb:-", "-scale", "800%",
                                os.path.join(os.environ["PREVIEW"], "%s-%s.png" % (fname.split(",")[0].lstrip("!"), name.lstrip("!")))],
                               input=raw, check=True)
        with open(os.path.join(out, fname), "wb") as f:
            f.write(sprite_file(spr))
        print(os.path.join(out, fname))


main()
