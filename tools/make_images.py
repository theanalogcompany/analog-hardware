#!/usr/bin/env python3
"""
make_images.py  —  bake images into the reTerminal sketch (no SD card)

Usage:
    python make_images.py a.png b.png            # fit: contain (default)
    python make_images.py a.png b.png cover      # full-bleed, crop edges

Outputs:
    images.h        -> put next to the .ino and re-upload
    preview_A.png   -> what the panel will ACTUALLY show (panel's real colors).
    preview_B.png      Open these to judge BEFORE flashing.

Approach: full Floyd-Steinberg dithering builds your warm browns/tans out of
red+yellow+black dots (that's what makes the color read right), saturation keeps
it rich, and a GENTLE white-only cleanup removes the speckle from near-white
background areas WITHOUT touching the colored/warm regions.

------------------------------------------------------------------
TUNE THESE, then re-run and re-open the previews:
"""
SATURATION    = 1.5    # color richness. Lower toward 1.2 if too punchy.
CONTRAST      = 1.08   # slight pop.
DITHER        = 0.75   # Floyd-Steinberg strength. Lower = fewer dots but flatter
                       # color; this is what builds the browns, so keep it high.
WHITE_CLEANUP = 0.8    # 0 = leave all background speckle (raw look),
                       # 1 = cleanest white background. Only affects near-white
                       # pixels; never desaturates your warm/colored areas.
"""
------------------------------------------------------------------
"""

import sys
import numpy as np
from PIL import Image, ImageEnhance

W, H = 800, 480

# Target/distance palette, SAME order as epaper_colors[] in the sketch.
PURE = np.array([
    [0,0,0],[255,255,255],[0,255,0],[0,0,255],[255,0,0],[255,255,0]
], dtype=np.float32)

# How the 6 slots actually look on the panel -- used ONLY for the preview.
DISP = np.array([
    [40,40,40],[242,242,236],[80,120,78],[60,78,132],[170,62,50],[212,176,60]
], dtype=np.uint8)


