"""SMD ở đáy trang KiCad (Y lớn), 3 cột trái→phải: bảo vệ | buck | MCU.

Plot KiCad: Y nhỏ = cạnh trên. Không đặt SMD vào vùng THT (Y nhỏ hơn dải mạch).
"""

from __future__ import annotations

from front_layout_spec import digit_centers, status_centers, status_column_x_mm, switch_centers

# Dải nguồn dưới keypad (SW1 tâm 100, mép 106). MCU là cột cao bên phải, dưới LED.
ELEC_Y0 = 108.0
ELEC_Y1 = 131.0
GAP_Y_MIN = 106.0
GAP_Y_MAX = 108.0

ZONE_PWR_24V = {"x0": 4.0, "x1": 40.0, "y0": ELEC_Y0, "y1": ELEC_Y1, "label": "24V_PROTECT"}
ZONE_BUCK = {"x0": 42.0, "x1": 76.0, "y0": ELEC_Y0, "y1": ELEC_Y1, "label": "BUCK_5V"}
ZONE_MCU = {"x0": 78.0, "x1": 97.0, "y0": 74.0, "y1": ELEC_Y1, "label": "MCU_LOGIC"}

BOTTOM_UI_MAX_Y = ELEC_Y1
FRONT_KEEPOUT_X0 = 17.0
FRONT_KEEPOUT_X1 = 77.0
FRONT_KEEPOUT_Y0 = 4.0
FRONT_KEEPOUT_Y1 = 106.0


def _grid(zone: dict, refs: list[str], cols: int, rows: int) -> dict[str, tuple[float, float, int]]:
    x0, x1 = zone["x0"] + 3.0, zone["x1"] - 3.0
    y0, y1 = zone["y0"] + 3.0, zone["y1"] - 3.0
    out: dict[str, tuple[float, float, int]] = {}
    for i, ref in enumerate(refs):
        row, col = divmod(i, cols)
        if row >= rows:
            row = rows - 1
            col = i - (rows - 1) * cols
        x = x0 + (x1 - x0) * (col / max(cols - 1, 1))
        y = y1 - (y1 - y0) * (row / max(rows - 1, 1))
        rot = 90 if ref in ("TVS1", "D8", "L1", "TVS2", "J2") else 0
        out[ref] = (round(x, 2), round(y, 2), rot)
    return out


def _pack(refs: list[str], xs: list[float], ys: list[float], rot: int = 0) -> dict[str, tuple[float, float, int]]:
    out: dict[str, tuple[float, float, int]] = {}
    for i, ref in enumerate(refs):
        out[ref] = (round(xs[i % len(xs)], 2), round(ys[i // len(xs)], 2), rot)
    return out


BOTTOM_POS: dict[str, tuple[float, float, int]] = {}
# Bảo vệ: C4 lớn bên trái, linh kiện nhỏ lưới 6 mm bên phải.
BOTTOM_POS.update(
    {
        "C4": (14.0, 119.0, 0),
        "NT1": (14.0, 128.0, 0),
        "TVS1": (24.0, 112.0, 90),
        "F1": (32.0, 112.0, 0),
        "L2": (24.0, 119.0, 0),
        "RV1": (32.0, 119.0, 0),
        "Q12": (24.0, 126.0, 0),
        "R17": (32.0, 126.0, 0),
        "C19": (36.0, 112.0, 0),
        "C15": (36.0, 119.0, 0),
        "C14": (36.0, 126.0, 0),
    }
)
# Buck: U4 giữa, passives xung quanh, bước >= 6 mm.
BOTTOM_POS.update(
    {
        "U4": (52.0, 116.0, 0),
        "R38": (44.0, 112.0, 0),
        "R36": (44.0, 118.0, 0),
        "R37": (44.0, 124.0, 0),
        "C16": (60.0, 112.0, 0),
        "C17": (66.0, 112.0, 0),
        "D8": (72.0, 112.0, 90),
        "L1": (66.0, 118.0, 90),
        "FB2": (72.0, 118.0, 0),
        "C2": (60.0, 126.0, 0),
        "C5": (66.0, 126.0, 0),
        "C22": (72.0, 126.0, 0),
        "FB5V": (54.0, 126.0, 0),
    }
)
# MCU: U2 trên cùng cột phải; U1/U5/U3 một hàng; passives lưới có khe ~0.7 mm.
BOTTOM_POS["U2"] = (87.5, 82.0, 0)
BOTTOM_POS["U1"] = (82.0, 96.0, 90)
BOTTOM_POS["U5"] = (88.5, 96.0, 90)
BOTTOM_POS["U3"] = (94.0, 96.0, 0)
_mcu_small = [
    "J2", "C7", "C3", "C18", "FB1",
    "C21", "CVDDA", "CVDD1", "RNST", "CNRST",
    "RBOT0", "CM3", "RDE", "R47A", "R47B",
    "L485", "RBIAS_A", "RBIAS_B", "RTERM", "JTERM",
    "TVS2", "R33A", "R33B", "R33C", "RPU",
    "CTM1", "CTM2", "RK1", "RK2", "RK3",
    "RK4", "CK1", "CK2", "CK3", "CK4",
    "RC1", "RC2", "RC3", "RC4", "CC1",
    "CC2", "CC3", "CC4",
]
BOTTOM_POS.update(
    _pack(
        _mcu_small,
        [80.0, 83.6, 87.2, 90.8, 94.4],
        [103.0 + i * 3.2 for i in range(9)],
        rot=0,
    )
)

# Sát cạnh dưới trang (Y=150). Dải Y 133–140 để silk tên giắc / tên chân.
BOTTOM_POS["J_PWR"] = (22.0, 146.0, 0)
BOTTOM_POS["J485"] = (86.0, 146.0, 0)

_rx = min(status_column_x_mm() - 4.0, 84.0)
for i in range(1, 9):
    _y = status_centers()[f"D{9 + i}"][1]
    BOTTOM_POS[f"RLED{i}"] = (round(_rx, 2), round(_y, 2), 0)

FRONT_POS: dict[str, tuple[float, float, int]] = {}
for ref, (x, y) in digit_centers().items():
    FRONT_POS[ref] = (x, y, 0)
for ref, (x, y) in switch_centers().items():
    FRONT_POS[ref] = (x, y, 0)
for ref, (x, y) in status_centers().items():
    FRONT_POS[ref] = (x, y, 0)
