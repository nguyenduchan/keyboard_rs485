"""Move footprints to FRONT_POS / BOTTOM_POS and drop copper routes.

Replacements are applied from the end of the file backward.
Board outline, mounting holes, and zone silk follow zone_layout.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from zone_layout import (
    BOARD_H,
    BOARD_W,
    BOARD_X0,
    BOTTOM_POS,
    FRONT_POS,
    MOUNT_POS,
    ZONE_BUCK,
    ZONE_MCU,
    ZONE_PWR_24V,
)

ROOT = Path(__file__).resolve().parents[1]
PCB = ROOT / "panel-keyboard.kicad_pcb"
POS = {**FRONT_POS, **BOTTOM_POS, **{k: (x, y, 0) for k, (x, y) in MOUNT_POS.items()}}


def footprint_spans(text: str) -> list[tuple[int, int]]:
    spans: list[tuple[int, int]] = []
    key = "(footprint "
    i = 0
    while True:
        j = text.find(key, i)
        if j < 0:
            break
        depth = 0
        for k in range(j, len(text)):
            if text[k] == "(":
                depth += 1
            elif text[k] == ")":
                depth -= 1
                if depth == 0:
                    spans.append((j, k + 1))
                    i = k + 1
                    break
        else:
            break
    return spans


def main() -> int:
    text = PCB.read_text(encoding="utf-8")
    edits: list[tuple[int, int, str]] = []
    moved = 0
    for start, end in footprint_spans(text):
        block = text[start:end]
        ref_m = re.search(r'\(property "Reference" "([^"]+)"', block)
        if not ref_m or ref_m.group(1) not in POS:
            continue
        x, y, rot = POS[ref_m.group(1)]
        at = f"(at {x:g} {y:g}" + (f" {rot:g}" if rot else "") + ")"
        new_block, n = re.subn(
            r"\(at\s+[-+\d.]+\s+[-+\d.]+(?:\s+[-+\d.]+)?\)",
            at,
            block,
            count=1,
        )
        if n and new_block != block:
            edits.append((start, end, new_block))
            moved += 1
    for start, end, new_block in reversed(edits):
        text = text[:start] + new_block + text[end:]
    segs = len(re.findall(r"\(segment\b", text))
    vias = len(re.findall(r"\(via\b", text))
    text = re.sub(r"\n\t\(segment\b.*?\n\t\)", "", text, flags=re.DOTALL)
    text = re.sub(r"\n\t\(via\b.*?\n\t\)", "", text, flags=re.DOTALL)
    text = text.replace("(end 100 150)", f"(end {BOARD_W:g} {BOARD_H:g})", 1)
    text = text.replace("(end 100 274)", f"(end {BOARD_W:g} {BOARD_H:g})", 1)
    text = text.replace("(end 100 156)", f"(end {BOARD_W:g} {BOARD_H:g})", 1)
    text = text.replace(
        "(start 0 0)\n\t\t(end 120 120)",
        f"(start {BOARD_X0:g} 0)\n\t\t(end {BOARD_W:g} {BOARD_H:g})",
        1,
    )
    text = text.replace(
        "(start -42 0)\n\t\t(end 100 112)",
        f"(start {BOARD_X0:g} 0)\n\t\t(end {BOARD_W:g} {BOARD_H:g})",
        1,
    )
    text = text.replace(
        "(start 4 116)\n\t\t(end 96 186)",
        f"(start {ZONE_MCU['x0']:g} {ZONE_MCU['y0']:g})\n\t\t(end {ZONE_MCU['x1']:g} {ZONE_MCU['y1']:g})",
        1,
    )
    text = text.replace(
        "(start 4 186)\n\t\t(end 96 224)",
        f"(start {ZONE_BUCK['x0']:g} {ZONE_BUCK['y0']:g})\n\t\t(end {ZONE_BUCK['x1']:g} {ZONE_BUCK['y1']:g})",
        1,
    )
    text = text.replace(
        "(start 4 222)\n\t\t(end 96 272)",
        f"(start {ZONE_PWR_24V['x0']:g} {ZONE_PWR_24V['y0']:g})\n\t\t(end {ZONE_PWR_24V['x1']:g} {ZONE_PWR_24V['y1']:g})",
        1,
    )
    text = text.replace("(start 10 108)\n\t\t(end 94 116)", "(start 4 104)\n\t\t(end 96 108)", 1)
    text = text.replace(
        "(start 76 66)\n\t\t(end 98 98)",
        f"(start {ZONE_MCU['x0']:g} {ZONE_MCU['y0']:g})\n\t\t(end {ZONE_MCU['x1']:g} {ZONE_MCU['y1']:g})",
        1,
    )
    text = text.replace(
        "(start 4 108)\n\t\t(end 96 132)",
        f"(start {ZONE_BUCK['x0']:g} {ZONE_BUCK['y0']:g})\n\t\t(end {ZONE_BUCK['x1']:g} {ZONE_BUCK['y1']:g})",
        1,
    )
    text = text.replace(
        "(start 4 132)\n\t\t(end 96 154)",
        f"(start {ZONE_PWR_24V['x0']:g} {ZONE_PWR_24V['y0']:g})\n\t\t(end {ZONE_PWR_24V['x1']:g} {ZONE_PWR_24V['y1']:g})",
        1,
    )
    text = text.replace("(start 4 104)\n\t\t(end 96 108)", "(start 16 106)\n\t\t(end 78 110)", 1)
    text = text.replace(
        "(start 76 66)\n\t\t(end 94 96)",
        f"(start {ZONE_MCU['x0']:g} {ZONE_MCU['y0']:g})\n\t\t(end {ZONE_MCU['x1']:g} {ZONE_MCU['y1']:g})",
        1,
    )
    text = text.replace(
        "(start 93 40)\n\t\t(end 118 86)",
        f"(start {ZONE_BUCK['x0']:g} {ZONE_BUCK['y0']:g})\n\t\t(end {ZONE_BUCK['x1']:g} {ZONE_BUCK['y1']:g})",
        1,
    )
    text = text.replace(
        "(start 86 86)\n\t\t(end 118 112)",
        f"(start {ZONE_PWR_24V['x0']:g} {ZONE_PWR_24V['y0']:g})\n\t\t(end {ZONE_PWR_24V['x1']:g} {ZONE_PWR_24V['y1']:g})",
        1,
    )
    text = text.replace(
        "(start -34 48)\n\t\t(end -8 68)",
        f"(start {ZONE_MCU['x0']:g} {ZONE_MCU['y0']:g})\n\t\t(end {ZONE_MCU['x1']:g} {ZONE_MCU['y1']:g})",
        1,
    )
    text = text.replace(
        "(start -34 4)\n\t\t(end 2 32)",
        f"(start {ZONE_BUCK['x0']:g} {ZONE_BUCK['y0']:g})\n\t\t(end {ZONE_BUCK['x1']:g} {ZONE_BUCK['y1']:g})",
        1,
    )
    text = text.replace(
        "(start -40 32)\n\t\t(end 2 104)",
        f"(start {ZONE_PWR_24V['x0']:g} {ZONE_PWR_24V['y0']:g})\n\t\t(end {ZONE_PWR_24V['x1']:g} {ZONE_PWR_24V['y1']:g})",
        1,
    )
    text = text.replace("(start 16 106)\n\t\t(end 78 110)", "(start 12 45.5)\n\t\t(end 68 52.5)", 1)
    text = text.replace(
        "(start 8 92)\n\t\t(end 96 142)",
        f"(start {ZONE_MCU['x0']:g} {ZONE_MCU['y0']:g})\n\t\t(end {ZONE_MCU['x1']:g} {ZONE_MCU['y1']:g})",
        1,
    )
    text = text.replace(
        "(start 10 118)\n\t\t(end 90 145)",
        f"(start {ZONE_BUCK['x0']:g} {ZONE_BUCK['y0']:g})\n\t\t(end {ZONE_BUCK['x1']:g} {ZONE_BUCK['y1']:g})",
        1,
    )
    text = text.replace("(start 7.5 7.5)\n\t\t(end 92.5 142.5)", "(start 4 116)\n\t\t(end 96 272)", 1)
    # Pin names sit just above the plugs (smaller Y).
    text = text.replace('(at 22 133 0)', "(at 22 258 0)", 1)
    text = text.replace('(at 19.5 138 0)', "(at 19.46 260 0)", 1)
    text = text.replace('(at 24.5 138 0)', "(at 24.54 260 0)", 1)
    text = text.replace('(at 86 133 0)', "(at 72 258 0)", 1)
    text = text.replace('(at 80.92 138 0)', "(at 66.92 260 0)", 1)
    text = text.replace('(at 86 138 0)', "(at 72 260 0)", 1)
    text = text.replace('(at 91.08 138 0)', "(at 77.08 260 0)", 1)
    pwr_silk = (
        "\t(gr_rect\n"
        f"\t\t(start {ZONE_PWR_24V['x0']:g} {ZONE_PWR_24V['y0']:g})\n"
        f"\t\t(end {ZONE_PWR_24V['x1']:g} {ZONE_PWR_24V['y1']:g})\n"
        "\t\t(stroke\n"
        "\t\t\t(width 0.15)\n"
        "\t\t\t(type default)\n"
        "\t\t)\n"
        "\t\t(fill no)\n"
        '\t\t(layer "B.SilkS")\n'
        '\t\t(uuid "a24f0001-0000-4000-8000-0000000000c4")\n'
        "\t)\n"
    )
    if "a24f0001-0000-4000-8000-0000000000c4" not in text:
        anchor = text.find('\t(gr_rect\n\t\t(start 0 0)')
        if anchor < 0:
            print("abort: edge-cut rect not found")
            return 1
        text = text[:anchor] + pwr_silk + text[anchor:]
    if text.count("(") != text.count(")"):
        print("abort: parenthesis imbalance")
        return 1
    PCB.write_text(text, encoding="utf-8")
    print(f"moved {moved} footprints; stripped {segs} segments, {vias} vias; outline {BOARD_W:g}x{BOARD_H:g}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
