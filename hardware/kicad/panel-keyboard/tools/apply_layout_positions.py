"""Move existing footprints to FRONT_POS / BOTTOM_POS without splicing corruption.

Replacements are applied from the end of the file backward.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from zone_layout import BOTTOM_POS, FRONT_POS

ROOT = Path(__file__).resolve().parents[1]
PCB = ROOT / "panel-keyboard.kicad_pcb"
POS = {**FRONT_POS, **BOTTOM_POS}


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
    if text.count("(") != text.count(")"):
        print("abort: parenthesis imbalance")
        return 1
    PCB.write_text(text, encoding="utf-8")
    print(f"moved {moved} footprints")
    return 0


if __name__ == "__main__":
    sys.exit(main())
