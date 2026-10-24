"""Turn the raw captures of the `showcase` e2e scenario into docs/showcase/.

    pwsh -NoProfile -File tools/e2e/run.ps1 -Scenario showcase
    python tools/e2e/showcase-media.py [--ffmpeg path\\to\\ffmpeg.exe]

Needs Pillow (with WebP) and numpy; ffmpeg for the MP4. Captures are native 1600x900 (GUI scale 3):
crops keep native pixels or scale them up by whole factors (nearest neighbour), so the pixel art
stays crisp. Animations keep the real timing of every captured frame (frames.csv).
docs/showcase/showcase.json is written by hand; this script only makes the media files.
"""

import argparse
import os
import shutil
import subprocess
import tempfile
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
RAW = ROOT / "tools" / "e2e" / "out" / "showcase"
DST = ROOT / "docs" / "showcase"

# Native 1600x900 geometry: hearts x 528..768, hunger bar x 831..1071, both at y 777..804.
HUD_BARS = (528, 777, 1072, 805)
HUD_STRIP = (496, 712, 1104, 900)   # 608x188 strip with the bars, shown 2x
WIDE = (160, 180, 1440, 900)        # 1280x720 native, bottom-centred (keeps the HUD)
INVENTORY = (160, 90, 1440, 810)    # 1280x720 native, centred on the inventory screen
SCENE_TOP = (160, 0, 1440, 720)     # 1280x720 native, above the HUD


def frames(name):
    d = RAW / name
    rows = [line.split(",") for line in (d / "frames.csv").read_text().split()]
    return [(d / f"{int(i):04d}.bmp", int(t)) for i, t in rows]


def bar_brightness(path):
    a = np.asarray(Image.open(path).convert("RGB").crop(HUD_BARS), dtype=np.float32)
    return float(a.mean())


def x2(img):
    return img.resize((img.width * 2, img.height * 2), Image.NEAREST)


def save_webp(img, name, quality=85):
    out = DST / name
    img.convert("RGB").save(out, "WEBP", quality=quality, method=6)
    print(f"{name}: {img.width}x{img.height}, {out.stat().st_size // 1024} KB")


def peak_frame(name):
    """Frame of a burst where the flashing preview is brightest (alpha at its peak)."""
    fs = frames(name)
    return max(fs, key=lambda f: bar_brightness(f[0]))[0]


def flash_loop(name, crop, out_name, period_ms=1600, quality=80):
    """One flash cycle (32 ticks = 1.6 s) as a seamless animated WebP, starting where the
    preview is invisible."""
    fs = frames(name)
    b = [bar_brightness(p) for p, _ in fs]
    # Start in the dark part of the cycle, early enough for a full period to follow.
    usable = [i for i, (_, t) in enumerate(fs) if t + period_ms <= fs[-1][1]]
    start = min(usable, key=lambda i: b[i])
    t0 = fs[start][1]
    sel = [i for i in range(start, len(fs)) if fs[i][1] < t0 + period_ms]
    imgs, durs = [], []
    for k, i in enumerate(sel):
        nxt = fs[sel[k + 1]][1] if k + 1 < len(sel) else t0 + period_ms
        imgs.append(x2(Image.open(fs[i][0]).convert("RGB").crop(crop)))
        durs.append(max(10, nxt - fs[i][1]))
    out = DST / out_name
    imgs[0].save(out, "WEBP", save_all=True, append_images=imgs[1:], duration=durs, loop=0,
                 quality=quality, method=6)
    print(f"{out_name}: {imgs[0].width}x{imgs[0].height}, {len(imgs)} frames, "
          f"{sum(durs)} ms, {out.stat().st_size // 1024} KB")


def video(name, crop, out_name, ffmpeg, crf=28, start_ms=0, end_ms=None):
    fs = [f for f in frames(name) if f[1] >= start_ms and (end_ms is None or f[1] <= end_ms)]
    with tempfile.TemporaryDirectory() as tmp:
        lines = []
        for k, (p, t) in enumerate(fs):
            png = Path(tmp) / f"{k:04d}.png"
            Image.open(p).convert("RGB").crop(crop).save(png, compress_level=1)
            dur = (fs[k + 1][1] - t) if k + 1 < len(fs) else 33
            lines += [f"file '{png.as_posix()}'", f"duration {dur / 1000:.3f}"]
        lines.append(f"file '{(Path(tmp) / f'{len(fs) - 1:04d}.png').as_posix()}'")
        lst = Path(tmp) / "list.txt"
        lst.write_text("\n".join(lines) + "\n")
        out = DST / out_name
        subprocess.run([ffmpeg, "-y", "-loglevel", "error", "-f", "concat", "-safe", "0", "-i", str(lst),
                        "-vf", "fps=30", "-c:v", "libx264", "-preset", "slow", "-crf", str(crf),
                        "-pix_fmt", "yuv420p", "-an", "-movflags", "+faststart", str(out)], check=True)
    print(f"{out_name}: {crop[2] - crop[0]}x{crop[3] - crop[1]}, {(fs[-1][1] - fs[0][1]) / 1000:.1f} s, "
          f"{out.stat().st_size // 1024} KB")
    return fs


