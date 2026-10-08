"""Bo 120 × 120 mm. Chip và SMD ở mặt trên (F.Cu). Phím và LED ở mặt dưới (B.Cu).

Hai hàng 7-seg sát nhau, khe 8 mm tới bàn phím để cắt CNC mặt máy.
Cột phải tách đôi: nguồn (giắc 24 V, bảo vệ, buck, LDO) ở trên; MCU + RS485 ở dưới.
Hở courtyard ≥ 1 mm. Không có dây đồng — chỉ đặt linh kiện.
Phần SMD xoay 180 sau khi lật từ mặt dưới lên mặt trên; giắc và linh kiện vốn xoay 90 giữ nguyên.
"""

from __future__ import annotations

import math

from front_layout_spec import digit_centers, status_centers, switch_centers

# Outline origin is the top-left corner. Plugs face the right edge.
ELEC_Y0 = 108.0
BOARD_X0 = 0.0
BOARD_W = 120.0
BOARD_H = 120.0
GAP_Y_MIN = 45.0
GAP_Y_MAX = 53.0

# Power block above, MCU block below, with a clear gap between the silk boxes.
ZONE_BUCK = {"x0": 82.0, "x1": 118.0, "y0": 4.0, "y1": 46.0, "label": "BUCK_5V"}
ZONE_PWR_24V = {"x0": 82.0, "x1": 118.0, "y0": 48.0, "y1": 76.0, "label": "24V_PROTECT"}
ZONE_MCU = {"x0": 72.0, "x1": 118.0, "y0": 78.0, "y1": 116.0, "label": "MCU_LOGIC"}

BOTTOM_UI_MAX_Y = BOARD_H

# Courtyard relative to footprint origin, rot 0: (xmin, ymin, xmax, ymax).
# Measured from the footprints on the board, plus the body of the terminals.
_B = {
    "0805": (-1.75, -1.05, 1.75, 1.05),
    "C4": (-7.0, -6.2, 7.0, 6.2),
    "ELEC63": (-5.0, -3.8, 5.0, 3.8),
    "U4": (-3.7, -2.7, 3.7, 2.7),
    "U1": (-3.7, -3.5, 3.7, 3.5),
    "U6": (-5.9, -9.25, 5.9, 9.25),
    "U3": (-3.9, -2.9, 3.9, 2.9),
    "U5": (-4.6, -3.8, 4.6, 3.8),
    "F1": (-3.0, -2.1, 3.0, 2.1),
    "RV1": (-5.5, -3.5, 5.5, 3.5),
    "TVS1": (-3.9, -2.5, 3.9, 2.5),
    "L1": (-3.5, -3.5, 3.5, 3.5),
    "D8": (-3.3, -1.6, 3.3, 1.6),
    "Q12": (-2.1, -1.9, 2.1, 1.9),
    "TVS2": (-2.2, -1.9, 2.2, 1.9),
    "LCM": (-1.7, -1.0, 1.7, 1.0),
    "J_PWR": (-6.2, -3.5, 6.2, 3.5),
    "J485": (-8.0, -3.5, 8.0, 3.5),
    "J2": (-1.3, -1.3, 1.3, 8.9),
    "NT1": (-3.2, -1.3, 3.2, 1.3),
    "JTERM": (-1.8, -1.4, 1.8, 1.4),
    "DS": (-25.45, -9.75, 25.45, 9.75),
    "SW": (-6.2, -6.2, 6.2, 6.2),
    "LED": (-2.3, -1.1, 2.3, 1.1),
    "BZ": (-2.45, -6.25, 10.05, 6.25),
}

_KIND = {
    "C2": "ELEC63",
    "C22": "ELEC63",
    "L2": "LCM",
    "L485": "LCM",
    "FB1": "0805",
    "FB2": "0805",
    "FB5V": "0805",
    "BZ1": "BZ",
    "QBZ": "Q12",
    "DREV": "D8",
}


