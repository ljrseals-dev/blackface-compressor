#!/usr/bin/env python3
"""Generates the Blackface UI artwork and source/BlackfaceLayout.h.

Everything static is baked here (panel, labels, tick marks, knob caps, VU dial,
glass). Everything that moves (arcs, pointers, needle, lit LEDs, readouts) is drawn
live by the VSTGUI views, using the same numbers from the generated header.

  python3 tools/make_assets.py [--art DIR]

--art DIR   folder holding the two tomb-painting JPGs (13739.jpg, 13740.jpg).
            Cropped copies are cached in tools/art/ so the script runs without them.
"""
import argparse, math, os, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter, ImageChops

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
RES = os.path.join(ROOT, "resource")
SRC = os.path.join(ROOT, "source")
ART = os.path.join(HERE, "art")
os.makedirs(RES, exist_ok=True)
os.makedirs(ART, exist_ok=True)

FONT_DIR = "/usr/share/fonts/opentype/inter/"
def font(name, px):
    for p in (FONT_DIR + name, "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"):
        if os.path.exists(p):
            return ImageFont.truetype(p, int(px))
    return ImageFont.load_default()

# ----------------------------------------------------------------------------- layout
W, H = 1120, 550
L = dict(
    W=W, H=H,
    # big knobs: centre, cap diameter, ring radius, tick radii
    input_c=(150, 278), output_c=(385, 278), big_cap=140, big_ring=80,
    # small knobs
    attack_c=(555, 202), release_c=(555, 374), small_cap=56, small_ring=36,
    # bottom knobs
    mix_c=(100, 490), hpf_c=(190, 490), color_c=(280, 490), mini_cap=40, mini_ring=26,
    # buttons (x, y, w, h)
    ratio_x=640, ratio_y=152, ratio_w=66, ratio_h=46, ratio_gap=8,
    meter_x=1034, meter_y=152, meter_w=62, meter_h=46, meter_gap=8,
    det_x=360, det_y=470, det_w=54, det_h=30, det_gap=6,
    os_x=570, os_y=470, os_w=42, os_h=30, os_gap=6,
    prev=(790, 466, 26, 38), name=(820, 466, 150, 38), next=(974, 466, 26, 38),
    power_c=(1062, 490), power_r=22,
    # VU window (x, y, w, h) and dial geometry (relative to window)
    vu=(740, 152, 270, 185), vu_pivot=(135, 172), vu_arc_r=120, vu_sweep=46.0, vu_needle=124,
    sweep_deg=135.0,  # knob sweep: -135 .. +135 from 12 o'clock
)
VU_MARKS_MAJOR = [-20, -10, -7, -5, -3, 0, 1, 2, 3]
VU_MARKS_MINOR = [-15, -9, -8, -6, -4, -2, -1]
def vu_pos(vu):  # standard VU taper: 0 VU at ~71 % of the arc, +3 VU at 100 %
    return (10 ** (vu / 20.0)) / (10 ** (3 / 20.0))
def vu_angle(vu):
    return -L["vu_sweep"] + 2 * L["vu_sweep"] * vu_pos(vu)

# ----------------------------------------------------------------------------- helpers
SS = 4  # supersample, then downsample to @2x and @1x

def rgba(w, h, c=(0, 0, 0, 0)):
    return Image.new("RGBA", (int(w), int(h)), c)

def polar(c, r, deg):  # deg from 12 o'clock, clockwise
    a = math.radians(deg)
    return (c[0] + r * math.sin(a), c[1] - r * math.cos(a))

def spaced_text(d, xy, text, fnt, fill, spacing=0.0, anchor="mm"):
    """Letter-spaced text centred on xy (anchor mm only)."""
    widths = [d.textlength(ch, font=fnt) for ch in text]
    total = sum(widths) + spacing * (len(text) - 1)
    x = xy[0] - total / 2
    for ch, w in zip(text, widths):
        d.text((x + w / 2, xy[1]), ch, font=fnt, fill=fill, anchor="mm")
        x += w + spacing

def S(v):  # scale a layout number to the supersampled canvas
    return int(round(v * SS))

def downsample(img, scale):
    w, h = img.size
    return img.resize((int(round(w * scale / SS)), int(round(h * scale / SS))), Image.LANCZOS)

