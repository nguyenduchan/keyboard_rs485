"""Lỗ phay CNC mặt máy (cánh tủ) cho từng biến thể lắp ráp, mọi kích thước nguyên mm.

Biến thể = cùng một PCB, khác số linh kiện mặt trước được lắp:
  7-seg  : 0, 1 (DS1, digit 1–4, hàng trên) hoặc 2 module
  LED    : 0…8, lắp từ D10 (trên cùng) xuống
  Phím   : 0…4 hàng tact × 4 cột, lắp từ hàng sát 7-seg (SW13–16) xuống; hoặc "M" = bàn phím màng (J_KEY)

Bản vẽ ở HƯỚNG NHÌN TỪ MẶT TỦ: phím / LED / 7-seg ở mặt dưới PCB nên X bị lật gương so với KiCad
(x_mặt_tủ = 120 − x_KiCad). Y giữ nguyên (Y nhỏ = trên). DXF dùng Y hướng lên (y_dxf = 120 − y).

  python tools/cnc_panel.py --check            # kiểm tra mọi tổ hợp (3 × 9 × 6 = 162 biến thể)
  python tools/cnc_panel.py D2-L8-K16 D1-L4-M  # xuất SVG + DXF + CSV vào hardware/mechanical/cnc/
  python tools/cnc_panel.py --standard         # xuất các biến thể chuẩn
"""

from __future__ import annotations

import math
import re
import sys
from dataclasses import dataclass
from itertools import product
from pathlib import Path

from front_layout_spec import digit_centers, status_centers, switch_centers
from zone_layout import BOTTOM_POS, MOUNT_POS

OUT = Path(__file__).resolve().parents[4] / "hardware" / "mechanical" / "cnc"
BOARD = 120.0

# Kích thước linh kiện nhô qua mặt máy (mm)
DIGIT_BODY = (50.4, 19.05)   # 5641AS
KEY_CAP = 11.8               # chụp trong vuông 11.8×11.8×3.9 (theo bản vẽ nhà bán) trùm mũ A14 10×10
KEY_HOLE = 13                # lỗ vuông; tâm nguyên mm, cạnh nguyên mm (mép nằm trên .5)
LED_D = 3.0                  # LED 3 mm (thân)
# Khe đuôi bàn phím màng trên PCB, so với gốc footprint J_KEY (Membrane_1x10_P2.54_Vertical_Slot)
JKEY_SLOT = (-4.4, -2.57, -1.8, 25.43)

CLR_MIN = 0.25    # khe hở tối thiểu mỗi phía giữa lỗ phay và linh kiện
CLR_MAX = 1.0     # quá mức này thì không còn "vừa khít"
RIB_MIN = 3.0     # gân tôn tối thiểu giữa hai lỗ phay
EDGE_MIN = 3.0    # lỗ phay cách mép vùng bo 120 × 120
MOUNT_D = 3       # lỗ bắt vít M3 trên cánh tủ
STANDOFF_R = 3.2  # bán kính trụ đệm M3 lục giác 5,5 mm (góc)

LED_LABELS = ["RUN", "STOP", "FAULT", "AUTO", "MAN", "RDY", "COMM", "ALM"]


@dataclass
class Cut:
    kind: str           # "rect" | "circle"
    name: str
    x0: float           # rect: mép; circle: tâm (x0, y0), đường kính d
    y0: float
    x1: float = 0.0
    y1: float = 0.0
    d: float = 0.0
    part: tuple = ()    # thân linh kiện (x0, y0, x1, y1) hoặc (cx, cy, d) cần lọt qua

    def box(self) -> tuple[float, float, float, float]:
        if self.kind == "rect":
            return (self.x0, self.y0, self.x1, self.y1)
        r = self.d / 2
        return (self.x0 - r, self.y0 - r, self.x0 + r, self.y0 + r)


