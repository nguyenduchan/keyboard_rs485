"""Write an autorouter result (pathfinder.exe) into panel-keyboard.kicad_pcb using KiCad's API.

usage (KiCad python):  python.exe tools/write_tracks.py problem.txt result.txt
Existing tracks and vias are replaced. Close the board in KiCad first.
"""

from __future__ import annotations

import sys
from pathlib import Path

import pcbnew  # type: ignore

sys.path.insert(0, str(Path(__file__).resolve().parent))
from route_apply import dedupe, read_problem_nets, read_result, split_tjunctions  # noqa: E402
from route_export import VIA_D, VIA_DRILL  # noqa: E402

BOARD = Path(__file__).resolve().parents[1] / "panel-keyboard.kicad_pcb"


def mm(v: float) -> int:
    return pcbnew.FromMM(v)


def main() -> int:
    netname = read_problem_nets(sys.argv[1])
    segs, vias, uns = read_result(sys.argv[2])
    segs = split_tjunctions(dedupe(segs))
    board = pcbnew.LoadBoard(str(BOARD))
    for t in list(board.GetTracks()):
        board.Remove(t)
    nets = {}

    def net(n: int):
        if n not in nets:
            nets[n] = board.FindNet(netname[n])
        return nets[n]

    for n, l, x1, y1, x2, y2, w in segs:
        t = pcbnew.PCB_TRACK(board)
        t.SetStart(pcbnew.VECTOR2I(mm(x1), mm(y1)))
        t.SetEnd(pcbnew.VECTOR2I(mm(x2), mm(y2)))
        t.SetWidth(mm(w))
        t.SetLayer(pcbnew.F_Cu if l == 0 else pcbnew.B_Cu)
        t.SetNet(net(n))
        board.Add(t)
    for n, x, y in vias:
        v = pcbnew.PCB_VIA(board)
        v.SetPosition(pcbnew.VECTOR2I(mm(x), mm(y)))
        v.SetWidth(mm(VIA_D))
        v.SetDrill(mm(VIA_DRILL))
        v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
        v.SetNet(net(n))
        board.Add(v)
    pcbnew.SaveBoard(str(BOARD), board)
    print(f"wrote {len(segs)} tracks, {len(vias)} vias; unrouted: {len(uns)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
