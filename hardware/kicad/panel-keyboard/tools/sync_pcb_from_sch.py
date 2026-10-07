"""Legacy PCB sync — prefer tools/update_pcb_all.py (stable on KiCad 10 Windows)."""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

import pcbnew  # type: ignore

ROOT = Path(__file__).resolve().parents[1]
BOARD_PATH = ROOT / "panel-keyboard.kicad_pcb"
SCH = ROOT / "panel-keyboard.kicad_sch"
CONN = ROOT / "panel-keyboard.connectivity.json"

FP_ROOT = Path(
    r"C:\Users\duchan.nguyen\AppData\Local\Programs\KiCad\10.0\share\kicad\footprints"
)
PANEL_FP = ROOT / "footprints" / "Panel.pretty"

KEEP_REFS = frozenset({f"MH{i}" for i in range(1, 5)})

PLACEMENT: dict[str, tuple[float, float, int, str]] = {}


def init_placement() -> None:
    bottom = {
        "J_PWR": (19, 8, 0),
        "J485": (80, 8, 0),
        "F1": (28, 12, 0),
        "C15": (34, 12, 0),
        "TVS1": (34, 18, 90),
        "Q12": (40, 12, 0),
        "R17": (40, 18, 0),
        "C4": (46, 12, 0),
        "C14": (46, 18, 0),
        "NT1": (52, 15, 0),
        "C16": (58, 12, 0),
        "C17": (58, 18, 0),
        "U4": (22, 132, 0),
        "D8": (28, 128, 90),
        "L1": (34, 132, 90),
        "C2": (40, 128, 0),
        "C5": (46, 128, 0),
        "R36": (48, 136, 0),
        "R37": (48, 140, 0),
        "R38": (54, 136, 0),
        "FB5V": (62, 128, 0),
        "U5": (15, 125, 0),
        "C7": (18, 132, 0),
        "C3": (12, 128, 0),
        "C18": (12, 134, 0),
        "FB33": (72, 125, 0),
        "CVDDA": (96, 125, 0),
        "U1": (28, 124, 0),
        "CVDD1": (10, 115, 0),
        "RNST": (22, 115, 0),
        "CNRST": (28, 115, 0),
        "RBOT0": (34, 115, 0),
        "J2": (8, 118, 90),
        "U3": (58, 135, 0),
        "R47A": (68, 132, 0),
        "R47B": (68, 138, 0),
        "RDE": (74, 132, 0),
        "CM3": (74, 138, 0),
        "L485": (80, 135, 0),
        "RBIAS_A": (86, 138, 0),
        "RBIAS_B": (86, 132, 0),
        "RTERM": (92, 135, 0),
        "JTERM": (96, 135, 0),
        "TVS2": (88, 10, 90),
        "U2": (40, 125, 0),
        "R33A": (35, 120, 0),
        "R33B": (40, 120, 0),
        "R33C": (45, 120, 0),
        "RPU": (50, 120, 0),
        "CTM1": (55, 120, 0),
        "CTM2": (60, 120, 0),
        "DISP1": (10, 110, 0),
        "DISP2": (10, 82, 0),
    }
    for ref, (x, y, r) in bottom.items():
        layer = "F.Cu" if ref in {"DISP1", "DISP2"} else "B.Cu"
        PLACEMENT[ref] = (x, y, r, layer)
    for i in range(1, 17):
        row, col = (i - 1) // 4, (i - 1) % 4
        PLACEMENT[f"SW{i}"] = (12 + col * 16, 16 + row * 16, 0, "B.Cu")
    led_y = [40 + (4.5 - n) * 9 for n in range(1, 9)]
    for i, y in enumerate(led_y):
        PLACEMENT[f"D{9 + i}"] = (88, y, 0, "F.Cu")
        PLACEMENT[f"RLED{i + 1}"] = (85, y, 0, "B.Cu")


def parse_sch_footprints() -> dict[str, str]:
    text = SCH.read_text(encoding="utf-8")
    out: dict[str, str] = {}
    for block in re.findall(r"\(symbol\b.*?\n\t\)", text, re.DOTALL):
        ref_m = re.search(r'\(property "Reference" "([^"]+)"', block)
        fp_m = re.search(r'\(property "Footprint" "([^"]+)"', block)
        if ref_m and fp_m and fp_m.group(1):
            out[ref_m.group(1)] = fp_m.group(1)
    return out


def parse_connectivity() -> dict[tuple[str, str], str]:
    if not CONN.is_file():
        raise SystemExit(f"Missing {CONN}; run tools/build_schematic.py first")
    rows = json.loads(CONN.read_text(encoding="utf-8"))
    return {(r["ref"], r["pin"]): r["net"] for r in rows}


def fp_lib_path(fp_id: str) -> Path:
    lib, _ = fp_id.split(":", 1)
    if lib == "Panel":
        return PANEL_FP
    return FP_ROOT / f"{lib}.pretty"


def load_footprint(fp_id: str) -> pcbnew.FOOTPRINT:
    path = str(fp_lib_path(fp_id))
    lib, name = fp_id.split(":", 1)
    fp = pcbnew.FootprintLoad(path, name)
    if fp is None:
        raise RuntimeError(f"Cannot load footprint {fp_id} from {path}")
    return fp


def mm(v: float) -> int:
    return pcbnew.FromMM(v)


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


def place_fp(fp: pcbnew.FOOTPRINT, x: float, y: float, rot: int, layer: str) -> None:
    fp.SetPosition(pcbnew.VECTOR2I(mm(x), mm(y)))
    fp.SetOrientation(pcbnew.EDA_ANGLE(rot * 10, pcbnew.DEGREES_T))
    on_front = layer == "F.Cu"
    if on_front and fp.GetLayer() != pcbnew.F_Cu:
        fp.Flip(fp.GetPosition(), False)
    elif not on_front and fp.GetLayer() == pcbnew.F_Cu:
        fp.Flip(fp.GetPosition(), False)


def main() -> int:
    init_placement()
    sch_fp = parse_sch_footprints()
    sch_refs = {r for r in sch_fp if not r.startswith("#PWR")}
    net_map = parse_connectivity()

    board = pcbnew.LoadBoard(str(BOARD_PATH))
    by_ref: dict[str, pcbnew.FOOTPRINT] = {
        fp.GetReference(): fp for fp in board.GetFootprints()
    }

    added = 0
    moved = 0
    for ref in sorted(sch_refs):
        if ref.startswith("#"):
            continue
        fp_id = sch_fp[ref]
        if ref not in PLACEMENT:
            print("WARN no placement for", ref)
            continue
        x, y, rot, layer = PLACEMENT[ref]
        if ref in by_ref:
            moved += 1
            continue
        else:
            try:
                fp = load_footprint(fp_id)
            except RuntimeError as e:
                print("SKIP add", ref, e)
                continue
            fp.SetReference(ref)
            place_fp(fp, x, y, rot, layer)
            board.Add(fp)
            by_ref[ref] = fp
            added += 1

    removed = 0

    pcbnew.SaveBoard(str(BOARD_PATH), board)
    print(f"Removed {removed}, added {added}, moved {moved}; saved {BOARD_PATH.name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