def _kind(ref: str) -> str:
    if ref in _KIND:
        return _KIND[ref]
    if ref in _B:
        return ref
    if ref.startswith("Q") and ref[1:].isdigit():
        return "Q12"
    if ref.startswith("DS"):
        return "DS"
    if ref.startswith("SW"):
        return "SW"
    if ref.startswith("D") and ref[1:].isdigit():
        return "LED"
    return "0805"


def _rot_box(box: tuple[float, float, float, float], rot: int) -> tuple[float, float, float, float]:
    """KiCad positive rotation is CCW on the Y-down board."""
    x0, y0, x1, y1 = box
    rad = math.radians(rot)
    c, s = math.cos(rad), math.sin(rad)
    xs: list[float] = []
    ys: list[float] = []
    for lx, ly in ((x0, y0), (x0, y1), (x1, y0), (x1, y1)):
        xs.append(lx * c + ly * s)
        ys.append(-lx * s + ly * c)
    return (min(xs), min(ys), max(xs), max(ys))


def world_box(ref: str, x: float, y: float, rot: int) -> tuple[float, float, float, float]:
    x0, y0, x1, y1 = _rot_box(_B[_kind(ref)], rot)
    return (x + x0, y + y0, x + x1, y + y1)


def _overlap(a: tuple[float, float, float, float], b: tuple[float, float, float, float], gap: float) -> bool:
    return not (a[2] + gap <= b[0] or b[2] + gap <= a[0] or a[3] + gap <= b[1] or b[3] + gap <= a[1])


def _hits_hole(box: tuple[float, float, float, float], hx: float, hy: float, radius: float) -> bool:
    cx = min(max(hx, box[0]), box[2])
    cy = min(max(hy, box[1]), box[3])
    return math.hypot(hx - cx, hy - cy) < radius


BOTTOM_POS: dict[str, tuple[float, float, int]] = {}

# Each capacitor sits against the pin it bypasses. Duplicate ceramics are gone.
BOTTOM_POS["U6"] = (78.0, 86.0, 0)

BOTTOM_POS["U1"] = (95.0, 86.0, 180)
BOTTOM_POS["CVDD1"] = (100.3, 87.0, 90)
BOTTOM_POS["CNRST"] = (100.3, 83.6, 90)
BOTTOM_POS["J2"] = (94.5, 94.0, 90)

BOTTOM_POS["U3"] = (108.0, 84.0, 180)
BOTTOM_POS["R47A"] = (104.2, 76.2, 180)
BOTTOM_POS["R47B"] = (108.8, 76.2, 180)
BOTTOM_POS["CM3"] = (113.4, 76.2, 180)
BOTTOM_POS["RDE"] = (113.4, 89.5, 180)
BOTTOM_POS["TVS2"] = (108.0, 94.0, 180)
for _ref, _x in zip(("RBIAS_A", "RBIAS_B", "RTERM", "JTERM"), (82.6, 87.2, 91.8, 96.4)):
    BOTTOM_POS[_ref] = (_x, 110.2, 180)

# Power column, upper right. Local ceramics sit on the pin they serve.
BOTTOM_POS["J_PWR"] = (115.0, 16.0, 90)
BOTTOM_POS["C16"] = (84.7, 22.4, 180)
BOTTOM_POS["C17"] = (89.3, 22.4, 180)
BOTTOM_POS["C5"] = (107.7, 22.4, 180)
BOTTOM_POS["U4"] = (95.5, 32.0, 0)
BOTTOM_POS["C2"] = (109.0, 31.0, 90)
BOTTOM_POS["D8"] = (86.5, 43.0, 90)
BOTTOM_POS["L1"] = (93.0, 43.0, 0)
BOTTOM_POS["FB2"] = (99.6, 43.0, 180)
BOTTOM_POS["C22"] = (109.0, 42.5, 90)
BOTTOM_POS["FB5V"] = (96.2, 48.2, 180)
BOTTOM_POS["F1"] = (89.0, 55.0, 90)
BOTTOM_POS["C4"] = (108.0, 56.0, 90)
BOTTOM_POS["NT1"] = (88.0, 64.0, 180)
BOTTOM_POS["C19"] = (96.8, 64.2, 180)
BOTTOM_POS["C15"] = (104.0, 65.2, 180)
BOTTOM_POS["FB1"] = (73.2, 71.5, 180)
BOTTOM_POS["C7"] = (78.0, 71.5, 180)
BOTTOM_POS["U5"] = (91.0, 71.5, 180)
BOTTOM_POS["C3"] = (99.2, 76.2, 180)
BOTTOM_POS["C18"] = (86.5, 77.4, 180)
BOTTOM_POS["TVS1"] = (104.0, 70.0, 180)
BOTTOM_POS["J485"] = (115.0, 102.0, 90)

