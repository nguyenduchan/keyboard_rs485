"""Embed missing footprints, assign nets, route B.Cu/F.Cu traces.

Does not call pcbnew (KiCad lock crashes LoadBoard on this machine).
"""

from __future__ import annotations

import heapq
import json
import math
import re
import sys
import uuid
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from zone_layout import BOTTOM_POS, FRONT_POS

ROOT = Path(__file__).resolve().parents[1]
PCB = ROOT / "panel-keyboard.kicad_pcb"
SCH = ROOT / "panel-keyboard.kicad_sch"
CONN = ROOT / "panel-keyboard.connectivity.json"
FP_ROOT = Path(r"C:\Users\duchan.nguyen\AppData\Local\Programs\KiCad\10.0\share\kicad\footprints")
PANEL = ROOT / "footprints" / "Panel.pretty"
POS = {**FRONT_POS, **BOTTOM_POS}

CELL = 0.5
GW = int(round(100 / CELL))
GH = int(round(150 / CELL))
LAYER_NAME = ("B.Cu", "F.Cu")


def footprint_spans(text: str) -> list[tuple[int, int]]:
    spans: list[tuple[int, int]] = []
    key = "(footprint "
    i = 0
    while True:
        j = text.find(key, i)
        if j < 0:
            break
        depth = 0
        for k in range(j, len(text)):
            if text[k] == "(":
                depth += 1
            elif text[k] == ")":
                depth -= 1
                if depth == 0:
                    spans.append((j, k + 1))
                    i = k + 1
                    break
        else:
            break
    return spans


def sch_fp(text: str) -> dict[str, str]:
    out: dict[str, str] = {}
    for block in re.findall(r"\(symbol\b.*?\n\t\)", text, re.DOTALL):
        ref_m = re.search(r'\(property "Reference" "([^"]+)"', block)
        fp_m = re.search(r'\(property "Footprint" "([^"]+)"', block)
        if ref_m and fp_m and fp_m.group(1):
            out[ref_m.group(1)] = fp_m.group(1)
    return out


def load_mod(fp_id: str) -> str:
    lib, name = fp_id.split(":", 1)
    path = (PANEL if lib == "Panel" else FP_ROOT / f"{lib}.pretty") / f"{name}.kicad_mod"
    return path.read_text(encoding="utf-8")


def to_bottom(text: str) -> str:
    pairs = (
        ("F.Cu", "B.Cu"),
        ("F.SilkS", "B.SilkS"),
        ("F.Paste", "B.Paste"),
        ("F.Mask", "B.Mask"),
        ("F.CrtYd", "B.CrtYd"),
        ("F.Fab", "B.Fab"),
        ("F.Adhes", "B.Adhes"),
    )
    for a, b in pairs:
        text = text.replace(f'"{a}"', f'"{b}"')
    return text


def prepare_fp(fp_id: str, ref: str, x: float, y: float, rot: int, bottom: bool) -> str:
    mod = load_mod(fp_id)
    mod = re.sub(r'\(footprint "[^"]+"', f'(footprint "{fp_id}"', mod, count=1)
    if bottom:
        mod = to_bottom(mod)
    if '(property "Reference"' in mod:
        mod = re.sub(
            r'(\(property "Reference" ")[^"]*"',
            rf'\1{ref}"',
            mod,
            count=1,
        )
    else:
        silk = "B.SilkS" if bottom else "F.SilkS"
        mod = mod.replace(
            "\n",
            "\n"
            f'(property "Reference" "{ref}"\n'
            f'\t(at 0 -2 0)\n'
            f'\t(layer "{silk}")\n'
            f'\t(effects (font (size 0.8 0.8) (thickness 0.12)))\n'
            f')\n',
            1,
        )
    uid = str(uuid.uuid4())
    header = f'(at {x:g} {y:g} {rot:g})\n(uuid "{uid}")\n'
    nl = mod.find("\n")
    mod = mod[: nl + 1] + header + mod[nl + 1 :]
    return "\n".join("\t" + ln if ln else ln for ln in mod.splitlines()) + "\n"


def pad_numbers(block: str) -> set[str]:
    return set(re.findall(r'\(pad "([^"]*)"', block))


def needs_reload(ref: str, block: str, pins: set[str]) -> bool:
    pads = pad_numbers(block)
    if ref.startswith("D") and ref[1:].isdigit() and int(ref[1:]) >= 10:
        return "A" not in pads and "K" not in pads
    return any(p not in pads for p in pins)


