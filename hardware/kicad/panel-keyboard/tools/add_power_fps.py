"""Add missing power footprints one at a time (workaround pcbnew batch crash)."""

from __future__ import annotations

import re
import sys
from pathlib import Path

import pcbnew  # type: ignore

ROOT = Path(__file__).resolve().parents[1]
BOARD = ROOT / "panel-keyboard.kicad_pcb"
SCH = ROOT / "panel-keyboard.kicad_sch"
FP_ROOT = Path(
    r"C:\Users\duchan.nguyen\AppData\Local\Programs\KiCad\10.0\share\kicad\footprints"
)
PANEL = ROOT / "footprints" / "Panel.pretty"

TO_ADD = {
    "C14": (46, 18, 0),
    "C16": (58, 12, 0),
    "C17": (58, 18, 0),
    "C2": (40, 128, 0),
    "C3": (12, 128, 0),
    "C18": (12, 134, 0),
    "C4": (46, 12, 0),
    "C5": (46, 128, 0),
    "C7": (18, 132, 0),
    "D8": (28, 128, 90),
    "NT1": (52, 15, 0),
    "Q17": (40, 12, 0),
    "R17": (40, 18, 0),
    "R36": (48, 136, 0),
    "R37": (48, 140, 0),
    "R38": (54, 136, 0),
}


def sch_fps() -> dict[str, str]:
    text = SCH.read_text(encoding="utf-8")
    out: dict[str, str] = {}
    for block in re.findall(r"\(symbol\b.*?\n\t\)", text, re.DOTALL):
        ref_m = re.search(r'\(property "Reference" "([^"]+)"', block)
        fp_m = re.search(r'\(property "Footprint" "([^"]+)"', block)
        if ref_m and fp_m and fp_m.group(1):
            out[ref_m.group(1)] = fp_m.group(1)
    return out


def mm(v: float) -> int:
    return pcbnew.FromMM(v)


def main() -> int:
    fps = sch_fps()
    board = pcbnew.LoadBoard(str(BOARD))
    existing = {fp.GetReference() for fp in board.GetFootprints()}
    for ref, (x, y, rot) in TO_ADD.items():
        if ref in existing:
            print("skip", ref)
            continue
        fp_id = fps[ref]
        lib, name = fp_id.split(":", 1)
        path = str(PANEL if lib == "Panel" else FP_ROOT / f"{lib}.pretty")
        fp = pcbnew.FootprintLoad(path, name)
        if fp is None:
            print("fail load", ref)
            return 1
        fp.SetReference(ref)
        fp.SetPosition(pcbnew.VECTOR2I(mm(x), mm(y)))
        fp.SetOrientation(pcbnew.EDA_ANGLE(rot * 10, pcbnew.DEGREES_T))
        if fp.GetLayer() == pcbnew.F_Cu:
            fp.Flip(fp.GetPosition(), False)
        board.Add(fp)
        pcbnew.SaveBoard(str(BOARD), board)
        print("added", ref)
    return 0


if __name__ == "__main__":
    sys.exit(main())
