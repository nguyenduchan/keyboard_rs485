"""Export routing problem for autoroute.cpp from panel-keyboard.kicad_pcb.

usage: python tools/route_export.py problem.txt
"""
import math
import sys

from pcb_model import load, fp_copper

BOARD_W, BOARD_H = 120.0, 120.0
CLEARANCE = 0.2
EDGE = 0.5
VIA_D, VIA_DRILL = 0.6, 0.3

# track-width classes (mm): signal / rail / main power path
CLASS_W = [0.25, 0.35, 0.5]
POWER_MAIN = {'+24V_IN', '+24V_FUSE', 'VIN_PROTECT', '+24V', '+5V_SW', '+5V_RAW', 'GND_PWR'}
POWER_RAIL = {'+5V', '+3V3', '+3V3_LDO', '+5V_LED', 'GND'}


def net_class(name):
    if name in POWER_MAIN:
        return 2
    if name in POWER_RAIL:
        return 1
    return 0


def pad_geom(p):
    """(cx, cy, hx, hy, is_circle) in absolute coordinates."""
    w, h = p['w'], p['h']
    if p['shape'] == 'custom':
        w, h = 1.0, 1.5
    ang = p['a'] % 180  # KiCad stores the pad angle as an absolute (board) angle
    if abs(ang - 90) < 1e-6:
        w, h = h, w
    if p['shape'] == 'circle':
        return p['x'], p['y'], w / 2, w / 2, True
    return p['x'], p['y'], w / 2, h / 2, False


def main(out):
    t, fps, pads = load()
    nets = sorted({p['net'] for p in pads if p['net']})
    nid = {n: i + 1 for i, n in enumerate(nets)}
    count = {}
    for p in pads:
        if p['net']:
            count[p['net']] = count.get(p['net'], 0) + 1

    L = []
    L.append(f'BOARD {BOARD_W} {BOARD_H}')
    L.append(f'RULE {CLEARANCE} {EDGE} {VIA_D} {VIA_DRILL}')
    L.append('CLASS ' + ' '.join(str(w) for w in CLASS_W))
    for n in nets:
        L.append(f'NET {nid[n]} {net_class(n)} {n}')

    for p in pads:
        cx, cy, hx, hy, circ = pad_geom(p)
        lay = 0
        for l in p['layers']:
            if l in ('F.Cu', '*.Cu'):
                lay |= 1
            if l in ('B.Cu', '*.Cu'):
                lay |= 2
        if lay == 0:
            continue                        # paste/mask-only pads (U4 thermal paste windows)
        net = nid.get(p['net'], 0) if (p['net'] and count[p['net']] >= 2) else -1
        if p['net'] and count[p['net']] < 2:
            net = nid[p['net']]
        shape = f'C {cx:.4f} {cy:.4f} {hx:.4f}' if circ else f'R {cx:.4f} {cy:.4f} {hx:.4f} {hy:.4f}'
        L.append(f'OBS {net} {lay} {shape}')

    # copper graphics inside footprints (net-tie bridges etc.): hard obstacles
    for cx, cy, hx, hy, lay in fp_copper(t):
        L.append(f'OBS -1 {lay} R {cx:.4f} {cy:.4f} {hx:.4f} {hy:.4f}')

    # slots milled inside footprints (J_KEY membrane tail): keep EDGE from copper on both layers
    grow = EDGE - CLEARANCE
    for cx, cy, hx, hy, _ in fp_copper(t, ('Edge.Cuts',)):
        L.append(f'OBS -1 3 R {cx:.4f} {cy:.4f} {hx + grow:.4f} {hy + grow:.4f}')

    # terminals
    for p in pads:
        if not p['net'] or count[p['net']] < 2:
            continue
        cx, cy, hx, hy, circ = pad_geom(p)
        n = nid[p['net']]
        lay = 0
        for l in p['layers']:
            if l in ('F.Cu', '*.Cu'):
                lay |= 1
            if l in ('B.Cu', '*.Cu'):
                lay |= 2
        if lay == 0:
            continue
        smd = p['type'] == 'smd'
        big = max(hx, hy) * 2 > 3.0 or min(hx, hy) * 2 > 1.6
        key = f"{p['ref']}.{p['num']}"
        npads_fp = sum(1 for q in pads if q['ref'] == p['ref'])
        if smd and not big and npads_fp >= 3:
            fx, fy = p['fp']['x'], p['fp']['y']
            dx, dy = cx - fx, cy - fy
            if abs(dx) >= abs(dy):
                d = (1 if dx >= 0 else -1, 0)
                edge = hx
            else:
                d = (0, 1 if dy >= 0 else -1)
                edge = hy
            # pitch-dependent stub length (beyond pad edge)
            fine = p['fpname'].startswith(('LQFP', 'TSSOP', 'SSOP', 'SOIC', 'SOT', 'SOP'))
            lmax = 1.0 if fine else 0.6
            lmin = 0.4 if fine else 0.3
            L.append(f'TSTUB {n} {lay} {cx:.4f} {cy:.4f} {d[0]} {d[1]} {hx:.4f} {hy:.4f} {lmin} {lmax} {key}')
        else:
            single = 1 if (p['type'] == 'thru_hole' and min(hx, hy) <= 1.6) or (smd and not big) else 0  # passives & small parts: no stub
            shape = f'C {cx:.4f} {cy:.4f} {hx:.4f}' if circ else f'R {cx:.4f} {cy:.4f} {hx:.4f} {hy:.4f}'
            L.append(f'TREG {n} {lay} {single} {shape} {key}')
    open(out, 'w', encoding='utf8').write('\n'.join(L) + '\n')
    print('wrote', out, len(L), 'lines;', len(nets), 'nets')


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else 'problem.txt')
