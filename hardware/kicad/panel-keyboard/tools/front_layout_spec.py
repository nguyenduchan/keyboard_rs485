"""Front-panel layout (mm).

KiCad plot/SVG: gốc góc trên-trái của trang, Y tăng xuống dưới.
Cạnh trên màn hình = Y nhỏ. Cạnh dưới = Y lớn.
"""

from __future__ import annotations

PANEL_WIDTH_MM = 100.0
PANEL_HEIGHT_MM = 150.0
# Tâm keypad/7-seg. Cột phải của bo 120 mm dành cho nguồn và MCU.
MAIN_AXIS_X = 40.0
KEY_AXIS_X = 42.0      # keypad dịch phải 2 mm so với 7-seg để góc lỗ phím không sát lỗ vít M3

KEY_COLS = 4
KEY_ROWS = 4
DIGIT_COLS = 4
DIGIT_ROWS = 2
STATUS_COUNT = 8

KEY_PITCH = 16.0
# Module 4 số 0,56" (5641AS): thân 50,4 × 19,05 mm, bước digit 12,7 mm.
# Tâm hai hàng đặt trên số nguyên mm (Y 15 / 35, bước 20) để cửa sổ phay mặt máy có mép nguyên mm.
DIGIT_MODULE_W_MM = 50.4
DIGIT_BODY_W_MM = 12.70
DIGIT_BODY_H_MM = 19.05
DIGIT_ROW_PITCH_Y = 20.0
SWITCH_HALF_MM = 6.0
DIGIT_HALF_H_MM = DIGIT_BODY_H_MM / 2

STATUS_PITCH_Y = 8.0
STATUS_LED_HALF_MM = 1.5
# Khe thân phím → tâm LED 10 mm: cột LED tại X = 80 (số nguyên, cho lỗ phay).
STATUS_GAP_MM = 8.5
# Khe giữa thân 7-seg và thân phím, đủ cho gân mặt máy giữa cửa sổ 7-seg và lỗ phím.
MIN_GAP_KEYPAD_TO_DIGIT_MM = 8.0

# Hai hàng sát nhau (khe thân 0,95 mm, chung một cửa sổ phay). SW13 tại Y=59, mép thân phím Y=53.
DIGIT_ROW_TOP_Y = 15.0
DIGIT_ROW_BOT_Y = DIGIT_ROW_TOP_Y + DIGIT_ROW_PITCH_Y  # 35, mép dưới thân 44,525
KEY_ROW0_Y = 107.0      # SW1; SW13 = 59. Mép dưới thân phím = 113.


def _grid_x(col: int, n_cols: int, pitch: float, axis: float = MAIN_AXIS_X) -> float:
    return axis + (col - (n_cols - 1) / 2) * pitch


def keypad_center_y_mm() -> float:
    return KEY_ROW0_Y - (KEY_ROWS - 1) / 2 * KEY_PITCH


def main_column_right_x_mm() -> float:
    n = max(KEY_COLS, DIGIT_COLS)
    right_col = _grid_x(n - 1, n, KEY_PITCH)
    return right_col + SWITCH_HALF_MM


def status_column_x_mm() -> float:
    return main_column_right_x_mm() + STATUS_GAP_MM + STATUS_LED_HALF_MM


STATUS_X = status_column_x_mm()


def digit_centers() -> dict[str, tuple[float, float]]:
    """Hai module 4 số 0,56" (5641AS): DS1 hàng trên (digit 1–4), DS2 hàng dưới (digit 5–8).

    Thân module 50,4 × 19,0 mm, bước digit 12,7 mm; tâm module = tâm keypad.
    """
    return {
        "DS1": (MAIN_AXIS_X, DIGIT_ROW_TOP_Y),
        "DS2": (MAIN_AXIS_X, DIGIT_ROW_BOT_Y),
    }


def switch_centers() -> dict[str, tuple[float, float]]:
    out: dict[str, tuple[float, float]] = {}
    for n in range(1, KEY_COLS * KEY_ROWS + 1):
        row, col = (n - 1) // KEY_COLS, (n - 1) % KEY_COLS
        out[f"SW{n}"] = (
            _grid_x(col, KEY_COLS, KEY_PITCH, KEY_AXIS_X),
            KEY_ROW0_Y - row * KEY_PITCH,
        )
    return out


def status_centers() -> dict[str, tuple[float, float]]:
    """D10 sát cạnh trên trang (Y nhỏ), cột phải."""
    top = 12.0
    ys = [top + i * STATUS_PITCH_Y for i in range(STATUS_COUNT)]
    return {f"D{10 + i}": (STATUS_X, ys[i]) for i in range(STATUS_COUNT)}


def keypad_top_y_mm() -> float:
    """Mép keypad phía 7-seg (Y nhỏ hơn)."""
    sw13 = KEY_ROW0_Y - (KEY_ROWS - 1) * KEY_PITCH
    return sw13 - SWITCH_HALF_MM


def digit_bottom_row_y_mm() -> float:
    """Mép dưới thân hàng 7-seg dưới (thân 19,05 mm, tâm tại anchor)."""
    return DIGIT_ROW_BOT_Y + DIGIT_HALF_H_MM


def keypad_center_x_mm() -> float:
    return KEY_AXIS_X


def digit_block_center_x_mm() -> float:
    """Tâm ngang cụm module 7-seg."""
    xs = [x for x, _ in digit_centers().values()]
    return (min(xs) + max(xs)) / 2