def move_ats(text: str) -> tuple[str, int]:
    edits: list[tuple[int, int, str]] = []
    moved = 0
    for start, end in footprint_spans(text):
        block = text[start:end]
        ref_m = re.search(r'\(property "Reference" "([^"]+)"', block)
        if not ref_m or ref_m.group(1) not in POS:
            continue
        x, y, rot = POS[ref_m.group(1)]
        at = f"(at {x:g} {y:g}" + (f" {rot:g}" if rot else "") + ")"
        new_block, n = re.subn(
            r"\(at\s+[-+\d.]+\s+[-+\d.]+(?:\s+[-+\d.]+)?\)",
            at,
            block,
            count=1,
        )
        if n and new_block != block:
            edits.append((start, end, new_block))
            moved += 1
    for start, end, new_block in reversed(edits):
        text = text[:start] + new_block + text[end:]
    return text, moved


def net_for_pad(ref: str, num: str, conn: dict[tuple[str, str], str]) -> str | None:
    if ref.startswith("D") and ref[1:].isdigit() and int(ref[1:]) >= 10:
        if num == "A":
            return conn.get((ref, "1"))
        if num == "K":
            return conn.get((ref, "2"))
    if ref.startswith("SW"):
        if num == "4":
            return conn.get((ref, "1"))
        if num == "3":
            return conn.get((ref, "2"))
    return conn.get((ref, num))


def assign_nets(text: str, conn: dict[tuple[str, str], str]) -> tuple[str, int]:
    edits: list[tuple[int, int, str]] = []
    n = 0
    for start, end in footprint_spans(text):
        block = text[start:end]
        ref_m = re.search(r'\(property "Reference" "([^"]+)"', block)
        if not ref_m:
            continue
        ref = ref_m.group(1)
        spans = []
        i = 0
        while True:
            j = block.find("(pad ", i)
            if j < 0:
                break
            depth = 0
            for k in range(j, len(block)):
                if block[k] == "(":
                    depth += 1
                elif block[k] == ")":
                    depth -= 1
                    if depth == 0:
                        spans.append((j, k + 1))
                        i = k + 1
                        break
            else:
                break
        new_block = block
        shift = 0
        for ps, pe in spans:
            ps += shift
            pe += shift
            pad = new_block[ps:pe]
            num_m = re.match(r'\(pad "([^"]*)"', pad)
            if not num_m or not num_m.group(1):
                continue
            net = net_for_pad(ref, num_m.group(1), conn)
            if not net:
                continue
            if re.search(r'\(net "[^"]*"\)', pad):
                pad2 = re.sub(r'\(net "[^"]*"\)', f'(net "{net}")', pad, count=1)
            else:
                pad2 = pad[:-1] + f'\n\t\t\t(net "{net}"))'
            if pad2 != pad:
                new_block = new_block[:ps] + pad2 + new_block[pe:]
                shift += len(pad2) - len(pad)
                n += 1
        if new_block != block:
            edits.append((start, end, new_block))
    for start, end, new_block in reversed(edits):
        text = text[:start] + new_block + text[end:]
    return text, n


def abs_pad(fx: float, fy: float, rot: float, lx: float, ly: float) -> tuple[float, float]:
    r = math.radians(rot)
    c, s = math.cos(r), math.sin(r)
    return fx + lx * c - ly * s, fy + lx * s + ly * c


