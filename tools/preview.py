#!/usr/bin/env python3
"""Approximate render of the finished editor (static assets + what the views draw live)."""
import math, os, sys
from PIL import Image, ImageDraw, ImageFont
sys.path.insert(0, os.path.dirname(__file__))
import make_assets as M
L = M.L
RES = M.RES
img = Image.open(os.path.join(RES, "panel@2x.png")).convert("RGBA")
K = 2
layer = Image.new('RGBA', img.size, (0, 0, 0, 0))
d = ImageDraw.Draw(layer)
def flush():
    global layer, d
    img.alpha_composite(layer)
    layer = Image.new('RGBA', img.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
def P(v): return v * K
def knob(c, cap, ring, name, norm, text, big):
    flush()
    cap_img = Image.open(os.path.join(RES, name + "@2x.png")).convert("RGBA")
    img.alpha_composite(cap_img, (int(P(c[0] - cap / 2)), int(P(c[1] - cap / 2))))
    sw = L["sweep_deg"]
    def arc(n0, n1, col, w):
        steps = 80
        pts = [M.polar((P(c[0]), P(c[1])), P(ring), -sw + 2 * sw * (n0 + (n1 - n0) * i / steps)) for i in range(steps + 1)]
        d.line(pts, fill=col, width=int(P(w)), joint="curve")
    arc(0, 1, (70, 70, 74, 255), 4.5)
    arc(0, norm, (245, 160, 20, 255), 4.5)
    a = -sw + 2 * sw * norm
    p0 = M.polar((P(c[0]), P(c[1])), P(cap * 0.22), a); p1 = M.polar((P(c[0]), P(c[1])), P(cap * 0.44), a)
    d.line([p0, p1], fill=(250, 250, 250, 255), width=int(P(3.2 if big else 2.4)))
    f = ImageFont.truetype(M.FONT_DIR + "Inter-SemiBold.otf", int(P(11)))
    d.text((P(c[0]), P(c[1] + ring + 22 if cap > 50 else c[1] + ring + 14)), text, font=f, fill=(255, 190, 80, 255), anchor="mm")
knob(L["input_c"], L["big_cap"], L["big_ring"], "knob_big", .55, "+22.0 dB", True)
knob(L["output_c"], L["big_cap"], L["big_ring"], "knob_big", .5, "-6.0 dB", True)
knob(L["attack_c"], L["small_cap"], L["small_ring"], "knob_small", .5, "126 us", False)
knob(L["release_c"], L["small_cap"], L["small_ring"], "knob_small", .3, "235 ms", False)
for c, n, t in ((L["mix_c"], 1.0, "100 %"), (L["hpf_c"], 0.3, "90 Hz"), (L["color_c"], .25, "25 %")):
    knob(c, L["mini_cap"], L["mini_ring"], "knob_mini", n, t, False)
# VU
vx, vy, vw, vh = L["vu"]
flush()
face = Image.open(os.path.join(RES, "vu_face@2x.png")).convert("RGBA")
img.alpha_composite(face, (P(vx), P(vy)))
px, py = L["vu_pivot"]
a = M.vu_angle(-6)  # needle at -6 VU
tip = M.polar((P(vx + px), P(vy + py)), P(L["vu_needle"]), a)
d.line([(P(vx + px) + 3, P(vy + py) + 3), (tip[0] + 3, tip[1] + 3)], fill=(0, 0, 0, 70), width=int(P(2)))
d.line([(P(vx + px), P(vy + py)), tip], fill=(20, 16, 14, 255), width=int(P(1.8)))
d.ellipse([P(vx + px - 8), P(vy + py - 8), P(vx + px + 8), P(vy + py + 8)], fill=(20, 18, 16, 255))
flush()
glass = Image.open(os.path.join(RES, "vu_glass@2x.png")).convert("RGBA")
img.alpha_composite(glass, (P(vx), P(vy)))
flush()
# lit buttons: ratio 4:1, meter GR, det INT, os 4x, power
def lit(x, y, w, h, led):
    d.rounded_rectangle([P(x), P(y), P(x + w), P(y + h)], P(5), fill=(255, 170, 40, 46), outline=(255, 170, 40, 160), width=2)
    if led:
        cx, cy, r = x + w / 2, y + 10, 3.6
        d.ellipse([P(cx - 3 * r), P(cy - 3 * r), P(cx + 3 * r), P(cy + 3 * r)], fill=(255, 150, 30, 40))
        d.ellipse([P(cx - r), P(cy - r), P(cx + r), P(cy + r)], fill=(255, 190, 70, 255))
lit(L["ratio_x"], L["ratio_y"], L["ratio_w"], L["ratio_h"], True)
lit(L["meter_x"], L["meter_y"], L["meter_w"], L["meter_h"], True)
lit(L["det_x"], L["det_y"], L["det_w"], L["det_h"], False)
lit(L["os_x"] + 2 * (L["os_w"] + L["os_gap"]), L["os_y"], L["os_w"], L["os_h"], False)
pc = L["power_c"]; r = 9
d.ellipse([P(pc[0] - 3 * r), P(pc[1] - 3 * r), P(pc[0] + 3 * r), P(pc[1] + 3 * r)], fill=(255, 150, 30, 50))
d.ellipse([P(pc[0] - r), P(pc[1] - r), P(pc[0] + r), P(pc[1] + r)], fill=(255, 190, 70, 255))
n = L["name"]
f = ImageFont.truetype(M.FONT_DIR + "Inter-SemiBold.otf", int(P(14)))
d.text((P(n[0] + n[2] / 2), P(n[1] + n[3] / 2)), "Lead Vocal", font=f, fill=(255, 190, 80, 255), anchor="mm")
for bx, dirn in ((L["prev"], -1), (L["next"], 1)):
    cx, cy = bx[0] + bx[2] / 2, bx[1] + bx[3] / 2
    d.polygon([(P(cx - dirn * 4), P(cy - 6)), (P(cx - dirn * 4), P(cy + 6)), (P(cx + dirn * 5), P(cy))], fill=(226, 218, 196, 255))
flush()
out = sys.argv[1] if len(sys.argv) > 1 else "/tmp/preview.png"
img.convert("RGB").save(out)
img.convert("RGB").resize((img.width // 2, img.height // 2), Image.LANCZOS).save(out.replace(".png", "_1x.png"))
print(out)