@dataclass(frozen=True)
class Variant:
    digits: int     # 0..2 module
    leds: int       # 0..8
    keys: str       # "0", "4", "8", "12", "16" (tact) hoặc "M" (màng)

    @property
    def name(self) -> str:
        return f"D{self.digits}-L{self.leds}-{'M' if self.keys == 'M' else 'K' + self.keys}"

    @staticmethod
    def parse(text: str) -> "Variant":
        m = re.fullmatch(r"D([0-2])-L([0-8])-(M|K(?:0|4|8|12|16))", text.upper())
        if not m:
            raise ValueError(f"tên biến thể sai: {text} (vd D2-L8-K16, D1-L4-M)")
        return Variant(int(m[1]), int(m[2]), "M" if m[3] == "M" else m[3][1:])


def all_variants() -> list[Variant]:
    return [Variant(d, n, k) for d, n, k in product(range(3), range(9), ("0", "4", "8", "12", "16", "M"))]


STANDARD = ["D2-L8-K16", "D2-L8-M", "D2-L8-K0", "D1-L4-K16", "D1-L4-M", "D2-L0-K16", "D0-L8-K16", "D0-L0-M"]


def _round_out(box: tuple[float, float, float, float]) -> tuple[float, float, float, float]:
    """Nới hộp thêm CLR_MIN rồi làm tròn ra ngoài tới nguyên mm."""
    x0, y0, x1, y1 = box
    return (math.floor(x0 - CLR_MIN), math.floor(y0 - CLR_MIN), math.ceil(x1 + CLR_MIN), math.ceil(y1 + CLR_MIN))


def _hole_d(part_d: float) -> int:
    return math.ceil(part_d + 2 * CLR_MIN)


def cuts(v: Variant) -> list[Cut]:
    out: list[Cut] = []
    dc = digit_centers()
    mods = [dc["DS1"], dc["DS2"]][: v.digits]
    if mods:
        w, h = DIGIT_BODY
        body = (min(x for x, _ in mods) - w / 2, min(y for _, y in mods) - h / 2,
                max(x for x, _ in mods) + w / 2, max(y for _, y in mods) + h / 2)
        # hai hàng cách thân 0,95 mm → chung một cửa sổ
        out.append(Cut("rect", f"7SEG x{v.digits}", *_round_out(body), part=body))
    for ref, (x, y) in list(status_centers().items())[: v.leds]:
        out.append(Cut("circle", ref, x, y, d=_hole_d(LED_D), part=(x, y, LED_D)))
    if v.keys == "M":
        jx, jy, _ = BOTTOM_POS["J_KEY"]
        slot = (jx + JKEY_SLOT[0], jy + JKEY_SLOT[1], jx + JKEY_SLOT[2], jy + JKEY_SLOT[3])
        out.append(Cut("rect", "TAIL J_KEY", *_round_out(slot), part=slot))
    else:
        n = int(v.keys)
        sw = switch_centers()
        order = sorted(sw, key=lambda r: (sw[r][1], sw[r][0]))  # hàng trên (sát 7-seg) trước
        for ref in order[:n]:
            x, y = sw[ref]
            h, c = KEY_HOLE / 2, KEY_CAP / 2
            out.append(Cut("rect", ref, x - h, y - h, x + h, y + h, part=(x - c, y - c, x + c, y + c)))
    for name, (x, y) in MOUNT_POS.items():
        out.append(Cut("circle", name, x, y, d=MOUNT_D))
    return out


def _is_int(v: float) -> bool:
    return abs(v - round(v)) < 1e-6


def _gap(a: Cut, b: Cut) -> float:
    """Khoảng cách mép–mép giữa hai lỗ (âm = chồng nhau)."""
    if a.kind == b.kind == "circle":
        return math.hypot(a.x0 - b.x0, a.y0 - b.y0) - (a.d + b.d) / 2
    if a.kind == "circle":
        a, b = b, a
    if b.kind == "circle":  # rect a, circle b
        cx = min(max(b.x0, a.x0), a.x1)
        cy = min(max(b.y0, a.y0), a.y1)
        return math.hypot(b.x0 - cx, b.y0 - cy) - b.d / 2
    dx = max(b.x0 - a.x1, a.x0 - b.x1, 0)
    dy = max(b.y0 - a.y1, a.y0 - b.y1, 0)
    if dx == 0 and dy == 0:
        return -1.0
    return math.hypot(dx, dy)