def save_pair(img, name):
    downsample(img, 2).save(os.path.join(RES, f"{name}@2x.png"), optimize=True)
    downsample(img, 1).save(os.path.join(RES, f"{name}.png"), optimize=True)

# ----------------------------------------------------------------------------- panel parts
def brushed_metal(w, h, seed=7):
    rng = np.random.default_rng(seed)
    noise = rng.normal(0, 1, (h, w // 8 + 2)).astype(np.float32)
    noise = np.array(Image.fromarray(noise).resize((w, h), Image.BICUBIC))
    streak = np.array(Image.fromarray(((noise - noise.min()) / (np.ptp(noise) + 1e-6) * 255).astype(np.uint8))
                      .filter(ImageFilter.GaussianBlur(1.2))).astype(np.float32) / 255.0
    y = np.linspace(0, 1, h, dtype=np.float32)[:, None]
    base = 22 + 10 * (1 - y) + 7 * (streak - 0.5)
    x = np.linspace(-1, 1, w, dtype=np.float32)[None, :]
    base = base - 5 * x * x                       # darker toward the sides
    rgb = np.stack([base, base, base * 1.04], -1).clip(0, 255).astype(np.uint8)
    return Image.fromarray(rgb, "RGB").convert("RGBA")

def inset_shadow(img, box, radius, depth=10, alpha=170):
    """Darken the inside edge of a rounded rect (recessed look)."""
    w, h = img.size
    mask = Image.new("L", (w, h), 0)
    ImageDraw.Draw(mask).rounded_rectangle(box, radius, fill=255)
    inv = ImageChops.invert(mask).filter(ImageFilter.GaussianBlur(S(depth)))
    sh = Image.new("RGBA", (w, h), (0, 0, 0, alpha))
    sh.putalpha(ImageChops.multiply(inv, mask))
    img.alpha_composite(sh)

def drop_shadow(img, box, radius, blur=6, off=3, alpha=150):
    w, h = img.size
    m = Image.new("L", (w, h), 0)
    b = [box[0] + S(off) * 0.4, box[1] + S(off), box[2] + S(off) * 0.4, box[3] + S(off)]
    ImageDraw.Draw(m).rounded_rectangle(b, radius, fill=alpha)
    m = m.filter(ImageFilter.GaussianBlur(S(blur)))
    sh = Image.new("RGBA", (w, h), (0, 0, 0, 255))
    sh.putalpha(m)
    img.alpha_composite(sh)

def screw(d, c, r):
    x, y = S(c[0]), S(c[1])
    rr = S(r)
    d.ellipse([x - rr, y - rr, x + rr, y + rr], fill=(14, 14, 16, 255), outline=(60, 60, 64, 255), width=S(1))
    d.ellipse([x - rr * .8, y - rr * .8, x + rr * .8, y + rr * .8], outline=(34, 34, 38, 255), width=S(1))
    for dx, dy in ((1, 0), (0, 1)):
        d.line([x - dx * rr * .6, y - dy * rr * .6, x + dx * rr * .6, y + dy * rr * .6], fill=(70, 70, 76, 255), width=S(2))

def fit_cover(im, w, h):
    s = max(w / im.width, h / im.height)
    im = im.resize((int(im.width * s) + 1, int(im.height * s) + 1), Image.LANCZOS)
    l, t = (im.width - w) // 2, (im.height - h) // 2
    return im.crop((l, t, l + w, t + h))

def tomb_plate(img, box, src_name, crop, art_dir):
    """Gold-bezelled inlay of an ancient-Egyptian tomb painting."""
    cache = os.path.join(ART, f"{os.path.splitext(src_name)[0]}_crop.jpg")
    if art_dir and os.path.exists(os.path.join(art_dir, src_name)):
        Image.open(os.path.join(art_dir, src_name)).convert("RGB").crop(crop).save(cache, quality=95)
    if not os.path.exists(cache):
        print(f"  (no art for {src_name}; plate left blank)")
        art = Image.new("RGB", (int(box[2] - box[0]), int(box[3] - box[1])), (60, 45, 30))
    else:
        art = Image.open(cache).convert("RGB")
    w, h = int(box[2] - box[0]), int(box[3] - box[1])
    art = fit_cover(art, w, h).convert("RGBA")
    # warm, slightly darkened grade so it sits in the dark panel
    art = ImageChops.multiply(art, Image.new("RGBA", art.size, (236, 224, 205, 255)))
    # vignette
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    v = 1 - 0.38 * (((xx - w / 2) / (w / 2)) ** 2 + ((yy - h / 2) / (h / 2)) ** 2) ** 1.2
    arr = np.array(art).astype(np.float32)
    arr[..., :3] *= v.clip(0.45, 1)[..., None]
    art = Image.fromarray(arr.clip(0, 255).astype(np.uint8), "RGBA")
    # bezel
    bz = S(5)
    drop_shadow(img, [box[0] - bz, box[1] - bz, box[2] + bz, box[3] + bz], S(5), blur=5, off=3, alpha=190)
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([box[0] - bz, box[1] - bz, box[2] + bz, box[3] + bz], S(5), fill=(122, 92, 38, 255),
                        outline=(214, 172, 84, 255), width=S(2))
    d.rounded_rectangle([box[0] - bz + S(2), box[1] - bz + S(2), box[2] + bz - S(2), box[3] + bz - S(2)], S(4),
                        outline=(70, 50, 20, 255), width=S(1))
    img.alpha_composite(art, (int(box[0]), int(box[1])))
    inset_shadow(img, [box[0], box[1], box[2], box[3]], S(2), depth=3, alpha=150)

def button_face(img, box, label, fnt, led=False):
    d = ImageDraw.Draw(img)
    drop_shadow(img, box, S(5), blur=3, off=2, alpha=170)
    d = ImageDraw.Draw(img)
    x0, y0, x1, y1 = box
    # body gradient
    body = Image.new("RGBA", (int(x1 - x0), int(y1 - y0)))
    g = np.linspace(1, 0, body.height)[:, None]
    arr = np.zeros((body.height, body.width, 4), np.float32)
    shade = 28 + 22 * g
    arr[..., 0] = shade; arr[..., 1] = shade; arr[..., 2] = shade * 1.04; arr[..., 3] = 255
    body = Image.fromarray(arr.astype(np.uint8), "RGBA")
    m = Image.new("L", body.size, 0)
    ImageDraw.Draw(m).rounded_rectangle([0, 0, body.width - 1, body.height - 1], S(5), fill=255)
    img.paste(body, (int(x0), int(y0)), m)
    d.rounded_rectangle(box, S(5), outline=(78, 78, 84, 255), width=S(1))
    d.rounded_rectangle([x0 + S(1), y0 + S(1), x1 - S(1), y1 - S(1)], S(4), outline=(10, 10, 12, 200), width=S(1))
    cy = (y0 + y1) / 2 + (S(5) if led else 0)
    d.text(((x0 + x1) / 2, cy), label, font=fnt, fill=(226, 218, 196, 255), anchor="mm")
    if led:
        lx, ly, lr = (x0 + x1) / 2, y0 + S(10), S(3.6)
        d.ellipse([lx - lr, ly - lr, lx + lr, ly + lr], fill=(30, 18, 8, 255), outline=(90, 70, 40, 255), width=S(1))

def knob_ticks(d, c, r0, r1, rmaj, rlab, marks, fnt, sweep):
    """marks: list of (norm, label or None, major)"""
    for n, lab, major in marks:
        deg = -sweep + 2 * sweep * n
        p0 = polar((S(c[0]), S(c[1])), S(r0), deg)
        p1 = polar((S(c[0]), S(c[1])), S(rmaj if major else r1), deg)
        d.line([p0, p1], fill=(212, 204, 182, 255) if major else (130, 126, 116, 255), width=S(1.6 if major else 1.1))
        if lab:
            pl = polar((S(c[0]), S(c[1])), S(rlab), deg)
            d.text(pl, lab, font=fnt, fill=(206, 198, 176, 255), anchor="mm")

def knob_well(img, c, r):
    w, h = img.size
    m = Image.new("L", (w, h), 0)
    ImageDraw.Draw(m).ellipse([S(c[0] - r), S(c[1] - r), S(c[0] + r), S(c[1] + r)], fill=255)
    sh = Image.new("RGBA", (w, h), (0, 0, 0, 255))
    sh.putalpha(m.filter(ImageFilter.GaussianBlur(S(4))).point(lambda v: int(v * 0.62)))
    img.alpha_composite(sh)

# ----------------------------------------------------------------------------- panel
def build_panel(art_dir):
    img = brushed_metal(W * SS, H * SS)
    d = ImageDraw.Draw(img)
    # frame
    d.rectangle([0, 0, W * SS - 1, H * SS - 1], outline=(52, 52, 56, 255), width=S(2))
    d.rectangle([S(2), S(2), S(W - 3), S(H - 3)], outline=(8, 8, 10, 255), width=S(1))
    d.rounded_rectangle([S(10), S(10), S(W - 10), S(H - 10)], S(6), outline=(124, 96, 44, 110), width=S(1))
    for c in ((26, 26), (W - 26, 26), (26, H - 26), (W - 26, H - 26)):
        screw(d, c, 9)

    # --- top bar: tomb paintings + title
    tomb_plate(img, [S(56), S(22), S(356), S(106)], "13740.jpg", (0, 18, 900, 270), art_dir)
    tomb_plate(img, [S(W - 356), S(22), S(W - 56), S(106)], "13739.jpg", (0, 0, 900, 252), art_dir)
    d = ImageDraw.Draw(img)
    spaced_text(d, (S(W / 2), S(52)), "BLACKFACE", font("Inter-Bold.otf", S(50)), (234, 232, 224, 255), S(9))
    spaced_text(d, (S(W / 2), S(94)), "CLASS A FET LIMITING AMPLIFIER", font("Inter-Medium.otf", S(13)),
                (150, 148, 140, 255), S(3))
    f_lab = font("Inter-SemiBold.otf", S(15))
    f_tick = font("Inter-Medium.otf", S(11))
    f_btn = font("Inter-Bold.otf", S(15))
    f_small = font("Inter-SemiBold.otf", S(12))
    cream = (222, 214, 192, 255)

    # --- big knobs
    for name, c, marks in (
        ("INPUT", L["input_c"], [(0, "0", True), (.125, None, False), (.25, "10", True), (.375, None, False),
                                 (.5, "20", True), (.625, None, False), (.75, "30", True), (.875, None, False),
                                 (1.0, "40", True)]),
        ("OUTPUT", L["output_c"], [(0, "-24", True), (1 / 6, None, False), (1 / 3, "-12", True), (.5, None, False),
                                   (2 / 3, "0", True), (5 / 6, None, False), (1.0, "+12", True)]),
    ):
        knob_well(img, c, 104)
        d = ImageDraw.Draw(img)
        knob_ticks(d, c, L["big_ring"] + 9, L["big_ring"] + 15, L["big_ring"] + 19, L["big_ring"] + 33, marks, f_tick,
                   L["sweep_deg"])
        spaced_text(d, (S(c[0]), S(c[1] + 128)), name, font("Inter-Bold.otf", S(22)), cream, S(2))
    # --- small knobs
    for name, c, sw in (("ATTACK", L["attack_c"], "fast"), ("RELEASE", L["release_c"], "fast")):
        knob_well(img, c, 54)
        d = ImageDraw.Draw(img)
        marks = [(i / 6, str(i + 1), True) for i in range(7)]
        knob_ticks(d, c, L["small_ring"] + 6, L["small_ring"] + 11, L["small_ring"] + 13, L["small_ring"] + 25, marks,
                   f_tick, L["sweep_deg"])
        spaced_text(d, (S(c[0]), S(c[1] - 84)), name, f_lab, cream, S(2))

    # --- ratio buttons
    spaced_text(d, (S(L["ratio_x"] + L["ratio_w"] / 2), S(L["ratio_y"] - 20)), "RATIO", f_lab, cream, S(2))
    for i, lab in enumerate(["4:1", "8:1", "12:1", "20:1", "ALL"]):
        y = L["ratio_y"] + i * (L["ratio_h"] + L["ratio_gap"])
        button_face(img, [S(L["ratio_x"]), S(y), S(L["ratio_x"] + L["ratio_w"]), S(y + L["ratio_h"])], lab, f_btn, True)
    # --- meter buttons
    d = ImageDraw.Draw(img)
    spaced_text(d, (S(L["meter_x"] + L["meter_w"] / 2), S(L["meter_y"] - 20)), "METER", f_lab, cream, S(2))
    for i, lab in enumerate(["GR", "+4", "+8", "OFF"]):
        y = L["meter_y"] + i * (L["meter_h"] + L["meter_gap"])
        button_face(img, [S(L["meter_x"]), S(y), S(L["meter_x"] + L["meter_w"]), S(y + L["meter_h"])], lab, f_btn, True)

    # --- VU window bezel
    vx, vy, vw, vh = L["vu"]
    box = [S(vx - 9), S(vy - 9), S(vx + vw + 9), S(vy + vh + 9)]
    drop_shadow(img, box, S(8), blur=6, off=4, alpha=200)
    d = ImageDraw.Draw(img)
    d.rounded_rectangle(box, S(8), fill=(10, 10, 12, 255), outline=(86, 86, 92, 255), width=S(2))
    d.rounded_rectangle([box[0] + S(3), box[1] + S(3), box[2] - S(3), box[3] - S(3)], S(6), outline=(30, 30, 34, 255),
                        width=S(2))
    d.rectangle([S(vx - 1), S(vy - 1), S(vx + vw), S(vy + vh)], fill=(222, 208, 160, 255))  # under-face
    spaced_text(d, (S(vx + vw / 2), S(vy + vh + 36)), "BLACKFACE \u00b7 LIMITING AMPLIFIER", font("Inter-Bold.otf", S(16)),
                (226, 222, 208, 255), S(1.2))
    spaced_text(d, (S(vx + vw / 2), S(vy + vh + 62)), "FET SERIES \u00b7 CLASS A", font("Inter-Medium.otf", S(13)),
                (146, 144, 136, 255), S(2.5))

    # --- bottom strip
    d.line([S(28), S(432), S(W - 28), S(432)], fill=(8, 8, 10, 255), width=S(2))
    d.line([S(28), S(434), S(W - 28), S(434)], fill=(58, 58, 62, 255), width=S(1))
    mini = [("MIX", L["mix_c"], [(0, "0", True), (.5, None, False), (1, "100", True)]),
            ("SC HPF", L["hpf_c"], [(0, "OFF", True), (.5, None, False), (1, "300", True)]),
            ("COLOR", L["color_c"], [(0, "0", True), (.5, None, False), (1, "MAX", True)])]
    for name, c, marks in mini:
        knob_well(img, c, 34)
        d = ImageDraw.Draw(img)
        knob_ticks(d, c, L["mini_ring"] + 5, L["mini_ring"] + 9, L["mini_ring"] + 11, L["mini_ring"] + 19, marks,
                   font("Inter-Medium.otf", S(9)), L["sweep_deg"])
        spaced_text(d, (S(c[0]), S(c[1] - 52)), name, f_small, cream, S(1.5))
    # detector
    dx = L["det_x"]
    spaced_text(d, (S(dx + (3 * L["det_w"] + 2 * L["det_gap"]) / 2), S(L["det_y"] - 18)), "DETECTOR", f_small, cream, S(1.5))
    for i, lab in enumerate(["INT", "LINK", "EXT"]):
        x = dx + i * (L["det_w"] + L["det_gap"])
        button_face(img, [S(x), S(L["det_y"]), S(x + L["det_w"]), S(L["det_y"] + L["det_h"])], lab, f_small)
    d = ImageDraw.Draw(img)
    ox = L["os_x"]
    spaced_text(d, (S(ox + (4 * L["os_w"] + 3 * L["os_gap"]) / 2), S(L["os_y"] - 18)), "OVERSAMPLE", f_small, cream, S(1.5))
    for i, lab in enumerate(["1x", "2x", "4x", "8x"]):
        x = ox + i * (L["os_w"] + L["os_gap"])
        button_face(img, [S(x), S(L["os_y"]), S(x + L["os_w"]), S(L["os_y"] + L["os_h"])], lab, f_small)
    d = ImageDraw.Draw(img)
    # preset
    p, n, nx = L["prev"], L["name"], L["next"]
    spaced_text(d, (S(n[0] + n[2] / 2), S(n[1] - 18)), "PRESET", f_small, cream, S(1.5))
    for b in (p, nx):
        button_face(img, [S(b[0]), S(b[1]), S(b[0] + b[2]), S(b[1] + b[3])], "", f_small)
    d = ImageDraw.Draw(img)
    nb = [S(n[0]), S(n[1]), S(n[0] + n[2]), S(n[1] + n[3])]
    d.rounded_rectangle(nb, S(5), fill=(8, 8, 10, 255), outline=(70, 70, 76, 255), width=S(1))
    inset_shadow(img, nb, S(5), depth=5, alpha=200)
    # power
    pc, pr = L["power_c"], L["power_r"]
    d = ImageDraw.Draw(img)
    drop_shadow(img, [S(pc[0] - pr), S(pc[1] - pr), S(pc[0] + pr), S(pc[1] + pr)], S(pr), blur=4, off=2, alpha=200)
    d = ImageDraw.Draw(img)
    d.ellipse([S(pc[0] - pr), S(pc[1] - pr), S(pc[0] + pr), S(pc[1] + pr)], fill=(26, 26, 29, 255),
              outline=(84, 84, 90, 255), width=S(2))
    d.ellipse([S(pc[0] - pr + 6), S(pc[1] - pr + 6), S(pc[0] + pr - 6), S(pc[1] + pr - 6)], outline=(10, 10, 12, 255),
              width=S(2))
    spaced_text(d, (S(pc[0]), S(pc[1] + pr + 18)), "POWER", f_small, cream, S(1.5))
    return img

# ----------------------------------------------------------------------------- knob caps
def knob_cap(diam, small=False):
    n = int(diam * SS)
    yy, xx = np.mgrid[0:n, 0:n].astype(np.float32)
    cx = cy = (n - 1) / 2
    dx, dy = (xx - cx) / (n / 2), (yy - cy) / (n / 2)
    r = np.sqrt(dx * dx + dy * dy)
    th = np.arctan2(dx, -dy)  # 0 at 12 o'clock
    rng = np.random.default_rng(11 if small else 5)
    # conic anisotropic highlight (light from top-left, second weaker from bottom-right)
    hi = (np.abs(np.cos(th - 0.9)) ** 6) * 0.55 + (np.abs(np.cos(th - 0.9 - math.pi / 2)) ** 10) * 0.18
    skirt_lo, rim = 0.80, 0.985
    # knurled skirt
    flutes = 0.5 + 0.5 * np.cos(th * (36 if not small else 28))
    skirt = 0.10 + 0.10 * flutes + 0.34 * hi * (0.55 + 0.45 * flutes)
    # top cap: fine concentric brushing
    rings = 0.5 + 0.5 * np.sin(r * (diam * 1.1))
    grain = rng.normal(0, 1, (n, n)).astype(np.float32)
    top = 0.17 + 0.04 * rings + 0.46 * hi * (1.0 - 0.45 * r) + 0.012 * grain
    # bevel between cap and skirt
    bevel = np.exp(-((r - skirt_lo) / 0.018) ** 2)
    val = np.where(r < skirt_lo, top, skirt)
    val = val - 0.14 * bevel + 0.20 * np.exp(-((r - (skirt_lo - 0.03)) / 0.012) ** 2) * (hi + 0.2)
    # rim light
    val = val + 0.22 * np.exp(-((r - rim) / 0.012) ** 2) * (hi + 0.25)
    val = val.clip(0, 1)
    v8 = (val * 255).astype(np.uint8)
    rgb = np.stack([v8, v8, np.minimum(v8.astype(np.int32) + 3, 255).astype(np.uint8)], -1)
    alpha = (np.clip((1.0 - r) * (n / 2) / 1.5, 0, 1) * 255).astype(np.uint8)
    img = Image.fromarray(np.dstack([rgb, alpha]), "RGBA")
    return img

# ----------------------------------------------------------------------------- VU face
def vu_face():
    vx, vy, vw, vh = L["vu"]
    img = rgba(vw * SS, vh * SS)
    arr = np.zeros((vh * SS, vw * SS, 4), np.float32)
    yy, xx = np.mgrid[0:vh * SS, 0:vw * SS].astype(np.float32)
    px, py = L["vu_pivot"]
    rr = np.sqrt(((xx / SS - px)) ** 2 + ((yy / SS - py)) ** 2)
    glow = 1.0 - 0.30 * np.clip(rr / 190.0, 0, 1) ** 1.6                      # lamp falloff from the pivot
    top = np.linspace(1.0, 0.94, vh * SS)[:, None]
    base = np.array([240, 228, 186], np.float32)
    for k in range(3):
        arr[..., k] = base[k] * glow * top
    arr[..., 3] = 255
    img = Image.fromarray(arr.clip(0, 255).astype(np.uint8), "RGBA")
    d = ImageDraw.Draw(img)
    ink = (28, 24, 20, 255)
    red = (190, 36, 28, 255)
    R = L["vu_arc_r"]
    cP = (S(px), S(py))
    # arc line
    def arc(r0, r1, a0, a1, fill, steps=240):
        pts_out = [polar(cP, S(r1), a0 + (a1 - a0) * i / steps) for i in range(steps + 1)]
        pts_in = [polar(cP, S(r0), a0 + (a1 - a0) * i / steps) for i in range(steps, -1, -1)]
        d.polygon(pts_out + pts_in, fill=fill)
    a_min, a_max = vu_angle(-20), vu_angle(3)
    a_zero = vu_angle(0)
    arc(R - 0.9, R + 0.9, a_min, a_zero, ink)
    arc(R - 3.0, R + 3.0, a_zero, a_max, red)
    f_num = font("Inter-SemiBold.otf", S(12))
    for v in VU_MARKS_MINOR:
        a = vu_angle(v)
        d.line([polar(cP, S(R), a), polar(cP, S(R + 6), a)], fill=ink, width=S(1.1))
    for v in VU_MARKS_MAJOR:
        a = vu_angle(v)
        col = red if v > 0 else ink
        d.line([polar(cP, S(R), a), polar(cP, S(R + 11), a)], fill=col, width=S(1.8))
        txt = ("+%d" % v) if v > 0 else str(v)
        d.text(polar(cP, S(R + 23), a), txt, font=f_num, fill=col, anchor="mm")
    spaced_text(d, (S(px), S(py - 52)), "VU", font("Inter-Bold.otf", S(30)), ink, S(2))
    d.text((S(px), S(py - 28)), "dB", font=font("Inter-Medium.otf", S(10)), fill=(70, 62, 50, 255), anchor="mm")
    d.text((S(12), S(vh - 14)), "GR  \u2190 more reduction", font=font("Inter-Medium.otf", S(9)), fill=(90, 80, 64, 255), anchor="lm")
    d.text((S(vw - 12), S(vh - 14)), "+4 / +8 = output", font=font("Inter-Medium.otf", S(9)), fill=(90, 80, 64, 255), anchor="rm")
    # window shading (top + sides), baked into the face
    sh = Image.new("RGBA", img.size, (0, 0, 0, 0))
    sd = ImageDraw.Draw(sh)
    for i in range(14):
        a = int(110 * (1 - i / 14) ** 2)
        sd.rectangle([0, S(i), img.width, S(i + 1)], fill=(0, 0, 0, a))
        sd.rectangle([S(i), 0, S(i + 1), img.height], fill=(0, 0, 0, a // 2))
        sd.rectangle([img.width - S(i + 1), 0, img.width - S(i), img.height], fill=(0, 0, 0, a // 2))
    img.alpha_composite(sh)
    return img

def vu_glass():
    vx, vy, vw, vh = L["vu"]
    w, h = vw * SS, vh * SS
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    # diagonal soft highlight across the upper-left
    t = (xx / w * 0.9 + yy / h * 1.0)
    band = np.exp(-((t - 0.55) / 0.22) ** 2) * 0.10
    top = np.exp(-(yy / (h * 0.28)) ** 2) * 0.10
    a = ((band + top) * 255).clip(0, 255).astype(np.uint8)
    rgb = np.full((h, w, 3), 255, np.uint8)
    return Image.fromarray(np.dstack([rgb, a]), "RGBA")

# ----------------------------------------------------------------------------- header
def write_header():
    def rect(name, x, y, w, h):
        return f"inline constexpr Rect {name}{{{x}, {y}, {w}, {h}}};\n"
    o = []
    o.append("// GENERATED by tools/make_assets.py - do not edit by hand.\n#pragma once\n\nnamespace bf::layout {\n\n")
    o.append("struct Rect { double x, y, w, h; };\nstruct Pt { double x, y; };\n\n")
    o.append(f"inline constexpr double kWidth = {W}, kHeight = {H};\n")
    o.append(f"inline constexpr double kSweepDeg = {L['sweep_deg']};\n\n")
    def knob(name, c, cap, ring, view_half, view_down):
        o.append(f"inline constexpr Pt {name}Center{{{c[0]}, {c[1]}}};\n")
        o.append(f"inline constexpr double {name}Cap = {cap}, {name}Ring = {ring}, {name}ViewHalf = {view_half}, {name}ViewDown = {view_down};\n")
    knob("kInput", L["input_c"], L["big_cap"], L["big_ring"], 100, 100)
    knob("kOutput", L["output_c"], L["big_cap"], L["big_ring"], 100, 100)
    knob("kAttack", L["attack_c"], L["small_cap"], L["small_ring"], 64, 62)
    knob("kRelease", L["release_c"], L["small_cap"], L["small_ring"], 64, 62)
    knob("kMix", L["mix_c"], L["mini_cap"], L["mini_ring"], 36, 48)
    knob("kHpf", L["hpf_c"], L["mini_cap"], L["mini_ring"], 36, 48)
    knob("kColor", L["color_c"], L["mini_cap"], L["mini_ring"], 36, 48)
    o.append("\n")
    def btns(name, n, x0, y0, w, h, gap, vertical):
        o.append(f"inline constexpr int {name}Count = {n};\n")
        o.append(f"inline constexpr Rect {name}First{{{x0}, {y0}, {w}, {h}}};\n")
        o.append(f"inline constexpr double {name}Step = {(h + gap) if vertical else (w + gap)};\n")
        o.append(f"inline constexpr bool {name}Vertical = {'true' if vertical else 'false'};\n")
    btns("kRatioBtn", 5, L["ratio_x"], L["ratio_y"], L["ratio_w"], L["ratio_h"], L["ratio_gap"], True)
    btns("kMeterBtn", 4, L["meter_x"], L["meter_y"], L["meter_w"], L["meter_h"], L["meter_gap"], True)
    btns("kDetBtn", 3, L["det_x"], L["det_y"], L["det_w"], L["det_h"], L["det_gap"], False)
    btns("kOsBtn", 4, L["os_x"], L["os_y"], L["os_w"], L["os_h"], L["os_gap"], False)
    o.append("\n")
    o.append(rect("kPrevBtn", *L["prev"]))
    o.append(rect("kNameBox", *L["name"]))
    o.append(rect("kNextBtn", *L["next"]))
    o.append(f"inline constexpr Pt kPowerCenter{{{L['power_c'][0]}, {L['power_c'][1]}}};\n")
    o.append(f"inline constexpr double kPowerRadius = {L['power_r']};\n\n")
    o.append(rect("kVuWindow", *L["vu"]))
    o.append(f"inline constexpr Pt kVuPivot{{{L['vu_pivot'][0]}, {L['vu_pivot'][1]}}};\n")
    o.append(f"inline constexpr double kVuSweepDeg = {L['vu_sweep']}, kVuNeedle = {L['vu_needle']};\n")
    o.append("}  // namespace bf::layout\n")
    open(os.path.join(SRC, "BlackfaceLayout.h"), "w").write("".join(o))

# ----------------------------------------------------------------------------- main
if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--art", default="/mnt/user-data/uploads")
    a = ap.parse_args()
    print("panel ..."); save_pair(build_panel(a.art), "panel")
    print("knob caps ...")
    save_pair(knob_cap(L["big_cap"]), "knob_big")
    save_pair(knob_cap(L["small_cap"], True), "knob_small")
    save_pair(knob_cap(L["mini_cap"], True), "knob_mini")
    print("vu ..."); save_pair(vu_face(), "vu_face"); save_pair(vu_glass(), "vu_glass")
    write_header()
    print("done")
