#!/usr/bin/env python3
# RISC OS port, copyright David Sharp 2026. GPL-3.0 (see dhewm3/COPYING.txt).
"""Draw the !dhewm3 icon: a stylised Mars, lit from one side and fading into darkness.

Procedural (no source art): a sphere with a Lambert-lit day side, rust colours with
value-noise mottling and darker "maria", limb darkening, a thin warm glow along the lit
edge, and a long soft terminator so the night side fades to near-black.

Usage: make-icon.py [output.png] [--light X,Y,Z] [--seed N]
  default output riscos/icons/dhewm3-icon.png (256x256 RGBA, transparent outside the disc).
  make-sprites.py then turns it into !Sprites and !Sprites22.
"""
import argparse
import os

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def value_noise(size, cells, rng):
    """Smooth noise: random values on a grid, bicubic-ish (smoothstep) interpolation."""
    g = rng.random((cells + 1, cells + 1))
    t = np.linspace(0, cells, size, endpoint=False)
    i = t.astype(int)
    f = t - i
    f = f * f * (3 - 2 * f)
    a = g[i][:, i]
    b = g[i][:, i + 1]
    c = g[i + 1][:, i]
    d = g[i + 1][:, i + 1]
    fx = f[None, :]
    fy = f[:, None]
    return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy


def fbm(size, rng, octaves=5, base=4):
    out = np.zeros((size, size))
    amp, total = 1.0, 0.0
    for o in range(octaves):
        out += amp * value_noise(size, base * 2 ** o, rng)
        total += amp
        amp *= 0.5
    return out / total


def draw(size, light, seed, ss=4):
    n = size * ss
    rng = np.random.default_rng(seed)
    r = 0.47                                    # disc radius (of the image width)
    y, x = np.mgrid[0:n, 0:n] + 0.5
    x = (x / n - 0.5) / r
    y = (0.5 - y / n) / r                       # +y up
    d2 = x * x + y * y
    inside = d2 <= 1.0
    z = np.sqrt(np.clip(1 - d2, 0, 1))

    L = np.array(light, float)
    L /= np.linalg.norm(L)
    ndotl = x * L[0] + y * L[1] + z * L[2]
    # long, soft terminator: light falls off smoothly well into the night side
    day = np.clip((ndotl + 0.18) / 0.95, 0, 1) ** 1.35 * 1.3

    # surface: mottling and darker regions, mapped on the sphere (approximately)
    u = (np.arctan2(x, z) / np.pi + 1) / 2
    v = (np.arcsin(np.clip(y, -1, 1)) / np.pi + 0.5)
    tex = fbm(n, rng, octaves=5, base=3)
    maria = fbm(n, rng, octaves=3, base=2)
    ui = np.clip((u * (n - 1)).astype(int), 0, n - 1)
    vi = np.clip((v * (n - 1)).astype(int), 0, n - 1)
    t = tex[vi, ui]
    m = maria[vi, ui]

    light_rust = np.array([1.0, 0.60, 0.32])
    dark_rust = np.array([0.50, 0.16, 0.07])
    k = np.clip((t - 0.32) * 2.6, 0, 1)
    k = np.clip(k - np.clip((m - 0.52) * 3.5, 0, 0.75), 0, 1)
    albedo = dark_rust[None, None, :] * (1 - k[..., None]) + light_rust[None, None, :] * k[..., None]

    limb = 0.65 + 0.35 * z                      # darker towards the edge
    col = albedo * (day * limb)[..., None]

    # warm atmospheric glow hugging the lit limb
    rim = np.clip(1 - z, 0, 1) ** 3 * np.clip(ndotl + 0.15, 0, 1)
    col += np.array([1.0, 0.55, 0.25])[None, None, :] * (rim * 0.55)[..., None]

    # the night side doesn't go fully black, so the disc keeps its shape on the icon bar
    night = np.array([0.10, 0.03, 0.02])
    col = np.maximum(col, night[None, None, :] * (0.6 + 0.4 * z)[..., None])
    col = np.clip(col, 0, 1)

    rgba = np.zeros((n, n, 4))
    rgba[..., :3] = col
    rgba[..., 3] = inside
    img = Image.fromarray((rgba * 255 + 0.5).astype(np.uint8), "RGBA")
    return img.resize((size, size), Image.LANCZOS)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out", nargs="?", default=os.path.join(ROOT, "riscos", "icons", "dhewm3-icon.png"))
    ap.add_argument("--light", default="0,0.85,0.5")	# from above, as on the title screen
    ap.add_argument("--seed", type=int, default=3)
    a = ap.parse_args()
    draw(256, [float(v) for v in a.light.split(",")], a.seed).save(a.out)
    print(a.out)


main()