# Cấu hình thương mại (TM1638, STM32G030F6P6, linh kiện phổ biến)
BOTTOM_POS["FBIN"] = (96.8, 56.0, 90)
BOTTOM_POS["DREV"] = (112.0, 67.6, 90)
BOTTOM_POS["CLED1"] = (70.6, 81.6, 90)
BOTTOM_POS["CLED2"] = (82.5, 97.6, 0)
BOTTOM_POS["CLED3"] = (78.0, 97.6, 0)
BOTTOM_POS["RSTB"] = (86.5, 81.2, 0)
BOTTOM_POS["RCLK"] = (86.5, 84.0, 0)
BOTTOM_POS["RDIO"] = (86.5, 86.8, 0)

# Còi chủ động 12 mm ở mặt trên, tâm thân (40, 50): dải trống giữa module 7-seg dưới và hàng phím trên
# (mặt trên chỉ có chân hàn THT ở đây). Gốc footprint = chân 1, tâm thân lệch +3,8 mm. Mạch lái bên phải còi.
BOTTOM_POS["BZ1"] = (36.2, 50.0, 0)
BOTTOM_POS["QBZ"] = (51.5, 49.2, 0)
BOTTOM_POS["RBZG"] = (56.2, 49.2, 0)

FRONT_POS: dict[str, tuple[float, float, int]] = {}
for ref, (x, y) in digit_centers().items():
    FRONT_POS[ref] = (x, y, 180)
for ref, (x, y) in switch_centers().items():
    FRONT_POS[ref] = (x, y, 180)
for ref, (x, y) in status_centers().items():
    FRONT_POS[ref] = (x, y, 180)

MOUNT_POS = {
    "MH1": (BOARD_X0 + 5.0, 5.0),
    "MH2": (BOARD_W - 5.0, 5.0),
    "MH3": (BOARD_X0 + 5.0, BOARD_H - 5.0),
    "MH4": (BOARD_W - 5.0, BOARD_H - 5.0),
}


def placement_issues(gap: float = 1.0) -> list[str]:
    """Courtyard clashes, board edge, and mounting holes."""
    errs: list[str] = []
    boxes: dict[str, tuple[float, float, float, float]] = {}
    for ref, (x, y, rot) in {**BOTTOM_POS, **FRONT_POS}.items():
        box = world_box(ref, x, y, rot)
        boxes[ref] = box
        if ref not in BOTTOM_POS:
            continue
        if box[0] < BOARD_X0 + 1.5 or box[2] > BOARD_W - 1.5 or box[1] < 1.5 or box[3] > BOARD_H - 1.5:
            errs.append(f"EDGE {ref} {tuple(round(v, 1) for v in box)}")
        for name, (hx, hy) in MOUNT_POS.items():
            if _hits_hole(box, hx, hy, 4.3):
                errs.append(f"HOLE {ref} vs {name}")
    refs = list(boxes)
    for i, a in enumerate(refs):
        for b in refs[i + 1 :]:
            if a not in BOTTOM_POS and b not in BOTTOM_POS:
                continue
            if _overlap(boxes[a], boxes[b], gap):
                errs.append(f"OVERLAP {a} & {b}")
    return errs
