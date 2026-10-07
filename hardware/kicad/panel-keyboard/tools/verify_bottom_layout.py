"""Check top-side SMD positions: expected coordinates, courtyard gap, U1 present."""

from __future__ import annotations

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from zone_layout import BOTTOM_POS, placement_issues

ROOT = Path(__file__).resolve().parents[1]
PCB = ROOT / "panel-keyboard.kicad_pcb"


def parse_bottom(text: str) -> dict[str, tuple[float, float, str]]:
    out: dict[str, tuple[float, float, str]] = {}
    blocks = re.findall(r"\(footprint\b.*?\n\t\)", text, re.DOTALL)
    for block in blocks:
        ref_m = re.search(r'\(property "Reference" "([^"]+)"', block)
        at_m = re.search(r"\(at\s+([\d.-]+)\s+([\d.-]+)", block)
        layer_m = re.search(r'\(layer "([^"]+)"\)', block)
        if ref_m and at_m and layer_m and ref_m.group(1) in BOTTOM_POS:
            out[ref_m.group(1)] = (
                float(at_m.group(1)),
                float(at_m.group(2)),
                layer_m.group(1),
            )
    return out


def main() -> int:
    if not PCB.is_file():
        print("no pcb")
        return 2
    text = PCB.read_text(encoding="utf-8")
    if re.search(r"\(segment\b", text) or re.search(r"\(via\b", text):
        print("BOTTOM LAYOUT ISSUES:")
        print(" - copper routes still present (segment/via)")
        return 1
    actual = parse_bottom(text)
    errs: list[str] = []
    if "U1" not in actual:
        errs.append("MISSING U1 (STM32) on board")
    for ref, (ex, ey, _) in BOTTOM_POS.items():
        if ref not in actual:
            errs.append(f"MISSING {ref}")
            continue
        ax, ay, layer = actual[ref]
        if layer != "F.Cu":
            errs.append(f"{ref}: chips and SMD belong on F.Cu, got {layer}")
        if abs(ax - ex) > 1.5 or abs(ay - ey) > 1.5:
            errs.append(f"{ref}: expected ({ex},{ey}) got ({ax:.1f},{ay:.1f})")
    errs.extend(placement_issues())
    if errs:
        print("BOTTOM LAYOUT ISSUES:")
        for e in errs[:40]:
            print(" -", e)
        if len(errs) > 40:
            print(f" ... +{len(errs) - 40} more")
        return 1
    print(f"OK bottom layout ({len(BOTTOM_POS)} refs), no routes, U1 @ {actual.get('U1')}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
