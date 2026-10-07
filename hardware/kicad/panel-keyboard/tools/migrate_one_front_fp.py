"""Replace one front footprint (fresh pcbnew process). Usage: migrate_one_front_fp.py REF"""

from __future__ import annotations

import sys
from pathlib import Path

import pcbnew  # type: ignore

sys.path.insert(0, str(Path(__file__).resolve().parent))
from zone_layout import FRONT_POS

ROOT = Path(__file__).resolve().parents[1]
BOARD = ROOT / "panel-keyboard.kicad_pcb"
FP_PANEL = ROOT / "footprints" / "Panel.pretty"


def mm(v: float) -> int:
    return pcbnew.FromMM(v)


def fp_name_for(ref: str) -> str:
    if ref.startswith("SW"):
        return "SW_Tactile_12mm_THT"
    if ref.startswith("DS"):
        return "LED7seg_1digit_0.56in_CC_THT"
    if ref.startswith("D1") and len(ref) == 3 and ref[2].isdigit():
        return "LED_3mm_THT"
    raise KeyError(ref)


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: migrate_one_front_fp.py REF")
        return 2
    ref = sys.argv[1]
    if ref not in FRONT_POS:
        print("not a front ref:", ref)
        return 2
    x, y, rot = FRONT_POS[ref]
    fp_name = fp_name_for(ref)
    board = pcbnew.LoadBoard(str(BOARD))
    existing = board.FindFootprintByReference(ref)
    if existing:
        try:
            if existing.GetFPID().GetLibItemName() == fp_name:
                existing.SetPosition(pcbnew.VECTOR2I(mm(x), mm(y)))
                existing.SetOrientation(pcbnew.EDA_ANGLE(rot * 10, pcbnew.DEGREES_T))
                if existing.GetLayer() != pcbnew.F_Cu:
                    existing.Flip(existing.GetPosition(), False)
                pcbnew.SaveBoard(str(BOARD), board)
                print("ok", ref, "already", fp_name)
                return 0
        except AttributeError:
            pass
        try:
            board.Remove(existing)
        except AttributeError:
            pass
    fp = pcbnew.FootprintLoad(str(FP_PANEL), fp_name)
    if fp is None:
        print("load failed", fp_name)
        return 1
    fp.SetReference(ref)
    fp.SetPosition(pcbnew.VECTOR2I(mm(x), mm(y)))
    fp.SetOrientation(pcbnew.EDA_ANGLE(rot * 10, pcbnew.DEGREES_T))
    if fp.GetLayer() != pcbnew.F_Cu:
        fp.Flip(fp.GetPosition(), False)
    board.Add(fp)
    pcbnew.SaveBoard(str(BOARD), board)
    print("ok", ref, fp_name)
    return 0


if __name__ == "__main__":
    sys.exit(main())