def collect_pads(text: str) -> list[dict]:
    pads = []
    for start, end in footprint_spans(text):
        block = text[start:end]
        ref_m = re.search(r'\(property "Reference" "([^"]+)"', block)
        at_m = re.search(r"\(at\s+([-+\d.]+)\s+([-+\d.]+)(?:\s+([-+\d.]+))?\)", block)
        if not ref_m or not at_m:
            continue
        fx, fy = float(at_m.group(1)), float(at_m.group(2))
        rot = float(at_m.group(3) or 0)
        i = 0
        while True:
            j = block.find("(pad ", i)
            if j < 0:
                break
            depth = 0
            for k in range(j, len(block)):
                if block[k] == "(":
                    depth += 1
                elif block[k] == ")":
                    depth -= 1
                    if depth == 0:
                        pad = block[j : k + 1]
                        i = k + 1
                        break
            else:
                break
            num_m = re.match(r'\(pad "([^"]*)"', pad)
            loc = re.search(r"\(at\s+([-+\d.]+)\s+([-+\d.]+)", pad)
            sz = re.search(r"\(size\s+([-+\d.]+)\s+([-+\d.]+)\)", pad)
            net_m = re.search(r'\(net "([^"]+)"\)', pad)
            lay_m = re.search(r'\(layers\s+([^)]*)\)', pad)
            if not (num_m and loc and sz and num_m.group(1)):
                continue
            ax, ay = abs_pad(fx, fy, rot, float(loc.group(1)), float(loc.group(2)))
            w, h = float(sz.group(1)), float(sz.group(2))
            if int(rot) % 180 == 90:
                w, h = h, w
            layers = lay_m.group(1) if lay_m else "B.Cu"
            both = "*.Cu" in layers or ("F.Cu" in layers and "B.Cu" in layers)
            lys = [0, 1] if both else ([1] if "F.Cu" in layers else [0])
            pads.append(
                {
                    "ref": ref_m.group(1),
                    "num": num_m.group(1),
                    "net": net_m.group(1) if net_m else "",
                    "x": ax,
                    "y": ay,
                    "w": w,
                    "h": h,
                    "layers": lys,
                }
            )
    return pads


def idx(layer: int, x: int, y: int) -> int:
    return (layer * GH + y) * GW + x


