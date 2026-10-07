import re
from pathlib import Path

import pcbnew  # type: ignore

ROOT = Path(__file__).resolve().parents[1]
SCH = ROOT / "panel-keyboard.kicad_sch"
BOARD = ROOT / "panel-keyboard.kicad_pcb"
FP_ROOT = Path(
    r"C:\Users\duchan.nguyen\AppData\Local\Programs\KiCad\10.0\share\kicad\footprints"
)
PANEL = ROOT / "footprints" / "Panel.pretty"

text = SCH.read_text(encoding="utf-8")
out: dict[str, str] = {}
for block in re.findall(r"\(symbol\b.*?\n\t\)", text, re.DOTALL):
    ref_m = re.search(r'\(property "Reference" "([^"]+)"', block)
    fp_m = re.search(r'\(property "Footprint" "([^"]+)"', block)
    if ref_m and fp_m and fp_m.group(1):
        out[ref_m.group(1)] = fp_m.group(1)

board = pcbnew.LoadBoard(str(BOARD))
existing = {fp.GetReference() for fp in board.GetFootprints()}
for ref in sorted(out):
    if ref.startswith("#") or ref in existing:
        continue
    fp_id = out[ref]
    lib, name = fp_id.split(":", 1)
    path = str(PANEL if lib == "Panel" else FP_ROOT / f"{lib}.pretty")
    try:
        fp = pcbnew.FootprintLoad(path, name)
        print("OK", ref, fp_id, fp)
    except Exception as e:
        print("FAIL", ref, fp_id, path, e)
        break
