"""Place all BOTTOM_POS in one pcbnew session (faster than per-ref subprocess)."""

from __future__ import annotations

import re
import sys
from pathlib import Path

import pcbnew  # type: ignore

sys.path.insert(0, str(Path(__file__).resolve().parent))
from zone_layout import BOTTOM_POS, ZONE_BUCK, ZONE_MCU, ZONE_PWR_24V

ROOT = Path(__file__).resolve().parents[1]
BOARD = ROOT / "panel-keyboard.kicad_pcb"
SCH = ROOT / "panel-keyboard.kicad_sch"
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


def draw_zone(board: pcbnew.BOARD, z: dict, layer: int) -> None:
    gr = pcbnew.PCB_SHAPE(board)
    gr.SetShape(pcbnew.SHAPE_T_RECT)
    gr.SetStart(pcbnew.VECTOR2I(mm(z["x0"]), mm(z["y0"])))
    gr.SetEnd(pcbnew.VECTOR2I(mm(z["x1"]), mm(z["y1"])))
    gr.SetLayer(layer)
    gr.SetWidth(mm(0.15))
    board.Add(gr)


def place_bottom(board: pcbnew.BOARD, ref: str, x: float, y: float, rot: int) -> None:
    existing = board.FindFootprintByReference(ref)
    if existing:
        try:
            board.Remove(existing)
        except AttributeError:
            pass
    fp_id = sch_fp(ref)
    lib, name = fp_id.split(":", 1)
    path = str(PANEL if lib == "Panel" else FP_KICAD / f"{lib}.pretty")
    fp = pcbnew.FootprintLoad(path, name)
    if fp is None:
        raise RuntimeError(fp_id)
    fp.SetReference(ref)
    fp.SetPosition(pcbnew.VECTOR2I(mm(x), mm(y)))
    fp.SetOrientation(pcbnew.EDA_ANGLE(rot * 10, pcbnew.DEGREES_T))
    if fp.GetLayer() == pcbnew.F_Cu:
        fp.Flip(fp.GetPosition(), False)
    board.Add(fp)


def main() -> int:
    board = pcbnew.LoadBoard(str(BOARD))
    failed: list[str] = []
    for ref, (x, y, rot) in sorted(BOTTOM_POS.items()):
        try:
            place_bottom(board, ref, x, y, rot)
        except Exception as e:
            failed.append(f"{ref}:{e}")
    for z in (ZONE_PWR_24V, ZONE_BUCK, ZONE_MCU):
        draw_zone(board, z, pcbnew.B_SilkS)
    pcbnew.SaveBoard(str(BOARD), board)
    if failed:
        print("FAILED", len(failed))
        for line in failed[:20]:
            print(" ", line)
        return 1
    print(f"OK bottom batch {len(BOTTOM_POS)} refs")
    return 0


if __name__ == "__main__":
    sys.exit(main())