def route(pads: list[dict]) -> tuple[list[str], list[str], list[str]]:
    blocked = bytearray(2 * GW * GH)
    for p in pads:
        x0 = max(0, int(math.floor((p["x"] - p["w"] / 2) / CELL)))
        x1 = min(GW - 1, int(math.ceil((p["x"] + p["w"] / 2) / CELL)))
        y0 = max(0, int(math.floor((p["y"] - p["h"] / 2) / CELL)))
        y1 = min(GH - 1, int(math.ceil((p["y"] + p["h"] / 2) / CELL)))
        for ly in (0, 1):
            for yy in range(y0, y1 + 1):
                for xx in range(x0, x1 + 1):
                    blocked[idx(ly, xx, yy)] = 1
    # board edge
    for ly in (0, 1):
        for x in range(GW):
            blocked[idx(ly, x, 0)] = 1
            blocked[idx(ly, x, GH - 1)] = 1
        for y in range(GH):
            blocked[idx(ly, 0, y)] = 1
            blocked[idx(ly, GW - 1, y)] = 1

    by_net: dict[str, list[dict]] = {}
    for p in pads:
        if p["net"]:
            by_net.setdefault(p["net"], []).append(p)

    segs: list[str] = []
    vias: list[str] = []
    failed: list[str] = []

    def cell_of(p: dict) -> tuple[int, int, int]:
        return p["layers"][0], int(round(p["x"] / CELL)), int(round(p["y"] / CELL))

    def pad_cells(p: dict) -> set[tuple[int, int, int]]:
        cells: set[tuple[int, int, int]] = set()
        x0 = max(0, int(math.floor((p["x"] - p["w"] / 2) / CELL)))
        x1 = min(GW - 1, int(math.ceil((p["x"] + p["w"] / 2) / CELL)))
        y0 = max(0, int(math.floor((p["y"] - p["h"] / 2) / CELL)))
        y1 = min(GH - 1, int(math.ceil((p["y"] + p["h"] / 2) / CELL)))
        for ly in p["layers"]:
            for yy in range(y0, y1 + 1):
                for xx in range(x0, x1 + 1):
                    cells.add((ly, xx, yy))
        return cells

    def astar(
        start: tuple[int, int, int],
        goals: set[tuple[int, int, int]],
        free: set[tuple[int, int, int]],
    ) -> list[tuple[int, int, int]] | None:
        if start in goals:
            return [start]
        sl, sx, sy = start
        if not (0 <= sx < GW and 0 <= sy < GH):
            return None
        inf = 10**9
        best_h = min(abs(sx - g[1]) + abs(sy - g[2]) for g in goals)
        heap = [(best_h, 0, sl, sx, sy)]
        seen: dict[tuple[int, int, int], int] = {start: 0}
        parent: dict[tuple[int, int, int], tuple[int, int, int]] = {}
        steps = 0
        while heap and steps < 200000:
            _f, g, ly, x, y = heapq.heappop(heap)
            steps += 1
            node = (ly, x, y)
            if g != seen.get(node, inf):
                continue
            if node in goals and node != start:
                path = [node]
                while node in parent:
                    node = parent[node]
                    path.append(node)
                path.reverse()
                return path
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if not (0 <= nx < GW and 0 <= ny < GH):
                    continue
                nxt = (ly, nx, ny)
                if blocked[idx(ly, nx, ny)] and nxt not in goals and nxt not in free:
                    continue
                ng = g + 1
                if ng < seen.get(nxt, inf):
                    seen[nxt] = ng
                    parent[nxt] = (ly, x, y)
                    hh = min(abs(nx - gg[1]) + abs(ny - gg[2]) for gg in goals)
                    heapq.heappush(heap, (ng + hh, ng, ly, nx, ny))
            oly = 1 - ly
            via = (oly, x, y)
            if 1 <= x < GW - 1 and 1 <= y < GH - 1:
                if not (blocked[idx(oly, x, y)] and via not in goals and via not in free):
                    ng = g + 6
                    if ng < seen.get(via, inf):
                        seen[via] = ng
                        parent[via] = (ly, x, y)
                        hh = min(abs(x - gg[1]) + abs(y - gg[2]) for gg in goals)
                        heapq.heappush(heap, (ng + hh, ng, oly, x, y))
        return None

    def emit(path: list[tuple[int, int, int]], net: str, width: float) -> None:
        if len(path) < 2:
            return
        run = [path[0]]
        chunks: list[list[tuple[int, int, int]]] = []
        for p in path[1:]:
            if p[0] != run[-1][0]:
                chunks.append(run)
                x = run[-1][1] * CELL
                y = run[-1][2] * CELL
                vias.append(
                    f'\t(via\n\t\t(at {x:.3f} {y:.3f})\n\t\t(size 0.8)\n\t\t(drill 0.4)\n'
                    f'\t\t(layers "F.Cu" "B.Cu")\n\t\t(net "{net}")\n\t\t(uuid "{uuid.uuid4()}")\n\t)'
                )
                blocked[idx(0, run[-1][1], run[-1][2])] = 1
                blocked[idx(1, run[-1][1], run[-1][2])] = 1
                run = [p]
            else:
                run.append(p)
        chunks.append(run)
        for run in chunks:
            for p in run:
                blocked[idx(p[0], p[1], p[2])] = 1
            if len(run) < 2:
                continue
            pts = [run[0]]
            for i in range(1, len(run) - 1):
                d1 = (run[i][1] - run[i - 1][1], run[i][2] - run[i - 1][2])
                d2 = (run[i + 1][1] - run[i][1], run[i + 1][2] - run[i][2])
                if d1 != d2:
                    pts.append(run[i])
            pts.append(run[-1])
            layer = LAYER_NAME[run[0][0]]
            for a, b in zip(pts, pts[1:]):
                segs.append(
                    f'\t(segment\n\t\t(start {a[1] * CELL:.3f} {a[2] * CELL:.3f})\n'
                    f'\t\t(end {b[1] * CELL:.3f} {b[2] * CELL:.3f})\n'
                    f'\t\t(width {width})\n\t\t(layer "{layer}")\n'
                    f'\t\t(net "{net}")\n\t\t(uuid "{uuid.uuid4()}")\n\t)'
                )

    # GND / GND_PWR dùng vùng đồng, không chiếm khe tín hiệu.
    order = sorted(
        (n for n in by_net if n not in ("GND", "GND_PWR")),
        key=lambda n: -len(by_net[n]),
    )
    for net in order:
        group = by_net[net]
        if len(group) < 2:
            continue
        width = 0.45 if net in ("GND", "GND_PWR") or net.startswith("+") else 0.25
        remaining = group[1:]
        goal_cells: set[tuple[int, int, int]] = set()
        free: set[tuple[int, int, int]] = set()
        for p in group:
            free |= pad_cells(p)
        goal_cells |= pad_cells(group[0])
        unrouted = 0
        while remaining:
            def dist(p: dict) -> int:
                cx, cy = int(round(p["x"] / CELL)), int(round(p["y"] / CELL))
                return min(abs(cx - g[1]) + abs(cy - g[2]) for g in goal_cells)

            remaining.sort(key=dist)
            nxt = remaining.pop(0)
            start = cell_of(nxt)
            path = astar(start, goal_cells, free)
            if path is None:
                unrouted += 1
                goal_cells.add(start)
                continue
            emit(path, net, width)
            for p in path:
                goal_cells.add(p)
        if unrouted:
            failed.append(f"{net} ({unrouted})")
    return segs, vias, failed


