"""Front THT + bottom SMD zones (see zone_layout.py)."""

from __future__ import annotations

import sys
from pathlib import Path

import pcbnew  # type: ignore

sys.path.insert(0, str(Path(__file__).resolve().parent))
from zone_layout import (
    BOTTOM_POS,
    FRONT_POS,
    GAP_Y_MAX,
    GAP_Y_MIN,
    ZONE_BUCK,
    ZONE_MCU,
    ZONE_PWR_24V,
)

ROOT = Path(__file__).resolve().parents[1]
BOARD = ROOT / "panel-keyboard.kicad_pcb"
FP_PANEL = ROOT / "footprints" / "Panel.pretty"
FP_KICAD = Path(
    r"C:\Users\duchan.nguyen\AppData\Local\Programs\KiCad\10.0\share\kicad\footprints"
)


def mm(v: float) -> int:
    return pcbnew.FromMM(v)


def load_fp(lib: str, name: str) -> pcbnew.FOOTPRINT:
    path = str(FP_PANEL if lib == "Panel" else FP_KICAD / f"{lib}.pretty")
    fp = pcbnew.FootprintLoad(path, name)
    if fp is None:
        raise RuntimeError(f"{lib}:{name}")
    return fp


def on_bottom(fp: pcbnew.FOOTPRINT) -> None:
    if fp.GetLayer() == pcbnew.F_Cu:
        fp.Flip(fp.GetPosition(), False)


def on_front(fp: pcbnew.FOOTPRINT) -> None:
    if fp.GetLayer() != pcbnew.F_Cu:
        fp.Flip(fp.GetPosition(), False)


def draw_zone(board: pcbnew.BOARD, z: dict, layer: int) -> None:
    gr = pcbnew.PCB_SHAPE(board)
    gr.SetShape(pcbnew.SHAPE_T_RECT)
    gr.SetStart(pcbnew.VECTOR2I(mm(z["x0"]), mm(z["y0"])))
    gr.SetEnd(pcbnew.VECTOR2I(mm(z["x1"]), mm(z["y1"])))
    gr.SetLayer(layer)
    gr.SetWidth(mm(0.15))
    board.Add(gr)


def place_ref(board: pcbnew.BOARD, ref: str, x: float, y: float, rot: int, front: bool) -> None:
    found = next((fp for fp in board.GetFootprints() if fp_ref(fp) == ref), None)
    if found:
        fp = found
    else:
        return
    fp.SetPosition(pcbnew.VECTOR2I(mm(x), mm(y)))
    fp.SetOrientation(pcbnew.EDA_ANGLE(rot * 10, pcbnew.DEGREES_T))
    if front:
        on_front(fp)
    else:
        on_bottom(fp)


def fp_ref(fp: pcbnew.FOOTPRINT) -> str | None:
    try:
        return fp.GetReference()
    except AttributeError:
        return None


def fp_item_name(fp: pcbnew.FOOTPRINT) -> str:
    try:
        return fp.GetFPID().GetLibItemName()
    except AttributeError:
        return ""


def add_front(board: pcbnew.BOARD, ref: str, fp_name: str, x: float, y: float, rot: int = 0) -> None:
    existing = next((fp for fp in board.GetFootprints() if fp_ref(fp) == ref), None)
    if existing:
        try:
            board.Remove(existing)
        except AttributeError:
            pass
    fp = load_fp("Panel", fp_name)
    fp.SetReference(ref)
    fp.SetPosition(pcbnew.VECTOR2I(mm(x), mm(y)))
    fp.SetOrientation(pcbnew.EDA_ANGLE(rot * 10, pcbnew.DEGREES_T))
    on_front(fp)
    board.Add(fp)


def main() -> int:
    board = pcbnew.LoadBoard(str(BOARD))

    for ref, (x, y, rot) in FRONT_POS.items():
        if ref.startswith("SW"):
            fp_name = "SW_Tactile_12mm_THT"
        elif ref.startswith("D1") and len(ref) == 3 and ref[2].isdigit():
            fp_name = "LED_3mm_THT"
        elif ref.startswith("DS"):
            fp_name = "LED7seg_1digit_0.56in_CC_THT"
        else:
            continue
        found = next((fp for fp in board.GetFootprints() if fp_ref(fp) == ref), None)
        if found and fp_item_name(found) == fp_name:
            place_ref(board, ref, x, y, rot, front=True)
        else:
            add_front(board, ref, fp_name, x, y, rot)

    for ref, (x, y, rot) in BOTTOM_POS.items():
        place_ref(board, ref, x, y, rot, front=False)

    for layer in (pcbnew.B_SilkS,):
        draw_zone(board, ZONE_PWR_24V, layer)
        draw_zone(board, ZONE_BUCK, layer)
        draw_zone(board, ZONE_MCU, layer)
        gap = pcbnew.PCB_SHAPE(board)
        gap.SetShape(pcbnew.SHAPE_T_RECT)
        gap.SetStart(pcbnew.VECTOR2I(mm(10), mm(GAP_Y_MIN)))
        gap.SetEnd(pcbnew.VECTOR2I(mm(94), mm(GAP_Y_MAX)))
        gap.SetLayer(pcbnew.Eco1_User)
        gap.SetWidth(mm(0.12))
        board.Add(gap)

    pcbnew.SaveBoard(str(BOARD), board)
    print(
        "Front: THT SW/LED/DS | Bottom: 3 zones "
        f"(PWR {ZONE_PWR_24V['y0']}-{ZONE_PWR_24V['y1']}, "
        f"BUCK {ZONE_BUCK['y0']}-{ZONE_BUCK['y1']}, "
        f"MCU {ZONE_MCU['y0']}+, gap Y={GAP_Y_MIN}-{GAP_Y_MAX})"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