def check(v: Variant) -> list[str]:
    errs: list[str] = []
    cs = cuts(v)
    for c in cs:
        if c.kind == "rect" and c.name.startswith("SW"):
            nums = ((c.x0 + c.x1) / 2, (c.y0 + c.y1) / 2, c.x1 - c.x0, c.y1 - c.y0)  # tâm + cạnh nguyên
        else:
            nums = (c.x0, c.y0, c.d) if c.kind == "circle" else (c.x0, c.y0, c.x1, c.y1)
        if not all(_is_int(n) for n in nums):
            errs.append(f"{c.name}: kích thước/tọa độ không nguyên mm {nums}")
        if c.part:
            if c.kind == "circle":
                px, py, pd = c.part
                off = math.hypot(px - c.x0, py - c.y0)
                lo = hi = (c.d - pd) / 2 - off
            else:
                gaps = (c.part[0] - c.x0, c.part[1] - c.y0, c.x1 - c.part[2], c.y1 - c.part[3])
                lo, hi = min(gaps), max(gaps)
            if lo < CLR_MIN - 1e-6:
                errs.append(f"{c.name}: khe hở {lo:.2f} < {CLR_MIN} mm — linh kiện kẹt")
            if hi > CLR_MAX + 1e-6:
                errs.append(f"{c.name}: khe hở {hi:.2f} > {CLR_MAX} mm — không vừa khít")
        x0, y0, x1, y1 = c.box()
        if min(x0, y0) < EDGE_MIN - 1e-6 or max(x1, y1) > BOARD - EDGE_MIN + 1e-6:
            errs.append(f"{c.name}: sát mép vùng bo < {EDGE_MIN} mm")
    for i, a in enumerate(cs):
        for b in cs[i + 1:]:
            mount = a.name in MOUNT_POS or b.name in MOUNT_POS
            if a.name in MOUNT_POS and b.name in MOUNT_POS:
                continue
            need = (STANDOFF_R - MOUNT_D / 2 + RIB_MIN) if mount else RIB_MIN
            g = _gap(a, b)
            if g < need - 1e-6:
                errs.append(f"{a.name} ↔ {b.name}: gân {g:.2f} < {need:.1f} mm")
    return errs


# ---------- xuất bản vẽ (hướng nhìn từ mặt tủ) ----------

def _fx(x: float) -> float:
    return BOARD - x


def _fmt(v: float) -> str:
    return f"{v:g}"


def _table(v: Variant) -> list[tuple[str, str, str]]:
    rows = []
    for c in cuts(v):
        if c.kind == "rect":
            x0, x1 = sorted((_fx(c.x0), _fx(c.x1)))
            rows.append((c.name, f"chữ nhật {_fmt(x1 - x0)} × {_fmt(c.y1 - c.y0)}",
                         f"X {_fmt(x0)}…{_fmt(x1)}, Y {_fmt(c.y0)}…{_fmt(c.y1)}"))
        else:
            rows.append((c.name, f"tròn Ø{_fmt(c.d)}", f"tâm X {_fmt(_fx(c.x0))}, Y {_fmt(c.y0)}"))
    return rows


