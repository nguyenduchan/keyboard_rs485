"""Sinh model 3D VRML cho phím B3F-4055 + mũ A14 + chụp trong, và module LED 7 đoạn 4 số 0,56" (5641AS).

Chạy: python tools/make_3d_models.py   -> hardware/kicad/panel-keyboard/3d/*.wrl

Quy ước VRML của KiCad: đơn vị 0,1 inch (2,54 mm), mặt phẳng bo là X-Y, Z hướng lên khỏi mặt linh kiện.
Mọi số đo bên dưới tính bằng mm rồi chia 2,54 khi ghi file.
"""

from __future__ import annotations

import math
from pathlib import Path

OUT = Path(__file__).resolve().parents[1] / "3d"
U = 2.54


class Wrl:
    def __init__(self) -> None:
        self.parts: list[str] = []

    def prism(self, pts: list[tuple[float, float]], z0: float, z1: float, color, transp: float = 0.0, spec=(0.2, 0.2, 0.2)):
        """Lăng trụ đứng: đa giác (x, y) lồi, đáy z0, đỉnh z1 (mm)."""
        n = len(pts)
        verts = [(x, y, z0) for x, y in pts] + [(x, y, z1) for x, y in pts]
        faces = [list(range(n - 1, -1, -1)), list(range(n, 2 * n))]
        for i in range(n):
            j = (i + 1) % n
            faces.append([i, j, n + j, n + i])
        coord = ", ".join(f"{x / U:.5f} {y / U:.5f} {z / U:.5f}" for x, y, z in verts)
        idx = ", ".join(" ".join(map(str, f)) + " -1" for f in faces)
        r, g, b = color
        sr, sg, sb = spec
        self.parts.append(
            "Shape {\n"
            f"  appearance Appearance {{ material Material {{ diffuseColor {r} {g} {b} specularColor {sr} {sg} {sb}"
            f" ambientIntensity 0.4 shininess 0.3 transparency {transp} }} }}\n"
            f"  geometry IndexedFaceSet {{ solid FALSE creaseAngle 0.5 coord Coordinate {{ point [ {coord} ] }}"
            f" coordIndex [ {idx} ] }}\n"
            "}\n"
        )

    def box(self, cx, cy, sx, sy, z0, z1, color, transp=0.0, spec=(0.2, 0.2, 0.2)):
        hx, hy = sx / 2, sy / 2
        self.prism([(cx - hx, cy - hy), (cx + hx, cy - hy), (cx + hx, cy + hy), (cx - hx, cy + hy)], z0, z1, color, transp, spec)

    def cyl(self, cx, cy, r, z0, z1, color, sides=16):
        pts = [(cx + r * math.cos(2 * math.pi * k / sides), cy + r * math.sin(2 * math.pi * k / sides)) for k in range(sides)]
        self.prism(pts, z0, z1, color)

    def save(self, name: str) -> None:
        OUT.mkdir(exist_ok=True)
        (OUT / name).write_text("#VRML V2.0 utf8\n" + "".join(self.parts), encoding="utf-8")
        print("wrote", OUT / name)


BLACK = (0.10, 0.10, 0.11)
STEEL = (0.70, 0.71, 0.74)
LEG = (0.82, 0.82, 0.78)


def tact() -> None:
    """B3F-4055 12x12x7.3 + mũ A14 10x10 + chụp trong 11.8x11.8x3.9.

    Chân 12,5 mm dọc theo Y (hàng X = ±2,5, Y = ±6,25); lỗ định vị Ø1,7 tại (±4,5; 0). Mặt đặt bo là z = 0.
    """
    w = Wrl()
    w.box(0, 0, 12, 12, 0.0, 3.3, BLACK)                        # thân nhựa
    w.box(0, 0, 12, 12, 3.3, 4.3, STEEL, spec=(0.6, 0.6, 0.6))  # nắp kim loại
    w.box(0, 0, 3.8, 3.8, 4.3, 7.3, BLACK)                      # trụ nhấn B3F-4055
    for sx in (-1, 1):
        for sy in (-1, 1):
            w.box(sx * 2.5, sy * 6.15, 0.8, 0.3, -3.5, 1.8, LEG, spec=(0.7, 0.7, 0.7))   # chân
            w.box(sx * 2.5, sy * 5.5, 0.8, 1.3, 1.5, 1.8, LEG, spec=(0.7, 0.7, 0.7))     # tai chân nối vào thân
    for sx in (-4.5, 4.5):
        w.cyl(sx, 0, 0.85, -1.5, 0.0, BLACK, 12)                # chốt định vị
    w.box(0, 0, 10, 10, 4.65, 8.05, (0.16, 0.38, 0.85), spec=(0.4, 0.4, 0.4))   # mũ A14 (xanh)
    w.box(0, 0, 11.8, 11.8, 5.15, 9.05, (0.88, 0.93, 1.0), transp=0.72, spec=(0.9, 0.9, 0.9))  # chụp trong
    w.save("SW_B3F4055_A14.wrl")


def seg_digit(w: Wrl, cx: float, cy: float) -> None:
    red = (1.0, 0.12, 0.10)
    z0, z1 = 8.05, 8.3
    t = 1.1
    for y in (6.0, 0.0, -6.0):                                   # a, g, d
        w.box(cx, cy + y, 4.6, t, z0, z1, red, spec=(0.8, 0.3, 0.3))
    for x in (-3.0, 3.0):                                        # f, b, e, c
        for y in (3.0, -3.0):
            w.box(cx + x, cy + y, t, 4.6, z0, z1, red, spec=(0.8, 0.3, 0.3))
    w.box(cx + 4.4, cy - 6.2, 1.1, 1.1, z0, z1, red, spec=(0.8, 0.3, 0.3))   # dấu chấm


def display() -> None:
    """5641AS: 4 số 0,56", thân 50,4 × 19,05, 12 chân 2,54 / hàng cách 15,24. Gốc ở tâm các chân; mặt sáng +z."""
    w = Wrl()
    w.box(0, 0, 50.4, 19.05, 0.4, 7.8, (0.04, 0.04, 0.045))      # thân đen
    w.box(0, 0, 50.4, 19.05, 7.8, 8.05, (0.08, 0.08, 0.09))      # mặt kính
    for k in range(4):
        cx = -19.05 + 12.7 * k
        w.box(cx, 0, 10.6, 15.6, 8.0, 8.06, (0.30, 0.03, 0.03))  # ô chữ số (đỏ sẫm)
        seg_digit(w, cx, 0.0)
    for i in range(6):
        for sy in (-7.62, 7.62):
            w.box(-6.35 + 2.54 * i, sy, 0.5, 0.5, -3.2, 0.4, LEG, spec=(0.7, 0.7, 0.7))
    w.save("DS_5641AS.wrl")


if __name__ == "__main__":
    tact()
    display()