def tooltip_box(path, mouse_x):
    """Bounding box of the tooltip right of the cursor: its fill colour (27, 12, 27) fills most
    of every row and column of the box, outer frame included."""
    a = np.asarray(Image.open(path).convert("RGB"), dtype=np.int32)[:, mouse_x:]
    fill = (a[..., 0] == 27) & (a[..., 1] == 12) & (a[..., 2] == 27)
    cols, rows = fill.sum(axis=0), fill.sum(axis=1)
    xs = np.nonzero(cols > 0.6 * cols.max())[0]
    ys = np.nonzero(rows > 0.6 * rows.max())[0]
    return mouse_x + int(xs[0]), int(ys[0]), mouse_x + int(xs[-1]) + 1, int(ys[-1]) + 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ffmpeg", default=shutil.which("ffmpeg") or "ffmpeg")
    args = ap.parse_args()
    DST.mkdir(parents=True, exist_ok=True)

    # README hero: one seamless flash cycle while holding cooked beef.
    flash_loop("burst-cooked_beef", HUD_STRIP, "hud-preview.webp")

    # Eating clip (true speed) and its poster.
    fs = video("video-eat", WIDE, "eat.mp4", args.ffmpeg)
    save_webp(Image.open(peak_frame("burst-cooked_beef")).crop(WIDE), "eat.webp")

    # Stills at the brightest moment of each preview.
    save_webp(x2(Image.open(peak_frame("burst-golden_apple")).crop(HUD_STRIP)), "hud-golden_apple.webp")
    save_webp(x2(Image.open(peak_frame("burst-rotten_flesh")).crop(HUD_STRIP)), "hud-rotten_flesh.webp")
    save_webp(x2(Image.open(RAW / "saturation.png").crop(HUD_STRIP)), "saturation.webp")
    save_webp(Image.open(RAW / "debug.png").convert("RGB").resize((1280, 720), Image.LANCZOS), "debug.webp")

    # A tooltip in its inventory screen.
    save_webp(Image.open(RAW / "tooltip-golden_apple.png").crop(INVENTORY), "tooltip.webp")

    # Four tooltips, 2x, over the dimmed scene.
    foods = ["cooked_beef", "golden_apple", "golden_carrot", "rotten_flesh"]
    order = ["cooked_beef", "golden_apple", "rotten_flesh", "golden_carrot", "bread", "pumpkin_pie", "cookie", "cake"]
    boxes = []
    for f in foods:
        mx = int((0.50 + 0.034 * order.index(f)) * 1600)
        p = RAW / f"tooltip-{f}.png"
        box = Image.open(p).convert("RGB").crop(tooltip_box(p, mx))
        # The box has rounded corners: in the corners, keep only the tooltip's own colours.
        px = np.asarray(box, dtype=np.int32)
        own = ((px == (27, 12, 27)).all(axis=-1) | (px == (44, 8, 99)).all(axis=-1))
        keep = np.ones(own.shape, dtype=bool)
        n = 12
        for sy in (slice(0, n), slice(-n, None)):
            for sx in (slice(0, n), slice(-n, None)):
                keep[sy, sx] = own[sy, sx]
        mask = Image.fromarray((keep * 255).astype(np.uint8), "L")
        boxes.append((x2(box), x2(mask)))
    bg = Image.open(RAW / "saturation.png").convert("RGB").crop(SCENE_TOP)
    bg = Image.blend(bg, Image.new("RGB", bg.size, (0, 0, 0)), 0.45)
    cols, rows = 2, 2
    cw = max(b.width for b, _ in boxes)
    rh = max(b.height for b, _ in boxes)
    gx = (1280 - cols * cw) // (cols + 1)
    gy = (720 - rows * rh) // (rows + 1)
    for k, (b, m) in enumerate(boxes):
        c, r = k % cols, k // cols
        x = gx + c * (cw + gx) + (cw - b.width) // 2
        y = gy + r * (rh + gy) + (rh - b.height) // 2
        bg.paste(b, (x, y), m)
    save_webp(bg, "tooltips.webp")

    total = sum(p.stat().st_size for p in DST.iterdir())
    print(f"docs/showcase total: {total / 1e6:.2f} MB")


if __name__ == "__main__":
    main()
