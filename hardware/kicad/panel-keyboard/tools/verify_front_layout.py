"""Check PCB footprint positions vs front_layout_spec."""

from __future__ import annotations

import re
import sys
from pathlib import Path

from front_layout_spec import (
    MAIN_AXIS_X,
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
TOL = 1.0  # mm


def parse_positions(text: str) -> dict[str, tuple[float, float]]:
    pos: dict[str, tuple[float, float]] = {}
    blocks = re.findall(r"\(footprint\b.*?\n\t\)", text, re.DOTALL)
    for block in blocks:
        ref_m = re.search(r'\(property "Reference" "([^"]+)"', block)
        at_m = re.search(r"\(at\s+([\d.-]+)\s+([\d.-]+)", block)
        if ref_m and at_m:
            pos[ref_m.group(1)] = (float(at_m.group(1)), float(at_m.group(2)))
    return pos


def check(expected: dict[str, tuple[float, float]], actual: dict[str, tuple[float, float]]) -> list[str]:
    errs: list[str] = []
    for ref, (ex, ey) in sorted(expected.items()):
        if ref not in actual:
            errs.append(f"MISSING {ref} expected ({ex},{ey})")
            continue
        ax, ay = actual[ref]
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
    if abs(kx - dx) > 0.05 or abs(kx - MAIN_AXIS_X) > 0.05:
        errs.append(f"AXIS center misaligned: keypad {kx:.2f}, digit {dx:.2f}, spec {MAIN_AXIS_X}")
    else:
        print(f"OK main axis X={MAIN_AXIS_X:.1f} mm (keypad + 7-seg + cutout)")
    col_ok = True
    for col in range(4):
        swx = switch_centers()[f"SW{1 + col}"][0]
        dsx = digit_centers()[f"DS{col + 1}"][0]
        if abs(swx - dsx) > 0.05:
            errs.append(f"COL {col} SW/DS X mismatch {swx} vs {dsx}")
            col_ok = False
    if col_ok:
        print("OK 7-seg columns aligned with keypad")
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