def svg(v: Variant) -> str:
    L = [f'<svg xmlns="http://www.w3.org/2000/svg" width="170mm" height="170mm" viewBox="-10 -10 170 170">',
         f"<title>CNC mặt máy {v.name} — nhìn từ mặt tủ, mm</title>",
         '<rect x="0" y="0" width="120" height="120" fill="none" stroke="#999" stroke-width="0.3" stroke-dasharray="2,1.5"/>',
         '<text x="0" y="-3" font-size="3" font-family="sans-serif">'
         f"{v.name} — NHÌN TỪ MẶT TỦ (gốc góc trên-trái vùng bo 120×120), mọi số đo nguyên mm</text>"]
    for c in cuts(v):
        col = "#06c" if c.name in MOUNT_POS else "#c00"
        if c.kind == "rect":
            x0 = min(_fx(c.x0), _fx(c.x1))
            L.append(f'<rect x="{_fmt(x0)}" y="{_fmt(c.y0)}" width="{_fmt(c.x1 - c.x0)}" height="{_fmt(c.y1 - c.y0)}" '
                     f'fill="none" stroke="{col}" stroke-width="0.35"/>')
            if not c.name.startswith("SW"):
              L.append(f'<text x="{_fmt(x0 + 1)}" y="{_fmt(c.y0 + 3.5)}" font-size="2.6" font-family="sans-serif" fill="{col}">'
                     f"{c.name} {_fmt(c.x1 - c.x0)}×{_fmt(c.y1 - c.y0)}</text>")
        else:
            L.append(f'<circle cx="{_fmt(_fx(c.x0))}" cy="{_fmt(c.y0)}" r="{_fmt(c.d / 2)}" fill="none" stroke="{col}" stroke-width="0.35"/>')
    y = 126
    for name, shape, pos in _table(v):
        L.append(f'<text x="0" y="{y}" font-size="2.2" font-family="monospace">{name:<10} {shape:<22} {pos}</text>')
        y += 2.6
        if y > 158:
            L.append('<text x="0" y="160" font-size="2.2" font-family="monospace">… xem file .csv</text>')
            break
    L.append("</svg>")
    return "\n".join(L) + "\n"


def dxf(v: Variant) -> str:
    """DXF R12, đơn vị mm, nhìn từ mặt tủ, Y hướng lên, gốc góc dưới-trái vùng bo."""
    E: list[str] = []

    def line(x0, y0, x1, y1, layer):
        E.extend(["0", "LINE", "8", layer, "10", _fmt(x0), "20", _fmt(y0), "30", "0",
                  "11", _fmt(x1), "21", _fmt(y1), "31", "0"])

    for c in cuts(v):
        layer = "MOUNT" if c.name in MOUNT_POS else "CUT"
        if c.kind == "rect":
            x0, x1 = sorted((_fx(c.x0), _fx(c.x1)))
            y0, y1 = BOARD - c.y1, BOARD - c.y0
            for a, b in (((x0, y0), (x1, y0)), ((x1, y0), (x1, y1)), ((x1, y1), (x0, y1)), ((x0, y1), (x0, y0))):
                line(*a, *b, layer)
        else:
            E.extend(["0", "CIRCLE", "8", layer, "10", _fmt(_fx(c.x0)), "20", _fmt(BOARD - c.y0), "30", "0",
                      "40", _fmt(c.d / 2)])
    for a, b in (((0, 0), (120, 0)), ((120, 0), (120, 120)), ((120, 120), (0, 120)), ((0, 120), (0, 0))):
        line(*a, *b, "BOARD_REF")
    return "\n".join(["0", "SECTION", "2", "HEADER", "9", "$INSUNITS", "70", "4", "0", "ENDSEC",
                      "0", "SECTION", "2", "ENTITIES", *E, "0", "ENDSEC", "0", "EOF"]) + "\n"


