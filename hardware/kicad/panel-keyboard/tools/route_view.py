"""Render pads + routing result to PNG.  usage: route_view.py result.txt out.png [x0 y0 x1 y1] [--nolabel]"""
import sys

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, Circle

from pcb_model import load
from route_export import pad_geom


def main():
    res, out = sys.argv[1], sys.argv[2]
    box = [float(v) for v in sys.argv[3:7]] if len(sys.argv) >= 7 else [0, 0, 120, 120]
    t, fps, pads = load()
    fig = plt.figure(figsize=(16, 16 * (box[3] - box[1]) / (box[2] - box[0])), dpi=110)
    ax = fig.add_axes([0.02, 0.02, 0.96, 0.96])
    ax.set_xlim(box[0], box[2]); ax.set_ylim(box[3], box[1]); ax.set_aspect('equal')
    ax.set_facecolor('#101418')
    ax.add_patch(Rectangle((0, 0), 120, 120, fill=False, ec='#ffd400', lw=1))
    for p in pads:
        cx, cy, hx, hy, circ = pad_geom(p)
        top = 'F.Cu' in p['layers'] or '*.Cu' in p['layers']
        thr = p['type'] == 'thru_hole'
        col = '#ff6a6a' if top else '#5a8dff'
        if thr:
            col = '#c8a03c'
        if circ:
            ax.add_patch(Circle((cx, cy), hx, fc=col, ec='none', alpha=0.55))
        else:
            ax.add_patch(Rectangle((cx - hx, cy - hy), 2 * hx, 2 * hy, fc=col, ec='none', alpha=0.55))
        if p['drill']:
            ax.add_patch(Circle((cx, cy), p['drill'] / 2, fc='#101418', ec='none'))
        if '--nolabel' not in sys.argv and box[0] - 2 < cx < box[2] + 2 and box[1] - 2 < cy < box[3] + 2:
            ax.text(cx, cy, f"{p['net']}", fontsize=3.2, color='white', ha='center', va='center')
    for ref, f in fps.items():
        if box[0] <= f['x'] <= box[2] and box[1] <= f['y'] <= box[3]:
            ax.text(f['x'], f['y'], ref, fontsize=5, color='#9fe0ff', ha='center', va='center')
    scale = 16 * 72 / (box[2] - box[0]) * 0.96   # points per mm
    for ln in open(res):
        f = ln.split()
        if not f:
            continue
        if f[0] == 'SEG':
            layer = int(f[2]); x1, y1, x2, y2, w = map(float, f[3:8])
            ax.plot([x1, x2], [y1, y2], color='#e03030' if layer == 0 else '#3070ff', lw=w * scale,
                    solid_capstyle='round', alpha=0.75, zorder=3 if layer == 0 else 2)
        elif f[0] == 'VIA':
            x, y = float(f[2]), float(f[3])
            ax.add_patch(Circle((x, y), 0.3, fc='#dddddd', ec='black', lw=0.3, zorder=5))
            ax.add_patch(Circle((x, y), 0.15, fc='black', ec='none', zorder=6))
    ax.tick_params(colors='white', labelsize=6)
    fig.patch.set_facecolor('#101418')
    fig.savefig(out)


if __name__ == '__main__':
    main()
