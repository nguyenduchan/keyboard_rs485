"""Assign pad nets from panel-keyboard.connectivity.json (one board load)."""

from __future__ import annotations

import json
import sys
from pathlib import Path

import pcbnew  # type: ignore

ROOT = Path(__file__).resolve().parents[1]
BOARD = ROOT / "panel-keyboard.kicad_pcb"
CONN = ROOT / "panel-keyboard.connectivity.json"


def ensure_netcode(board: pcbnew.BOARD, net_name: str) -> int:
    try:
        code = board.GetNetcodeFromNetname(net_name)
        if code > 0:
            return code
    except (IndexError, KeyError):
        pass
    ni = pcbnew.NETINFO_ITEM(board, net_name)
    board.Add(ni)
    try:
        return board.GetNetcodeFromNetname(net_name)
    except (IndexError, KeyError):
        return 0


def main() -> int:
    net_map = {
        (r["ref"], r["pin"]): r["net"]
        for r in json.loads(CONN.read_text(encoding="utf-8"))
    }
    board = pcbnew.LoadBoard(str(BOARD))
    n = 0
    for fp in board.GetFootprints():
        ref = fp.GetReference()
        for pad in fp.Pads():
            net_name = net_map.get((ref, str(pad.GetNumber())))
            if not net_name:
                continue
            code = ensure_netcode(board, net_name)
            if code > 0:
                pad.SetNetCode(code)
                n += 1
    pcbnew.SaveBoard(str(BOARD), board)
    print(f"assigned {n} pads")
    return 0


if __name__ == "__main__":
    sys.exit(main())
