"""Sync panel-keyboard.kicad_pcb with the schematic generator output (KiCad 10 python).

  python.exe tools/sync_pcb_from_conn.py remove   # drop footprints that are gone or whose footprint changed
  python.exe tools/sync_pcb_from_conn.py add      # add missing footprints (zone_layout positions) + assign all nets

The two steps run in separate processes: in KiCad 10 the SWIG wrappers break after board.Remove().
Front parts (FRONT_POS) go on B.Cu, everything else on F.Cu. Tracks are removed in the "remove" step.
Close the board in KiCad first.
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

import pcbnew  # type: ignore

sys.path.insert(0, str(Path(__file__).resolve().parent))
from zone_layout import BOTTOM_POS, FRONT_POS  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
BOARD = ROOT / "panel-keyboard.kicad_pcb"
SCH = ROOT / "panel-keyboard.kicad_sch"
CONN = ROOT / "panel-keyboard.connectivity.json"
FP_ROOT = Path(r"C:\Users\duchan.nguyen\AppData\Local\Programs\KiCad\10.0\share\kicad\footprints")
PANEL = ROOT / "footprints" / "Panel.pretty"
KEEP = {"MH1", "MH2", "MH3", "MH4"}


def sch_parts() -> dict[str, tuple[str, str]]:
    """ref -> (footprint lib id, value)"""
    text = SCH.read_text(encoding="utf-8")
    out = {}
    for block in re.findall(r"\(symbol\b.*?\n\t\)", text, re.DOTALL):
        r = re.search(r'\(property "Reference" "([^"]+)"', block)
        f = re.search(r'\(property "Footprint" "([^"]*)"', block)
        v = re.search(r'\(property "Value" "([^"]*)"', block)
        if r and f and f.group(1) and not r.group(1).startswith("#"):
            out[r.group(1)] = (f.group(1), v.group(1) if v else "")
    return out


def remove() -> int:
    parts = sch_parts()
    board = pcbnew.LoadBoard(str(BOARD))
    for t in list(board.GetTracks()):
        board.Remove(t)
    gone = []
    for fp in list(board.GetFootprints()):
        ref = fp.GetReference()
        if ref in KEEP:
            continue
        want = parts.get(ref)
        have = str(fp.GetFPID().GetLibItemName())
        if want is None or want[0].split(":", 1)[1] != have:
            gone.append(f"{ref}({have})")
            board.Remove(fp)
    pcbnew.SaveBoard(str(BOARD), board)
    print("removed", len(gone), ":", " ".join(gone))
    return 0


def add() -> int:
    parts = sch_parts()
    net_map = {(r["ref"], r["pin"]): r["net"] for r in json.loads(CONN.read_text(encoding="utf-8"))}
    board = pcbnew.LoadBoard(str(BOARD))
    present = {fp.GetReference() for fp in board.GetFootprints()}
    missing = [r for r in sorted(parts) if r not in present]
    loaded = {}
    for ref in missing:
        lib, name = parts[ref][0].split(":", 1)
        path = str(PANEL if lib == "Panel" else FP_ROOT / f"{lib}.pretty")
        loaded[ref] = pcbnew.FootprintLoad(path, name)
        if loaded[ref] is None:
            print("cannot load", parts[ref][0])
            return 1
    for ref in missing:
        fp = loaded[ref]
        front = ref in FRONT_POS
        x, y, rot = (FRONT_POS if front else BOTTOM_POS)[ref]
        fp.SetReference(ref)
        fp.SetValue(parts[ref][1])
        board.Add(fp)
        fp.SetPosition(pcbnew.VECTOR2I(pcbnew.FromMM(x), pcbnew.FromMM(y)))
        if front:
            fp.Flip(fp.GetPosition(), pcbnew.FLIP_DIRECTION_LEFT_RIGHT)
        fp.SetOrientation(pcbnew.EDA_ANGLE(float(rot), pcbnew.DEGREES_T))
        print("added", ref, parts[ref][0], (x, y, rot), "B" if front else "F")
    # positions of parts that stayed (zone_layout is the source of truth)
    for fp in board.GetFootprints():
        ref = fp.GetReference()
        if ref in missing or ref in KEEP:
            continue
        pos = FRONT_POS.get(ref) or BOTTOM_POS.get(ref)
        if pos:
            fp.SetPosition(pcbnew.VECTOR2I(pcbnew.FromMM(pos[0]), pcbnew.FromMM(pos[1])))
            fp.SetOrientation(pcbnew.EDA_ANGLE(float(pos[2]), pcbnew.DEGREES_T))
        if ref in parts:
            fp.SetValue(parts[ref][1])
    # nets: tact switch pads 3/4 are the internal twins of 2/1
    nets = {}
    n_set = 0
    for fp in board.GetFootprints():
        ref = fp.GetReference()
        for pad in fp.Pads():
            num = str(pad.GetNumber())
            key = (ref, num)
            if ref.startswith("SW") and num in ("3", "4"):
                key = (ref, {"3": "2", "4": "1"}[num])
            if ref.startswith("MH"):
                key = ("MH", "")
            name = None if ref.startswith("MH") else net_map.get(key)  # M3 holes isolated from logic GND
            if not name:
                pad.SetNetCode(0)
                continue
            if name not in nets:
                ni = board.FindNet(name)
                if ni is None:
                    ni = pcbnew.NETINFO_ITEM(board, name)
                    board.Add(ni)
                nets[name] = ni
            pad.SetNet(nets[name])
            n_set += 1
    pcbnew.SaveBoard(str(BOARD), board)
    print("nets assigned to", n_set, "pads")
    return 0


if __name__ == "__main__":
    sys.exit(remove() if sys.argv[1:] == ["remove"] else add())
