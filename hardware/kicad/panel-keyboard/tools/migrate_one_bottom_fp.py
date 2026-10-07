"""Place one B.Cu footprint (fresh pcbnew). Usage: migrate_one_bottom_fp.py REF"""

from __future__ import annotations

import re
import sys
from pathlib import Path

import pcbnew  # type: ignore

sys.path.insert(0, str(Path(__file__).resolve().parent))
from zone_layout import BOTTOM_POS, FRONT_POS

ROOT = Path(__file__).resolve().parents[1]
BOARD = ROOT / "panel-keyboard.kicad_pcb"
SCH = ROOT / "panel-keyboard.kicad_sch"
FP_ROOT = Path(
    r"C:\Users\duchan.nguyen\AppData\Local\Programs\KiCad\10.0\bin\..\share\kicad\footprints"
)
# KiCad 10 footprints path
FP_KICAD = Path(
    r"C:\Users\duchan.nguyen\AppData\Local\Programs\KiCad\10.0\share\kicad\footprints"
)
PANEL = ROOT / "footprints" / "Panel.pretty"


def mm(v: float) -> int:
    return pcbnew.FromMM(v)


def sch_fp(ref: str) -> str:
    text = SCH.read_text(encoding="utf-8")
    for block in re.findall(r"\(symbol\b.*?\n\t\)", text, re.DOTALL):
        ref_m = re.search(r'\(property "Reference" "([^"]+)"', block)
        if ref_m and ref_m.group(1) == ref:
            fp_m = re.search(r'\(property "Footprint" "([^"]+)"', block)
            if fp_m and fp_m.group(1):
                return fp_m.group(1)
    raise KeyError(ref)


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: migrate_one_bottom_fp.py REF")
        return 2
    ref = sys.argv[1]
    if ref not in BOTTOM_POS:
        print("not in BOTTOM_POS:", ref)
        return 2
    x, y, rot = BOTTOM_POS[ref]
    fp_id = sch_fp(ref)
    lib, name = fp_id.split(":", 1)
    path = str(PANEL if lib == "Panel" else FP_KICAD / f"{lib}.pretty")
    board = pcbnew.LoadBoard(str(BOARD))
    existing = board.FindFootprintByReference(ref)
    if existing:
        try:
            board.Remove(existing)
        except AttributeError:
            pass
    fp = pcbnew.FootprintLoad(path, name)
    if fp is None:
        print("load failed", fp_id)
        return 1
    fp.SetReference(ref)
    fp.SetPosition(pcbnew.VECTOR2I(mm(x), mm(y)))
    fp.SetOrientation(pcbnew.EDA_ANGLE(rot * 10, pcbnew.DEGREES_T))
    if fp.GetLayer() == pcbnew.F_Cu:
        fp.Flip(fp.GetPosition(), False)
    board.Add(fp)
    pcbnew.SaveBoard(str(BOARD), board)
    print("ok", ref, f"({x},{y})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
