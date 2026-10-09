"""Check PCB footprint positions vs front_layout_spec."""

from __future__ import annotations

import re
import sys
from pathlib import Path

from front_layout_spec import (
    KEY_AXIS_X,
    MAIN_AXIS_X,
    DIGIT_ROW_PITCH_Y,
    MIN_GAP_KEYPAD_TO_DIGIT_MM,
    STATUS_X,
    digit_block_center_x_mm,
    digit_bottom_row_y_mm,
    digit_centers,
    keypad_center_x_mm,
    keypad_top_y_mm,
    status_centers,
    switch_centers,
)

ROOT = Path(__file__).resolve().parents[1]
PCB = ROOT / "panel-keyboard.kicad_pcb"
TOL = 0.01  # mm — lỗ phay mặt máy tính từ tọa độ spec, PCB phải trùng chính xác


def parse_positions(text: str) -> dict[str, tuple[float, float, str]]:
    pos: dict[str, tuple[float, float, str]] = {}
    blocks = re.findall(r"\(footprint\b.*?\n\t\)", text, re.DOTALL)
    for block in blocks:
        ref_m = re.search(r'\(property "Reference" "([^"]+)"', block)
        at_m = re.search(r"\(at\s+([\d.-]+)\s+([\d.-]+)", block)
        layer_m = re.search(r'\(layer "([^"]+)"\)', block)
        if ref_m and at_m and layer_m:
            pos[ref_m.group(1)] = (float(at_m.group(1)), float(at_m.group(2)), layer_m.group(1))
    return pos


def check(expected: dict[str, tuple[float, float]], actual: dict[str, tuple[float, float, str]]) -> list[str]:
    errs: list[str] = []
    for ref, (ex, ey) in sorted(expected.items()):
        if ref not in actual:
            errs.append(f"MISSING {ref} expected ({ex},{ey})")
            continue
        ax, ay, layer = actual[ref]
        if layer != "B.Cu":
            errs.append(f"{ref}: keys and LEDs belong on B.Cu, got {layer}")
        if abs(ax - ex) > TOL or abs(ay - ey) > TOL:
            errs.append(f"{ref}: expected ({ex},{ey}) got ({ax:.2f},{ay:.2f})")
    return errs


def main() -> int:
    if not PCB.is_file():
        print("No PCB:", PCB)
        return 2
    actual = parse_positions(PCB.read_text(encoding="utf-8"))
    expected = {**digit_centers(), **switch_centers(), **status_centers()}
    errs = check(expected, actual)
    gap = keypad_top_y_mm() - digit_bottom_row_y_mm()
    if gap < MIN_GAP_KEYPAD_TO_DIGIT_MM:
        errs.append(
            f"GAP keypad<->7seg: {gap:.1f} mm < {MIN_GAP_KEYPAD_TO_DIGIT_MM} mm "
            f"(keypad top ~{keypad_top_y_mm():.1f}, digit bottom ~{digit_bottom_row_y_mm():.1f})"
        )
    else:
        print(f"OK gap keypad->7seg lower row: {gap:.1f} mm (>= {MIN_GAP_KEYPAD_TO_DIGIT_MM})")
    kx, dx = keypad_center_x_mm(), digit_block_center_x_mm()
    if abs(dx - MAIN_AXIS_X) > 0.05 or abs(kx - KEY_AXIS_X) > 0.05:
        errs.append(f"AXIS center misaligned: keypad {kx:.2f} (spec {KEY_AXIS_X}), digit {dx:.2f} (spec {MAIN_AXIS_X})")
    else:
        print(f"OK main axis X={MAIN_AXIS_X:.1f} mm (7-seg), keypad X={KEY_AXIS_X:.1f}")
    ys = sorted(y for _, y in digit_centers().values())
    if any(abs((b - a) - DIGIT_ROW_PITCH_Y) > 0.05 for a, b in zip(ys, ys[1:])):
        errs.append(f"7-seg row pitch expected {DIGIT_ROW_PITCH_Y} mm, got {ys}")
    else:
        print(f"OK 7-seg module rows Y={ys}, pitch {DIGIT_ROW_PITCH_Y} mm")
    sx = {actual[r][0] for r in status_centers() if r in actual}
    if len(sx) > 1 or (sx and abs(next(iter(sx)) - STATUS_X) > TOL):
        errs.append(f"STATUS column X expected {STATUS_X:.1f}, got {sx}")
    elif sx:
        print(f"OK status column X={STATUS_X:.1f} mm (gap from main block)")
    if errs:
        print("LAYOUT ISSUES:")
        for e in errs:
            print(" -", e)
        return 1
    print(f"OK all {len(expected)} front refs within {TOL} mm")
    return 0


if __name__ == "__main__":
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    sys.exit(main())
