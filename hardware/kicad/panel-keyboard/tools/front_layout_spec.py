"""Front-panel layout (mm).

KiCad plot/SVG: gốc góc trên-trái của trang, Y tăng xuống dưới.
Cạnh trên màn hình = Y nhỏ. Cạnh dưới = Y lớn.
"""

from __future__ import annotations

PANEL_WIDTH_MM = 100.0
PANEL_HEIGHT_MM = 150.0
# Lệch 3 mm sang trái so với tâm bo để cột phải đủ rộng cho khối MCU (đi dây).
MAIN_AXIS_X = 47.0

KEY_COLS = 4
KEY_ROWS = 4
DIGIT_COLS = 4
DIGIT_ROWS = 2
STATUS_COUNT = 8

KEY_PITCH = 16.0
DIGIT_ROW_PITCH_Y = 22.0
SWITCH_HALF_MM = 6.0
DIGIT_HALF_H_MM = 10.0  # thân 7-seg chủ yếu về phía −Y local

STATUS_PITCH_Y = 8.0
STATUS_LED_HALF_MM = 1.5
STATUS_GAP_MM = 8.0
MIN_GAP_KEYPAD_TO_DIGIT_MM = 8.0

# 7-seg sát cạnh trên (Y nhỏ). SW1 (KEY_ROW0) thấp hơn SW13 trên màn hình.
DIGIT_ROW_TOP_Y = 14.0  # DS1–DS4
DIGIT_ROW_BOT_Y = 36.0  # DS5–DS8
KEY_ROW0_Y = 100.0      # SW1; SW13 = 52 (gần 7-seg). Mép dưới phím = 106.


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
    out: dict[str, tuple[float, float]] = {}
    for i in range(DIGIT_COLS):
        x = _grid_x(i, DIGIT_COLS, KEY_PITCH)
        out[f"DS{i + 1}"] = (x, DIGIT_ROW_TOP_Y)
        if DIGIT_ROWS >= 2:
            out[f"DS{i + 1 + DIGIT_COLS}"] = (x, DIGIT_ROW_BOT_Y)
    return out


def switch_centers() -> dict[str, tuple[float, float]]:
    out: dict[str, tuple[float, float]] = {}
    for n in range(1, KEY_COLS * KEY_ROWS + 1):
        row, col = (n - 1) // KEY_COLS, (n - 1) % KEY_COLS
        out[f"SW{n}"] = (
            _grid_x(col, KEY_COLS, KEY_PITCH),
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
    """Mép 7-seg phía keypad (thân chủ yếu về −Y, mép +Y ≈ +2 mm)."""
    return DIGIT_ROW_BOT_Y + 2.0


def keypad_center_x_mm() -> float:
    return MAIN_AXIS_X


def digit_block_center_x_mm() -> float:
    return MAIN_AXIS_X
