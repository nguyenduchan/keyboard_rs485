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
ZONE_BUCK = {"x0": 82.0, "x1": 118.5, "y0": 36.0, "y1": 59.0, "label": "BUCK_5V"}
ZONE_PWR_24V = {"x0": 83.0, "x1": 118.5, "y0": 4.0, "y1": 34.5, "label": "INPUT_24V"}
ZONE_MCU = {"x0": 90.5, "x1": 103.5, "y0": 76.0, "y1": 108.5, "label": "MCU_LOGIC"}

BOTTOM_UI_MAX_Y = BOARD_H

# Courtyard relative to footprint origin, rot 0: (xmin, ymin, xmax, ymax).
# Measured from the footprints on the board, plus the body of the terminals.
_B = {
    "0805": (-1.75, -1.05, 1.75, 1.05),
    "C4": (-7.0, -6.2, 7.0, 6.2),
    "ELEC63": (-4.7, -3.6, 4.7, 3.6),
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
    "DS": (-7.5, -8.9, 7.5, 8.9),     # chỉ vùng chân THT; thân module nằm mặt dưới
    "SW": (-3.7, -7.4, 3.7, 7.4),     # chân 12,5 dọc Y (SW1–SW12)
    "SWX": (-7.4, -3.7, 7.4, 3.7),    # chân 12,5 dọc X (SW13–SW16)
    "LED": (-2.3, -1.1, 2.3, 1.1),
    "BZ": (-2.45, -6.25, 10.05, 6.25),
    "J_KEY": (-4.9, -3.07, 1.6, 25.93),
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
    "J_SWD": "J2",
    "J_LOG": "J2",
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
        return "SWX" if int(ref[2:]) >= 13 else "SW"
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

# Nhóm chức năng, mỗi nhóm một khối gọn, khe ≥ 2 mm giữa courtyard để còn chỗ đi dây.
# Dòng công suất đi từ trên xuống: INPUT → BUCK → (+5V) → LDO ở hàng dưới; MCU / RS485 / TM1638 ở giữa-dưới.

# --- INPUT: giắc 24 V, cầu chì, lọc, TVS, chống ngược cực, tụ bulk (x 84–118, y 4–36) ---
BOTTOM_POS["J_PWR"] = (115.0, 16.0, 90)
BOTTOM_POS["F1"] = (107.0, 8.0, 0)
BOTTOM_POS["FBIN"] = (99.5, 8.0, 0)
BOTTOM_POS["TVS1"] = (90.5, 8.0, 0)
BOTTOM_POS["C19"] = (99.5, 16.0, 0)
BOTTOM_POS["C15"] = (105.0, 16.0, 0)
BOTTOM_POS["DREV"] = (88.0, 17.0, 0)
BOTTOM_POS["C4"] = (104.0, 28.0, 90)
BOTTOM_POS["C16"] = (86.0, 25.0, 0)
BOTTOM_POS["C17"] = (91.0, 25.0, 0)

# --- BUCK 24 V → 5 V (x 82–118, y 36–58) ---
BOTTOM_POS["U4"] = (96.0, 40.0, 0)
BOTTOM_POS["D8"] = (86.0, 42.0, 90)
BOTTOM_POS["L1"] = (108.0, 40.0, 0)
BOTTOM_POS["C5"] = (114.0, 28.0, 90)
BOTTOM_POS["C2"] = (113.3, 49.0, 0)
BOTTOM_POS["C22"] = (102.8, 49.0, 0)
BOTTOM_POS["FB2"] = (100.0, 57.0, 0)
BOTTOM_POS["FB5V"] = (105.5, 57.0, 0)
BOTTOM_POS["NT1"] = (113.0, 60.0, 0)

# --- TM1638 (x 70–90, y 71–101) ---
BOTTOM_POS["U6"] = (78.0, 86.0, 0)
BOTTOM_POS["CLED1"] = (76.0, 73.0, 0)
BOTTOM_POS["CLED2"] = (82.0, 99.0, 0)
BOTTOM_POS["CLED3"] = (77.0, 99.0, 0)
BOTTOM_POS["RSTB"] = (88.0, 79.0, 0)
BOTTOM_POS["RCLK"] = (88.0, 84.0, 0)
BOTTOM_POS["RDIO"] = (88.0, 89.0, 0)

# --- MCU STM32G030 + giắc nạp (x 90–103, y 76–108) ---
BOTTOM_POS["U1"] = (95.0, 86.0, 180)
BOTTOM_POS["CVDD1"] = (102.0, 90.0, 90)
BOTTOM_POS["CNRST"] = (102.0, 82.0, 90)
# DAPLink 1×4, chân 1 (3V3) phía trên. J_SWD rồi J_LOG sát nhau, dưới U1.
BOTTOM_POS["J_SWD"] = (92.5, 98.0, 0)
BOTTOM_POS["J_LOG"] = (97.5, 98.0, 0)

# --- RS485 (x 104–118, y 72–116) ---
BOTTOM_POS["U3"] = (109.0, 84.0, 180)
BOTTOM_POS["R47A"] = (105.0, 76.0, 180)
BOTTOM_POS["R47B"] = (110.0, 76.0, 180)
BOTTOM_POS["CM3"] = (115.0, 76.0, 180)
BOTTOM_POS["RDE"] = (115.0, 90.0, 180)
BOTTOM_POS["TVS2"] = (108.0, 96.0, 180)
BOTTOM_POS["J485"] = (115.0, 102.0, 90)
BOTTOM_POS["RTERM"] = (108.0, 101.0, 180)
BOTTOM_POS["JTERM"] = (108.0, 105.0, 180)
BOTTOM_POS["RBIAS_A"] = (108.0, 109.0, 180)
BOTTOM_POS["RBIAS_B"] = (109.0, 113.0, 180)

# --- LDO 5 V → 3,3 V, hàng dưới cùng (x 77–105, y 108–118) ---
BOTTOM_POS["C7"] = (79.0, 113.0, 180)
BOTTOM_POS["U5"] = (87.0, 113.0, 180)
BOTTOM_POS["C18"] = (95.0, 113.0, 180)
BOTTOM_POS["C3"] = (99.5, 113.0, 180)
BOTTOM_POS["FB1"] = (104.0, 113.0, 180)

# Còi chủ động 12 mm ở mặt trên, góc dưới-trái khối BUCK (mặt dưới chỗ này chỉ có cột LED X ≤ 82,2).. Gốc footprint = chân 1, tâm thân lệch +3,8 mm. Mạch lái bên phải còi.
BOTTOM_POS["BZ1"] = (86.2, 56.5, 0)   # tâm thân (90, 56,5), trong vùng nguồn, cách mọi linh kiện / courtyard khác ≥ 1,4 mm
BOTTOM_POS["QBZ"] = (100.0, 62.0, 0)
BOTTOM_POS["RBZG"] = (106.0, 62.0, 0)

# Giắc bàn phím màng ở dải trái (KiCad), trống cả hai mặt, ngoài thân module 7-seg mặt dưới (X ≥ 14,8).
# Header đực thẳng 1×10 cột X=11,4; khe phay PCB X 7,0…9,6, Y 15,5…43,5 ngay bên trái. Cánh tủ có khe
# 4 × 29 mm trùng khe này, cách cửa sổ 7-seg 4 mm (tools/cnc_panel.py). Đuôi màng luồn qua hai khe,
# vòng lại và cắm từ trên xuống — cắm/rút được khi bo đã bắt vít.
BOTTOM_POS["J_KEY"] = (11.4, 18.07, 0)

# Khung nhóm vẽ trên lớp F.SilkS: (x0, y0, x1, y1, nhãn) hoặc ("poly", [(x, y), ...], nhãn)
GROUPS = [
    (83.0, 4.0, 118.5, 34.5, "INPUT 24V"),
    ("poly", [(82.0, 36.0), (118.5, 36.0), (118.5, 64.0), (97.5, 64.0), (97.5, 49.0), (82.0, 49.0)], "BUCK 5V"),
    (70.0, 70.5, 90.5, 102.0, "TM1638"),
    (90.5, 76.0, 103.5, 108.5, "MCU"),
    (103.5, 72.0, 118.5, 116.5, "RS485"),
    (76.0, 108.5, 106.0, 117.5, "LDO 3V3"),
    (82.0, 49.0, 97.5, 64.0, "BUZZER"),
]

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
