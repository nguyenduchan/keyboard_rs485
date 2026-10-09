"""Đặt lại footprint theo BOTTOM_POS / FRONT_POS, bỏ toàn bộ dây (segment/via) và vẽ khung nhóm GROUPS.

Chạy: python tools/place_groups.py   (không cần KiCad; sau đó đi dây bằng route_export + pathfinder + write_tracks)
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from zone_layout import BOTTOM_POS, FRONT_POS, GROUPS

PCB = Path(__file__).resolve().parents[1] / "panel-keyboard.kicad_pcb"
POS = {**FRONT_POS, **BOTTOM_POS}
NL = "\n"
T = "\t"


def spans(text: str) -> list[tuple[int, int]]:
    out, i = [], 0
    while (j := text.find("(footprint ", i)) >= 0:
        depth = 0
        for k in range(j, len(text)):
            depth += (text[k] == "(") - (text[k] == ")")
            if depth == 0:
                out.append((j, k + 1))
                i = k + 1
                break
    return out


def strip_graphics(text: str, labels: set[str]) -> str:
    """Xóa gr_rect / gr_poly (F.SilkS, Eco1.User) và gr_text nhãn nhóm ở cấp trên cùng của file."""
    out, i = [], 0
    while True:
        j = text.find(NL + T + "(gr_", i)
        if j < 0:
            out.append(text[i:])
            break
        k, depth = j + 2, 0
        for k in range(j + 2, len(text)):
            depth += (text[k] == "(") - (text[k] == ")")
            if depth == 0:
                break
        blk = text[j : k + 1]
        first = blk.split(NL)[1]
        shape = first.startswith((T + "(gr_rect", T + "(gr_poly"))
        drop = (shape and ('(layer "F.SilkS")' in blk or '(layer "Eco1.User")' in blk)) or (
            first.startswith(T + "(gr_text") and any(f'"{lb}"' in first for lb in labels)
        )
        out.append(text[i:j])
        if not drop:
            out.append(blk)
        i = k + 1
    return "".join(out)


def lines(*rows: str) -> str:
    return NL.join(rows) + NL


def frame(i: int, g: tuple) -> str:
    """gr_rect hoặc gr_poly + gr_text nhãn cho một nhóm."""
    stroke = lines(T * 2 + "(stroke", T * 3 + "(width 0.15)", T * 3 + "(type default)", T * 2 + ")")
    tail = lines(
        T * 2 + "(fill no)",
        T * 2 + '(layer "F.SilkS")',
        T * 2 + f'(uuid "grp00000-0000-4000-8000-00000000010{i}")',
        T + ")",
    )
    label = g[-1]
    if g[0] == "poly":
        pts = g[1]
        xy = "".join(NL + T * 3 + f"(xy {x:g} {y:g})" for x, y in pts)
        shape = T + "(gr_poly" + NL + T * 2 + "(pts" + xy + NL + T * 2 + ")" + NL + stroke + tail
        x0, y0 = pts[0]
    else:
        x0, y0, x1, y1, _ = g
        shape = lines(T + "(gr_rect", T * 2 + f"(start {x0:g} {y0:g})", T * 2 + f"(end {x1:g} {y1:g})") + stroke + tail
    text = lines(
        T + f'(gr_text "{label}"',
        T * 2 + f"(at {x0 + 1:g} {y0 + 1.2:g} 0)",
        T * 2 + '(layer "F.SilkS")',
        T * 2 + f'(uuid "grp00000-0000-4000-8000-00000000020{i}")',
        T * 2 + "(effects",
        T * 3 + "(font",
        T * 4 + "(size 1 1)",
        T * 4 + "(thickness 0.15)",
        T * 3 + ")",
        T * 3 + "(justify left top)",
        T * 2 + ")",
        T + ")",
    )
    return shape + text


def main() -> int:
    text = open(PCB, encoding="utf-8", newline="").read()
    nl = "\r\n" if "\r\n" in text else "\n"
    text = text.replace("\r\n", "\n")
    edits, seen = [], set()
    for a, b in spans(text):
        blk = text[a:b]
        m = re.search(r'\(property "Reference" "([^"]+)"', blk)
        if not m or m[1] not in POS:
            continue
        x, y, rot = POS[m[1]]
        at = f"(at {x:g} {y:g}" + (f" {rot:g}" if rot else "") + ")"
        nb = re.sub(r"\(at\s+[-+\d.]+\s+[-+\d.]+(?:\s+[-+\d.]+)?\)", at, blk, count=1)
        seen.add(m[1])
        if nb != blk:
            edits.append((a, b, nb))
    for a, b, nb in reversed(edits):
        text = text[:a] + nb + text[b:]
    missing = set(POS) - seen
    if missing:
        print("không thấy footprint:", sorted(missing))
    nseg = len(re.findall(r"\n\t\(segment\b", text))
    nvia = len(re.findall(r"\n\t\(via\b", text))
    text = re.sub(r"\n\t\(segment\b.*?\n\t\)", "", text, flags=re.S)
    text = re.sub(r"\n\t\(via\b.*?\n\t\)", "", text, flags=re.S)
    text = strip_graphics(text, {g[-1] for g in GROUPS})
    add = "".join(frame(i, g) for i, g in enumerate(GROUPS))
    anchor = text.find(T + "(gr_rect" + NL + T * 2 + "(start 0 0)")
    if anchor < 0 or text.count("(") != text.count(")"):
        print("abort")
        return 1
    text = text[:anchor] + add + text[anchor:]
    with open(PCB, "w", encoding="utf-8", newline="") as f:
        f.write(text.replace("\n", nl))
    print(f"moved {len(edits)} footprints; stripped {nseg} segments, {nvia} vias; {len(GROUPS)} group frames")
    return 0


if __name__ == "__main__":
    sys.exit(main())
