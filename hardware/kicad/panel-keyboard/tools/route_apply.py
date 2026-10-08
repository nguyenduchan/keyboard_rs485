"""Apply autoroute result to panel-keyboard.kicad_pcb (and run a geometric DRC).

usage: python tools/route_apply.py problem.txt result.txt [--write] [--drc-only]
Close the board in KiCad before --write.
"""
import math
import re
import sys
import uuid
from collections import defaultdict

from pcb_model import load, PCB
from route_export import pad_geom, CLEARANCE, EDGE, VIA_D, VIA_DRILL, BOARD_W, BOARD_H

LAYER = {0: 'F.Cu', 1: 'B.Cu'}


def read_problem_nets(path):
    nets = {}
    for ln in open(path, encoding='utf8'):
        if ln.startswith('NET '):
            _, i, c, name = ln.rstrip('\n').split(' ', 3)
            nets[int(i)] = name
    return nets


def read_result(path):
    segs, vias, uns = [], [], []
    for ln in open(path, encoding='utf8'):
        f = ln.split()
        if not f:
            continue
        if f[0] == 'SEG':
            segs.append([int(f[1]), int(f[2])] + [float(v) for v in f[3:8]])
        elif f[0] == 'VIA':
            vias.append([int(f[1]), float(f[2]), float(f[3])])
        elif f[0] == 'UNROUTED':
            uns.append(ln[9:].strip())
    return segs, vias, uns


def dedupe(segs):
    """drop zero-length and duplicate segments"""
    seen, out = set(), []
    for s in segs:
        n, l, x1, y1, x2, y2, w = s
        if abs(x1 - x2) < 1e-6 and abs(y1 - y2) < 1e-6:
            continue
        key = (n, l, round(x1, 3), round(y1, 3), round(x2, 3), round(y2, 3))
        key2 = (n, l, round(x2, 3), round(y2, 3), round(x1, 3), round(y1, 3))
        if key in seen or key2 in seen:
            continue
        seen.add(key)
        out.append(s)
    return out


def seg_dist_pt(px, py, x1, y1, x2, y2):
    vx, vy = x2 - x1, y2 - y1
    l2 = vx * vx + vy * vy
    t = 0 if l2 == 0 else max(0, min(1, ((px - x1) * vx + (py - y1) * vy) / l2))
    return math.hypot(x1 + t * vx - px, y1 + t * vy - py)


def seg_seg_dist(a, b):
    ax1, ay1, ax2, ay2 = a
    bx1, by1, bx2, by2 = b

    def ccw(p, q, r):
        return (q[0] - p[0]) * (r[1] - p[1]) - (q[1] - p[1]) * (r[0] - p[0])
    p1, p2, p3, p4 = (ax1, ay1), (ax2, ay2), (bx1, by1), (bx2, by2)
    d1, d2, d3, d4 = ccw(p3, p4, p1), ccw(p3, p4, p2), ccw(p1, p2, p3), ccw(p1, p2, p4)
    if ((d1 > 0) != (d2 > 0)) and ((d3 > 0) != (d4 > 0)) and d1 * d2 < 0 and d3 * d4 < 0:
        return 0.0
    return min(seg_dist_pt(ax1, ay1, bx1, by1, bx2, by2), seg_dist_pt(ax2, ay2, bx1, by1, bx2, by2),
               seg_dist_pt(bx1, by1, ax1, ay1, ax2, ay2), seg_dist_pt(bx2, by2, ax1, ay1, ax2, ay2))


def rect_seg_dist(cx, cy, hx, hy, x1, y1, x2, y2):
    """distance from axis-aligned rect to segment (0 if intersect)"""
    # sample-free: clip test + endpoint distances + rect corners to segment
    def pt_rect(px, py):
        return math.hypot(max(abs(px - cx) - hx, 0), max(abs(py - cy) - hy, 0))
    # intersection test by Liang-Barsky
    t0, t1 = 0.0, 1.0
    dx, dy = x2 - x1, y2 - y1
    for p, q in ((-dx, x1 - (cx - hx)), (dx, (cx + hx) - x1), (-dy, y1 - (cy - hy)), (dy, (cy + hy) - y1)):
        if p == 0:
            if q < 0:
                t0, t1 = 1, 0
                break
        else:
            r = q / p
            if p < 0:
                t0 = max(t0, r)
            else:
                t1 = min(t1, r)
    if t0 <= t1:
        return 0.0
    d = min(pt_rect(x1, y1), pt_rect(x2, y2))
    for qx, qy in ((cx - hx, cy - hy), (cx + hx, cy - hy), (cx + hx, cy + hy), (cx - hx, cy + hy)):
        d = min(d, seg_dist_pt(qx, qy, x1, y1, x2, y2))
    return d