def decal(v: Variant) -> str:
    """Decal 120 × 120 nhìn từ mặt tủ: cửa sổ, vòng phím, LED + nhãn."""
    L = ['<svg xmlns="http://www.w3.org/2000/svg" width="120mm" height="120mm" viewBox="0 0 120 120">',
         f"<title>Decal {v.name} — nhìn từ mặt tủ</title>",
         '<rect width="120" height="120" fill="#2a2a2a"/>']
    for c in cuts(v):
        if c.name in MOUNT_POS:
            continue
        if c.name.startswith("SW"):
            L.append(f'<rect x="{_fmt(min(_fx(c.x0), _fx(c.x1)))}" y="{_fmt(c.y0)}" width="{_fmt(c.x1 - c.x0)}" '
                     f'height="{_fmt(c.y1 - c.y0)}" fill="#1a1a1a" stroke="#888" stroke-width="0.15"/>')
        elif c.kind == "rect":
            fill = "#111" if c.name.startswith("7SEG") else "none"
            L.append(f'<rect x="{_fmt(min(_fx(c.x0), _fx(c.x1)))}" y="{_fmt(c.y0)}" width="{_fmt(c.x1 - c.x0)}" '
                     f'height="{_fmt(c.y1 - c.y0)}" fill="{fill}" stroke="#666" stroke-width="0.2"/>')
        else:
            i = int(c.name[1:]) - 10
            L.append(f'<circle cx="{_fmt(_fx(c.x0))}" cy="{_fmt(c.y0)}" r="{_fmt(c.d / 2)}" fill="#333" stroke="#888" stroke-width="0.15"/>')
            L.append(f'<text x="{_fmt(_fx(c.x0) - 4)}" y="{_fmt(c.y0 + 0.9)}" font-size="2.6" fill="#eee" '
                     f'font-family="sans-serif" text-anchor="end">{LED_LABELS[i]}</text>')
    if v.keys == "M":
        sw = switch_centers()
        xs = [_fx(x) for x, _ in sw.values()]
        ys = [y for _, y in sw.values()]
        L.append(f'<rect x="{_fmt(min(xs) - 7)}" y="{_fmt(min(ys) - 7)}" width="{_fmt(max(xs) - min(xs) + 14)}" '
                 f'height="{_fmt(max(ys) - min(ys) + 14)}" fill="none" stroke="#c90" stroke-width="0.25" stroke-dasharray="1,0.6"/>')
        L.append(f'<text x="{_fmt(min(xs) - 6)}" y="{_fmt(min(ys) - 8)}" font-size="2.4" fill="#c90" font-family="sans-serif">'
                 "vùng bàn phím màng (mặt tủ phẳng làm tấm đỡ)</text>")
    L.append("</svg>")
    return "\n".join(L) + "\n"


def export(v: Variant) -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / f"{v.name}.svg").write_text(svg(v), encoding="utf-8")
    (OUT / f"{v.name}.dxf").write_text(dxf(v), encoding="ascii")
    (OUT / f"{v.name}_decal.svg").write_text(decal(v), encoding="utf-8")
    rows = ["ten,hinh,vi_tri_nhin_tu_mat_tu_mm"] + [",".join(f'"{x}"' for x in r) for r in _table(v)]
    (OUT / f"{v.name}.csv").write_text("\n".join(rows) + "\n", encoding="utf-8-sig")
    print("wrote", OUT / v.name, "(.svg .dxf .csv _decal.svg)")


def main(argv: list[str]) -> int:
    sys.stdout.reconfigure(encoding="utf-8")
    if not argv or argv == ["--check"]:
        bad = 0
        for v in all_variants():
            errs = check(v)
            if errs:
                bad += 1
                print(v.name)
                for e in errs:
                    print("  -", e)
        n = len(all_variants())
        print(f"{n - bad}/{n} biến thể đạt (nguyên mm, khe hở {CLR_MIN}…{CLR_MAX}, gân ≥ {RIB_MIN}, cách mép ≥ {EDGE_MIN})")
        return 1 if bad else 0
    names = STANDARD if argv == ["--standard"] else argv
    rc = 0
    for name in names:
        v = Variant.parse(name)
        errs = check(v)
        for e in errs:
            print(f"{v.name}: {e}")
        rc |= bool(errs)
        export(v)
    return rc


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
