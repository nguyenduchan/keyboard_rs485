"""DISABLED: in-place regex splice corrupted panel-keyboard.kicad_pcb.
Do not run. Use layout_bottom_batch.py with KiCad closed.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from zone_layout import BOTTOM_POS

ROOT = Path(__file__).resolve().parents[1]
PCB = ROOT / "panel-keyboard.kicad_pcb"


def main() -> int:
    print("refusing to run: this script corrupts the PCB file")
    return 2
    text = PCB.read_text(encoding="utf-8")
    blocks = list(re.finditer(r"\(footprint\b.*?\n\t\)", text, re.DOTALL))
    changed = 0
    for m in blocks:
        block = m.group(0)
        if '(layer "F.Cu")' in block.split("\n", 2)[1:3]:
            continue
        ref_m = re.search(r'\(property "Reference" "([^"]+)"', block)
        if not ref_m:
            continue
        ref = ref_m.group(1)
        if ref not in BOTTOM_POS:
            continue
        x, y, rot = BOTTOM_POS[ref]
        new_at = f"(at {x:g} {y:g}"
        if rot:
            new_at += f" {rot}"
        new_at += ")"
        new_block, n = re.subn(r"\(at\s+[\d.-]+\s+[\d.-]+(?:\s+[\d.-]+)?\)", new_at, block, count=1)
        if n:
            text = text[: m.start()] + new_block + text[m.end() :]
            changed += 1
    PCB.write_text(text, encoding="utf-8")
    print(f"relocated {changed} bottom footprints")
    return 0 if changed else 1


if __name__ == "__main__":
    sys.exit(main())
