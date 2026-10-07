"""Check B.Cu positions: zones, no overlap, U1 present."""

from __future__ import annotations

import math
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from zone_layout import (
    BOTTOM_POS,
    FRONT_KEEPOUT_X0,
    FRONT_KEEPOUT_X1,
    FRONT_KEEPOUT_Y0,
    FRONT_KEEPOUT_Y1,
    ZONE_BUCK,
    ZONE_MCU,
    ZONE_PWR_24V,
)

ROOT = Path(__file__).resolve().parents[1]
PCB = ROOT / "panel-keyboard.kicad_pcb"
R_MIN = 1.6


def parse_bottom(text: str) -> dict[str, tuple[float, float, str]]:
    out: dict[str, tuple[float, float, str]] = {}
    blocks = re.findall(r"\(footprint\b.*?\n\t\)", text, re.DOTALL)
    for block in blocks:
        if '(layer "F.Cu")' in block.split("\n")[1:3]:
            continue
        ref_m = re.search(r'\(property "Reference" "([^"]+)"', block)
        at_m = re.search(r"\(at\s+([\d.-]+)\s+([\d.-]+)", block)
        layer = "B.Cu" if '(layer "B.Cu")' in block[:400] else "?"
        if ref_m and at_m:
            out[ref_m.group(1)] = (float(at_m.group(1)), float(at_m.group(2)), layer)
    return out


def in_zone(x: float, y: float, z: dict) -> bool:
    return z["x0"] <= x <= z["x1"] and z["y0"] <= y <= z["y1"]


def zone_for_ref(ref: str) -> dict | None:
    if ref in ("J_PWR", "J485", "TVS2") or ref.startswith("RLED"):
        return None
    if ref in ("F1", "L2", "RV1", "C19", "C15", "TVS1", "Q12", "R17", "C4", "C14", "NT1"):
        return ZONE_PWR_24V
    if ref in ("U4", "C16", "C17", "D8", "L1", "C2", "C5", "R36", "R37", "R38", "FB2", "C22", "FB5V"):
        return ZONE_BUCK
    return ZONE_MCU


def main() -> int:
    if not PCB.is_file():
        print("no pcb")
        return 2
    actual = parse_bottom(PCB.read_text(encoding="utf-8"))
    errs: list[str] = []
    if "U1" not in actual:
        errs.append("MISSING U1 (STM32) on board")
    for ref, (ex, ey, _) in BOTTOM_POS.items():
        if ref not in actual:
            errs.append(f"MISSING {ref}")
            continue
        ax, ay, _ = actual[ref]
        if abs(ax - ex) > 1.5 or abs(ay - ey) > 1.5:
            errs.append(f"{ref}: expected ({ex},{ey}) got ({ax:.1f},{ay:.1f})")
        z = zone_for_ref(ref)
        if z and not in_zone(ax, ay, z):
            errs.append(f"{ref} outside zone {z['label']}")
        if (
            not ref.startswith("RLED")
            and FRONT_KEEPOUT_X0 <= ax <= FRONT_KEEPOUT_X1
            and FRONT_KEEPOUT_Y0 <= ay <= FRONT_KEEPOUT_Y1
        ):
            errs.append(f"{ref} in front keepout ({ax:.1f},{ay:.1f})")

    refs = [r for r in BOTTOM_POS if r in actual]
    for i, a in enumerate(refs):
        ax, ay, _ = actual[a]
        for b in refs[i + 1 :]:
            bx, by, _ = actual[b]
            if math.hypot(ax - bx, ay - by) < R_MIN:
                errs.append(f"OVERLAP {a} & {b}")

    if errs:
        print("BOTTOM LAYOUT ISSUES:")
        for e in errs[:40]:
            print(" -", e)
        if len(errs) > 40:
            print(f" ... +{len(errs) - 40} more")
        return 1
    print(f"OK bottom layout ({len(refs)} refs), U1 @ {actual.get('U1')}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