def main() -> int:
    conn_rows = json.loads(CONN.read_text(encoding="utf-8"))
    conn = {(r["ref"], r["pin"]): r["net"] for r in conn_rows}
    pins_of: dict[str, set[str]] = {}
    for r in conn_rows:
        pins_of.setdefault(r["ref"], set()).add(r["pin"])
    fps = sch_fp(SCH.read_text(encoding="utf-8"))
    text = PCB.read_text(encoding="utf-8")

    existing: dict[str, tuple[int, int, str]] = {}
    for start, end in footprint_spans(text):
        block = text[start:end]
        ref_m = re.search(r'\(property "Reference" "([^"]+)"', block)
        if ref_m:
            existing[ref_m.group(1)] = (start, end, block)

    remove: list[tuple[int, int]] = []
    add: list[str] = []
    for ref, pins in pins_of.items():
        if ref not in POS and ref not in existing:
            continue
        if ref not in existing:
            add.append(ref)
            continue
        if needs_reload(ref, existing[ref][2], pins):
            remove.append((existing[ref][0], existing[ref][1]))
            add.append(ref)
    for start, end in sorted(remove, reverse=True):
        text = text[:start] + text[end:]
    blobs = []
    for ref in add:
        if ref not in fps or ref not in POS:
            print("skip embed", ref)
            continue
        x, y, rot = POS[ref]
        bottom = ref in BOTTOM_POS
        blobs.append(prepare_fp(fps[ref], ref, x, y, rot, bottom))
        print("embed", ref)
    if blobs:
        anchor = text.rfind("\t(embedded_fonts no)")
        text = text[:anchor] + "".join(blobs) + text[anchor:]

    text, moved = move_ats(text)
    print("moved", moved)
    text, nnets = assign_nets(text, conn)
    print("nets on pads", nnets)

    text = re.sub(r"\n\t\(segment\b.*?\n\t\)", "", text, flags=re.DOTALL)
    text = re.sub(r"\n\t\(via\b.*?\n\t\)", "", text, flags=re.DOTALL)
    # RS485 jack moved to x=86; keep silk above the pins.
    text = text.replace('(gr_text "RS485"\n\t\t(at 78 133 0)', '(gr_text "RS485"\n\t\t(at 86 133 0)')
    text = text.replace('(gr_text "A"\n\t\t(at 72.9 138 0)', '(gr_text "A"\n\t\t(at 80.92 138 0)')
    text = text.replace('(gr_text "B"\n\t\t(at 78 138 0)', '(gr_text "B"\n\t\t(at 86 138 0)')
    text = text.replace('(gr_text "0V"\n\t\t(at 83.1 138 0)', '(gr_text "0V"\n\t\t(at 91.08 138 0)')

    pads = collect_pads(text)
    print("pads", len(pads), "with net", sum(1 for p in pads if p["net"]))
    segs, vias, failed = route(pads)
    print("segments", len(segs), "vias", len(vias), "unrouted", len(failed))
    for item in failed[:30]:
        print("  open", item)
    if '(zone\n\t\t(net "GND_PWR")' not in text:
        segs.append(
            "\t(zone\n"
            '\t\t(net "GND_PWR")\n'
            '\t\t(layer "B.Cu")\n'
            f'\t\t(uuid "{uuid.uuid4()}")\n'
            "\t\t(hatch edge 0.5)\n"
            "\t\t(connect_pads\n"
            "\t\t\t(clearance 0.2)\n"
            "\t\t)\n"
            "\t\t(min_thickness 0.2)\n"
            "\t\t(fill yes\n"
            "\t\t\t(thermal_gap 0.5)\n"
            "\t\t\t(thermal_bridge_width 0.5)\n"
            "\t\t)\n"
            "\t\t(polygon\n"
            "\t\t\t(pts\n"
            "\t\t\t\t(xy 3 107) (xy 77 107) (xy 77 132) (xy 3 132)\n"
            "\t\t\t)\n"
            "\t\t)\n"
            "\t)"
        )
    blob = "\n".join(segs + vias) + "\n"
    anchor = text.rfind("\t(embedded_fonts no)")
    text = text[:anchor] + blob + text[anchor:]
    if text.count("(") != text.count(")"):
        print("abort paren", text.count("(") - text.count(")"))
        return 1
    PCB.write_text(text, encoding="utf-8")
    print("wrote", PCB)
    return 0


if __name__ == "__main__":
    sys.exit(main())
