"""Nudge footprints in panel-keyboard.kicad_pcb and check courtyard overlaps.

usage: python tools/move_fp.py REF dx dy [REF dx dy ...]      (mm, applied relative)
       python tools/move_fp.py --check                         (only report overlaps)
Close the board in KiCad first.
"""
import math
import re
import sys

from sexp import parse, val, find
from pcb_model import PCB


def footprint_spans(text):
    """yield (ref, start, end) char spans of top-level footprint blocks"""
    for m in re.finditer(r'^\t\(footprint ', text, re.M):
        i, depth = m.start(), 0
        j = i
        while True:
            c = text[j]
            if c == '(':
                depth += 1
            elif c == ')':
                depth -= 1
                if depth == 0:
                    break
            elif c == '"':
                j = text.index('"', j + 1)
            j += 1
        blk = text[i:j + 1]
        r = re.search(r'\(property "Reference" "([^"]+)"', blk)
        yield (r.group(1) if r else None), i, j + 1


def courtyards(path=PCB):
    t = parse(open(path, encoding='utf8').read())
    out = {}
    for fp in find(t, 'footprint'):
        ref = None
        for p in find(fp, 'property'):
            if val(p[1]) == 'Reference':
                ref = val(p[2])
        at = find(fp, 'at')[0]
        fx, fy = float(at[1]), float(at[2])
        fa = float(at[3]) if len(at) > 3 else 0.0
        side = val(find(fp, 'layer')[0][1])
        ca, sa = math.cos(math.radians(fa)), math.sin(math.radians(fa))
        xs, ys = [], []
        for prim in fp:
            if not isinstance(prim, list) or not prim or not str(prim[0]).startswith('fp_'):
                continue
            ly = find(prim, 'layer')
            if not ly or not val(ly[0][1]).endswith('CrtYd'):
                continue
            pts = []
            for k in (('center', 'end') if prim[0] == 'fp_circle' else ('start', 'end', 'center')):
                for e in find(prim, k):
                    pts.append((float(e[1]), float(e[2])))
            for e in find(prim, 'pts'):
                for xy in find(e, 'xy'):
                    pts.append((float(xy[1]), float(xy[2])))
            if prim[0] == 'fp_circle' and len(pts) >= 2:
                (cx, cy), (ex, ey) = pts[0], pts[1]
                r = math.hypot(ex - cx, ey - cy)
                pts = [(cx - r, cy - r), (cx + r, cy + r)]
            for lx, ly_ in pts:
                xs.append(fx + lx * ca + ly_ * sa)
                ys.append(fy - lx * sa + ly_ * ca)
        if xs:
            out[ref] = (min(xs), min(ys), max(xs), max(ys), side)
    return out


def overlaps(cy):
    res = []
    refs = sorted(cy)
    for i, a in enumerate(refs):
        for b in refs[i + 1:]:
            A, B = cy[a], cy[b]
            if A[4] != B[4]:
                continue
            if A[0] < B[2] - 1e-3 and B[0] < A[2] - 1e-3 and A[1] < B[3] - 1e-3 and B[1] < A[3] - 1e-3:
                res.append((a, b))
    return res


def move(moves, path=PCB):
    text = open(path, encoding='utf8').read()
    spans = {ref: (s, e) for ref, s, e in footprint_spans(text)}
    # apply from the end so earlier offsets stay valid
    for ref, dx, dy in sorted(moves, key=lambda m: -spans[m[0]][0]):
        s, e = spans[ref]
        blk = text[s:e]
        m = re.search(r'^\t\t\(at ([-\d.]+) ([-\d.]+)( [-\d.]+)?\)', blk, re.M)
        x, y = float(m.group(1)) + dx, float(m.group(2)) + dy
        new = f'\t\t(at {x:.4g} {y:.4g}{m.group(3) or ""})'
        blk = blk[:m.start()] + new + blk[m.end():]
        text = text[:s] + blk + text[e:]
        print(f'{ref}: -> ({x:.4g}, {y:.4g})')
    open(path, 'w', encoding='utf8', newline='\n').write(text)


if __name__ == '__main__':
    args = sys.argv[1:]
    if args and args[0] != '--check':
        mv = [(args[i], float(args[i + 1]), float(args[i + 2])) for i in range(0, len(args), 3)]
        move(mv)
    ov = overlaps(courtyards())
    print('courtyard overlaps:', ov if ov else 'none')