def drc(segs, vias, pads, netname):
    errs = []
    grid = defaultdict(list)
    for i, s in enumerate(segs):
        n, l, x1, y1, x2, y2, w = s
        for gx in range(int(min(x1, x2) // 5) - 1, int(max(x1, x2) // 5) + 2):
            for gy in range(int(min(y1, y2) // 5) - 1, int(max(y1, y2) // 5) + 2):
                grid[(l, gx, gy)].append(i)
    tol = 0.01
    # seg-seg
    seen = set()
    for key, lst in grid.items():
        for a in range(len(lst)):
            for b in range(a + 1, len(lst)):
                i, j = lst[a], lst[b]
                if (i, j) in seen:
                    continue
                seen.add((i, j))
                s, t = segs[i], segs[j]
                if s[0] == t[0]:
                    continue
                d = seg_seg_dist(s[2:6], t[2:6]) - s[6] / 2 - t[6] / 2
                if d < CLEARANCE - tol:
                    errs.append(f'track-track {netname[s[0]]}/{netname[t[0]]} {s[1]} gap {d:.3f} at ({s[2]:.2f},{s[3]:.2f})')
    # seg-pad, via-pad
    for p in pads:
        cx, cy, hx, hy, circ = pad_geom(p)
        pn = p['net']
        lays = [0] if ('F.Cu' in p['layers']) else ([1] if 'B.Cu' in p['layers'] else ([0, 1] if '*.Cu' in p['layers'] else []))
        for l in lays:
            r = max(hx, hy) + 2
            for gx in range(int((cx - r) // 5), int((cx + r) // 5) + 1):
                for gy in range(int((cy - r) // 5), int((cy + r) // 5) + 1):
                    for i in grid.get((l, gx, gy), []):
                        s = segs[i]
                        if netname[s[0]] == pn:
                            continue
                        if circ:
                            d = seg_dist_pt(cx, cy, *s[2:6]) - hx - s[6] / 2
                        else:
                            d = rect_seg_dist(cx, cy, hx, hy, *s[2:6]) - s[6] / 2
                        if d < CLEARANCE - tol:
                            errs.append(f"track-pad {netname[s[0]]} vs {p['ref']}.{p['num']}({pn}) gap {d:.3f} at ({cx:.2f},{cy:.2f})")
        for v in vias:
            if netname[v[0]] == pn:
                continue
            if circ:
                d = math.hypot(v[1] - cx, v[2] - cy) - hx - VIA_D / 2
            else:
                d = math.hypot(max(abs(v[1] - cx) - hx, 0), max(abs(v[2] - cy) - hy, 0)) - VIA_D / 2
            if d < CLEARANCE - tol:
                errs.append(f"via-pad {netname[v[0]]} vs {p['ref']}.{p['num']} gap {d:.3f} at ({v[1]:.2f},{v[2]:.2f})")
    # via-seg, via-via
    for v in vias:
        for l in (0, 1):
            for i in grid.get((l, int(v[1] // 5), int(v[2] // 5)), []):
                s = segs[i]
                if s[0] == v[0]:
                    continue
                d = seg_dist_pt(v[1], v[2], *s[2:6]) - s[6] / 2 - VIA_D / 2
                if d < CLEARANCE - tol:
                    errs.append(f'via-track {netname[v[0]]}/{netname[s[0]]} gap {d:.3f} at ({v[1]:.2f},{v[2]:.2f})')
    for i in range(len(vias)):
        for j in range(i + 1, len(vias)):
            a, b = vias[i], vias[j]
            if a[0] != b[0] and math.hypot(a[1] - b[1], a[2] - b[2]) - VIA_D < CLEARANCE - tol:
                errs.append(f'via-via {netname[a[0]]}/{netname[b[0]]} at ({a[1]:.2f},{a[2]:.2f})')
    # board edge
    for s in segs:
        for x, y in ((s[2], s[3]), (s[4], s[5])):
            m = min(x, y, BOARD_W - x, BOARD_H - y) - s[6] / 2
            if m < EDGE - tol:
                errs.append(f'edge {netname[s[0]]} at ({x:.2f},{y:.2f}) {m:.2f}')
    return errs


def connectivity(segs, vias, pads, netname):
    """union-find over copper; returns {net: number of components containing pads}"""
    parent = {}

    def find(a):
        while parent.setdefault(a, a) != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    def union(a, b):
        parent[find(a)] = find(b)
    nid = {v: k for k, v in netname.items()}
    items = []
    for i, s in enumerate(segs):
        items.append(('s', i))
    # tracks touching tracks / vias / pads of same net
    byNet = defaultdict(list)
    for i, s in enumerate(segs):
        byNet[s[0]].append(('s', i))
    for i, v in enumerate(vias):
        byNet[v[0]].append(('v', i))
    padsByNet = defaultdict(list)
    for k, p in enumerate(pads):
        if p['net']:
            padsByNet[nid[p['net']]].append(k)
    res = {}
    for n in set(byNet) | set(padsByNet):
        els = byNet[n]
        for a in range(len(els)):
            for b in range(a + 1, len(els)):
                ea, eb = els[a], els[b]
                if ea[0] == 's' and eb[0] == 's':
                    s, t = segs[ea[1]], segs[eb[1]]
                    if s[1] == t[1] and seg_seg_dist(s[2:6], t[2:6]) <= 0.005:
                        union(ea, eb)
                elif ea[0] == 'v' and eb[0] == 'v':
                    pass
                else:
                    sv, vv = (ea, eb) if ea[0] == 's' else (eb, ea)
                    s, v = segs[sv[1]], vias[vv[1]]
                    if seg_dist_pt(v[1], v[2], *s[2:6]) <= 0.005:
                        union(sv, vv)
        # vias connect both layers: segments touching via already unioned
        for k in padsByNet[n]:
            p = pads[k]
            cx, cy, hx, hy, circ = pad_geom(p)
            lays = [0, 1] if '*.Cu' in p['layers'] else ([0] if 'F.Cu' in p['layers'] else [1])
            key = ('p', k)
            find(key)
            for e in els:
                if e[0] == 's':
                    s = segs[e[1]]
                    if s[1] not in lays:
                        continue
                    d = (seg_dist_pt(cx, cy, *s[2:6]) - hx) if circ else rect_seg_dist(cx, cy, hx, hy, *s[2:6])
                    if d <= 0.005:
                        union(key, e)
        # through-hole pads join both layers
        comps = {find(('p', k)) for k in padsByNet[n]}
        res[n] = len(comps)
    return res


def split_tjunctions(segs):
    """split segments at endpoints of same-net segments that lie on their interior"""
    byNet = defaultdict(list)
    for s in segs:
        byNet[(s[0], s[1])].append(s)
    out = []
    for key, lst in byNet.items():
        pts = {(round(p[0], 4), round(p[1], 4)) for s in lst for p in ((s[2], s[3]), (s[4], s[5]))}
        for s in lst:
            n, l, x1, y1, x2, y2, w = s
            cuts = []
            for (px, py) in pts:
                if (abs(px - x1) < 1e-4 and abs(py - y1) < 1e-4) or (abs(px - x2) < 1e-4 and abs(py - y2) < 1e-4):
                    continue
                if seg_dist_pt(px, py, x1, y1, x2, y2) < 2e-3:
                    t = ((px - x1) * (x2 - x1) + (py - y1) * (y2 - y1)) / ((x2 - x1) ** 2 + (y2 - y1) ** 2)
                    cuts.append((t, px, py))
            cuts.sort()
            cur = (x1, y1)
            for t, px, py in cuts:
                out.append([n, l, cur[0], cur[1], px, py, w])
                cur = (px, py)
            out.append([n, l, cur[0], cur[1], x2, y2, w])
    return out


def kicad_text(segs, vias, netname):
    L = []
    for n, l, x1, y1, x2, y2, w in segs:
        L.append(f'\t(segment\n\t\t(start {x1:.4f} {y1:.4f})\n\t\t(end {x2:.4f} {y2:.4f})\n\t\t(width {w:g})\n'
                 f'\t\t(layer "{LAYER[l]}")\n\t\t(net "{netname[n]}")\n\t\t(uuid "{uuid.uuid4()}")\n\t)\n')
    for n, x, y in vias:
        L.append(f'\t(via\n\t\t(at {x:.4f} {y:.4f})\n\t\t(size {VIA_D:g})\n\t\t(drill {VIA_DRILL:g})\n'
                 f'\t\t(layers "F.Cu" "B.Cu")\n\t\t(net "{netname[n]}")\n\t\t(uuid "{uuid.uuid4()}")\n\t)\n')
    return ''.join(L)


def strip_tracks(text):
    """remove existing top-level (segment ...) and (via ...) blocks"""
    out, skip, depth = [], False, 0
    for ln in text.split('\n'):
        if not skip and re.match(r'^\t\((segment|via)$', ln):
            skip, depth = True, 1
            continue
        if skip:
            depth += ln.count('(') - ln.count(')')
            if depth <= 0:
                skip = False
            continue
        out.append(ln)
    return '\n'.join(out)


def main():
    prob, res = sys.argv[1], sys.argv[2]
    netname = read_problem_nets(prob)
    netname[0] = ''
    segs, vias, uns = read_result(res)
    segs = split_tjunctions(dedupe(segs))
    t, fps, pads = load()
    errs = drc(segs, vias, pads, netname)
    print(f'{len(segs)} segments, {len(vias)} vias, {len(uns)} unrouted terminals, {len(errs)} DRC violations')
    for e in errs[:40]:
        print('  DRC', e)
    for u in uns[:40]:
        print('  UNROUTED', u)
    comp = connectivity(segs, vias, pads, netname)
    bad = {netname[n]: c for n, c in comp.items() if c > 1}
    print('nets with >1 component:', bad)
    if '--write' in sys.argv:
        text = open(PCB, encoding='utf8').read()
        text = strip_tracks(text)
        idx = text.rstrip().rfind('\t(embedded_fonts no)\n)')
        if idx < 0:
            raise SystemExit('cannot find insertion point')
        text = text[:idx] + kicad_text(segs, vias, netname) + text[idx:]
        open(PCB, 'w', encoding='utf8', newline='\n').write(text)
        print('written', PCB)


if __name__ == '__main__':
    main()