def fit(img, mode):
    img = img.convert("RGBA")
    bg = Image.new("RGBA", img.size, (255,255,255,255))
    img = Image.alpha_composite(bg, img).convert("RGB")
    if mode == "stretch":
        img = img.resize((W, H), Image.LANCZOS)
    elif mode == "cover":
        s = max(W/img.width, H/img.height)
        r = img.resize((round(img.width*s), round(img.height*s)), Image.LANCZOS)
        l, t = (r.width-W)//2, (r.height-H)//2
        img = r.crop((l, t, l+W, t+H))
    else:
        s = min(W/img.width, H/img.height)
        r = img.resize((round(img.width*s), round(img.height*s)), Image.LANCZOS)
        c = Image.new("RGB", (W, H), (255,255,255)); c.paste(r, ((W-r.width)//2, (H-r.height)//2)); img = c
    img = ImageEnhance.Color(img).enhance(SATURATION)
    img = ImageEnhance.Contrast(img).enhance(CONTRAST)
    return np.asarray(img, dtype=np.float32)


def white_cleanup(arr):
    """Snap ONLY near-white, low-saturation pixels to pure white. Gentle by design:
    sat threshold stays low so warm washes (higher saturation) are never touched."""
    if WHITE_CLEANUP <= 0:
        return arr
    lum_thr = 245 - 18 * WHITE_CLEANUP
    sat_thr = 8 + 22 * WHITE_CLEANUP
    lum = 0.299*arr[...,0] + 0.587*arr[...,1] + 0.114*arr[...,2]
    sat = arr.max(2) - arr.min(2)
    m = (lum > lum_thr) & (sat < sat_thr)
    out = arr.copy(); out[m] = [255,255,255]
    return out


def floyd_steinberg(arr):
    h, w, _ = arr.shape
    buf = arr.copy(); out = np.zeros((h, w), np.uint8)
    for y in range(h):
        row = buf[y]
        for x in range(w):
            old = row[x].copy()
            i = int(((old - PURE)**2).sum(1).argmin()); out[y, x] = i
            e = (old - PURE[i]) * DITHER
            if x+1 < w:        row[x+1]   += e*0.4375
            if y+1 < h:
                nr = buf[y+1]
                if x > 0:      nr[x-1]    += e*0.1875
                nr[x]          += e*0.3125
                if x+1 < w:    nr[x+1]    += e*0.0625
    return out


def convert(img, mode):
    idx = floyd_steinberg(white_cleanup(fit(img, mode)))
    return idx.ravel(), Image.fromarray(DISP[idx], "RGB")


def emit(f, name, idx):
    f.write(f"const uint8_t {name}[{W*H}] = {{\n")
    line = []
    for v in idx:
        line.append(str(int(v)))
        if len(line) == 40:
            f.write(",".join(line) + ",\n"); line = []
    if line:
        f.write(",".join(line) + "\n")
    f.write("};\n\n")


# ---- mono (1-bit B/W) path for the E1001 / GxEPD2_BW, separate from the 6-color
# code above. Reuses fit() for identical framing, then Floyd-Steinberg dithers to
# 1-bit via PIL and packs 8 px/byte MSB-first (1=white, 0=black: GxEPD2 convention).
def convert_mono(img, mode):
    arr = fit(img, mode)
    rgb = np.clip(arr, 0, 255).astype(np.uint8)
    gray = Image.fromarray(rgb, "RGB").convert("L")   # luminance
    bw = gray.convert("1")                            # Floyd-Steinberg -> 1-bit
    return bw                                          # PIL "1" image == its own preview


def emit_mono(f, name, bw):
    packed = bw.tobytes()                             # 1bpp, MSB-first, 1=white, rows byte-padded
    f.write(f"const uint8_t {name}[{len(packed)}] = {{\n")
    line = []
    for v in packed:
        line.append(str(int(v)))
        if len(line) == 40:
            f.write(",".join(line) + ",\n"); line = []
    if line:
        f.write(",".join(line) + "\n")
    f.write("};\n\n")


def main():
    mode = "contain"
    mono = False
    args = sys.argv[1:]
    if args and args[-1] == "mono":
        mono = True; args.pop()
    if args and args[-1] in ("cover", "contain", "stretch"):
        mode = args.pop()
    if len(args) != 2:
        print("Usage: python make_images.py a.png b.png [cover|contain|stretch] [mono]")
        return
    import time; t = time.time()
    if mono:
        print(f"MONO 1-bit B/W path  fit={mode}")
        ba = convert_mono(Image.open(args[0]), mode)
        bb = convert_mono(Image.open(args[1]), mode)
        ba.save("preview_A.png"); bb.save("preview_B.png")
        with open("../src/images.h", "w") as f:
            f.write("// Auto-generated by make_images.py (mono 1-bpp, 8 px/byte MSB-first)\n")
            f.write("#pragma once\n#include <stdint.h>\n\n")
            f.write(f"#define IMG_W {W}\n#define IMG_H {H}\n\n")
            emit_mono(f, "IMG_RESTING", ba); emit_mono(f, "IMG_THOUGHTS", bb)
        print(f"Wrote mono images.h + preview_A.png + preview_B.png in {time.time()-t:.1f}s")
        return
    print(f"sat={SATURATION} contrast={CONTRAST} dither={DITHER} white_cleanup={WHITE_CLEANUP} fit={mode}")
    ia, pa = convert(Image.open(args[0]), mode)
    ib, pb = convert(Image.open(args[1]), mode)
    pa.save("preview_A.png"); pb.save("preview_B.png")
    with open("../src/images.h", "w") as f:
        f.write("// Auto-generated by make_images.py\n#pragma once\n#include <stdint.h>\n\n")
        f.write(f"#define IMG_W {W}\n#define IMG_H {H}\n\n")
        emit(f, "IMG_RESTING", ia); emit(f, "IMG_THOUGHTS", ib)
    print(f"Wrote images.h + preview_A.png + preview_B.png in {time.time()-t:.1f}s")
    print(">>> Open the previews. Good? Drop images.h next to the .ino and re-upload.")


if __name__ == "__main__":
    main()