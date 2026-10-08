"""Un-mirror texts of top-side footprints and move stray back-layer texts to the front (KiCad python)."""
from pathlib import Path

import pcbnew  # type: ignore

BOARD = Path(__file__).resolve().parents[1] / "panel-keyboard.kicad_pcb"
SWAP = {pcbnew.B_SilkS: pcbnew.F_SilkS, pcbnew.B_Fab: pcbnew.F_Fab}

board = pcbnew.LoadBoard(str(BOARD))
n = 0
for fp in board.GetFootprints():
    if fp.GetLayer() != pcbnew.F_Cu:
        continue
    items = list(fp.GetFields()) + [g for g in fp.GraphicalItems() if isinstance(g, pcbnew.PCB_TEXT)]
    for t in items:
        if t.GetLayer() in SWAP:
            t.SetLayer(SWAP[t.GetLayer()])
            n += 1
        if t.IsMirrored():
            t.SetMirrored(False)
            n += 1
pcbnew.SaveBoard(str(BOARD), board)
print("fixed", n, "text items")
