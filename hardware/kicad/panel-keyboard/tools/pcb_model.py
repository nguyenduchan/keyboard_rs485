"""Extract absolute pad geometry from panel-keyboard.kicad_pcb."""
import math
from sexp import parse, val, find

PCB = 'panel-keyboard.kicad_pcb'


def _f(x):
    return float(x)


def load(path=PCB):
    t = parse(open(path, encoding='utf8').read())
    pads = []
    fps = {}
    for fp in find(t, 'footprint'):
        ref = None
        for p in find(fp, 'property'):
            if val(p[1]) == 'Reference':
                ref = val(p[2])
        at = find(fp, 'at')[0]
        fx, fy = _f(at[1]), _f(at[2])
        fa = _f(at[3]) if len(at) > 3 else 0.0
        layer = val(find(fp, 'layer')[0][1])
        fps[ref] = dict(name=val(fp[1]), x=fx, y=fy, a=fa, layer=layer)
        ca, sa = math.cos(math.radians(fa)), math.sin(math.radians(fa))
        for pd in find(fp, 'pad'):
            num = val(pd[1])
            ptype, shape = pd[2], pd[3]
            pat = find(pd, 'at')[0]
            lx, ly = _f(pat[1]), _f(pat[2])
            pa = _f(pat[3]) if len(pat) > 3 else 0.0
            # KiCad: rotation is clockwise-positive on screen (y down) => x' = x*cos + y*sin
            ax = fx + lx * ca + ly * sa
            ay = fy - lx * sa + ly * ca
            sz = find(pd, 'size')[0]
            w, h = _f(sz[1]), _f(sz[2])
            dr = find(pd, 'drill')
            drill = None
            if dr:
                d = dr[0]
                drill = [_f(v) for v in d[1:] if not isinstance(v, (list, tuple)) and v != 'oval']
                drill = max(drill) if drill else None
            lay = [val(v) for v in find(pd, 'layers')[0][1:]]
            nt = find(pd, 'net')
            net = val(nt[0][1]) if nt else ''
            pads.append(dict(ref=ref, num=num, type=ptype, shape=shape, x=ax, y=ay,
                             a=pa, w=w, h=h, drill=drill, layers=lay, net=net,
                             fpname=fps[ref]['name'], fp=fps[ref],
                             prims=bool(find(pd, 'primitives'))))
    return t, fps, pads


if __name__ == '__main__':
    t, fps, pads = load()
    nets = {}
    for p in pads:
        nets.setdefault(p['net'], []).append(p)
    print(len(pads), 'pads', len(nets), 'nets')
    for n, ps in sorted(nets.items(), key=lambda kv: -len(kv[1])):
        print(f'{n or "<none>":14s} {len(ps):3d}', ' '.join(f"{p['ref']}.{p['num']}" for p in ps[:14]))
    print('shapes', sorted({(p['type'], p['shape'], p['prims']) for p in pads}))
    print('angles', sorted({p['a'] % 360 for p in pads}))
    for p in pads:
        if p['prims'] or p['type'] == 'np_thru_hole':
            print(p['ref'], p['num'], p['type'], p['shape'], p['w'], p['h'], p['drill'], p['layers'])


def fp_copper(t):
    """bounding boxes (cx, cy, hx, hy, layermask) of fp_poly / fp_rect on copper layers"""
    out = []
    for fp in find(t, 'footprint'):
        at = find(fp, 'at')[0]
        fx, fy = _f(at[1]), _f(at[2])
        fa = _f(at[3]) if len(at) > 3 else 0.0
        ca, sa = math.cos(math.radians(fa)), math.sin(math.radians(fa))
        for g in fp:
            if not isinstance(g, list) or not g or g[0] not in ('fp_poly', 'fp_rect'):
                continue
            ly = find(g, 'layer')
            if not ly or val(ly[0][1]) not in ('F.Cu', 'B.Cu'):
                continue
            pts = [(_f(e[1]), _f(e[2])) for k in ('start', 'end') for e in find(g, k)]
            for e in find(g, 'pts'):
                pts += [(_f(xy[1]), _f(xy[2])) for xy in find(e, 'xy')]
            xs = [fx + x * ca + y * sa for x, y in pts]
            ys = [fy - x * sa + y * ca for x, y in pts]
            lay = 1 if val(ly[0][1]) == 'F.Cu' else 2
            out.append(((min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2, (max(xs) - min(xs)) / 2, (max(ys) - min(ys)) / 2, lay))
    return out
