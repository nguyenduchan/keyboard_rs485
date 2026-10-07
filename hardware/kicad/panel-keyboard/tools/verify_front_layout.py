"""Check PCB footprint positions vs front_layout_spec."""

from __future__ import annotations

import re
import sys
from pathlib import Path

from front_layout_spec import (
    MAIN_AXIS_X,
    DIGIT_COLS,
    DIGIT_PITCH_TOP,
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
    if abs(kx - dx) > 0.05 or abs(kx - MAIN_AXIS_X) > 0.05:
        errs.append(f"AXIS center misaligned: keypad {kx:.2f}, digit {dx:.2f}, spec {MAIN_AXIS_X}")
    else:
        print(f"OK main axis X={MAIN_AXIS_X:.1f} mm (keypad + 7-seg + cutout)")
    top_xs = [digit_centers()[f"DS{i + 1}"][0] for i in range(DIGIT_COLS)]
    pitches = [round(top_xs[i + 1] - top_xs[i], 2) for i in range(DIGIT_COLS - 1)]
    if any(abs(p - DIGIT_PITCH_TOP) > 0.05 for p in pitches):
        errs.append(f"TOP row pitch expected {DIGIT_PITCH_TOP} mm, got {pitches}")
    else:
        print(f"OK top 7-seg pitch {DIGIT_PITCH_TOP} mm (body 12.70 + gap 1.0)")
    bot_xs = [digit_centers()[f"DS{i + 5}"][0] for i in range(DIGIT_COLS)]
    bot_pitches = [round(bot_xs[i + 1] - bot_xs[i], 2) for i in range(DIGIT_COLS - 1)]
    if bot_xs != top_xs or any(abs(p - DIGIT_PITCH_TOP) > 0.05 for p in bot_pitches):
        errs.append(f"BOTTOM row pitch expected {DIGIT_PITCH_TOP} mm aligned with top, got {bot_xs}")
    else:
        print(f"OK lower 7-seg pitch {DIGIT_PITCH_TOP} mm, same X as top row")
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
