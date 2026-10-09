"""Generate JLCPCB assembly files and a cost estimate.

usage: python tools/make_jlc_files.py [boards=20]
  writes hardware/jlcpcb/bom_jlc.csv and hardware/jlcpcb/cpl_jlc.csv (top side SMT only)
  prints the part cost for the given quantity using live JLCPCB prices, MOQ and attrition.
"""
import csv
import json
import math
import sys
import urllib.request
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from jlc_lookup import API, kind  # noqa: E402
from jlc_parts import NOT_ASSEMBLED, PARTS  # noqa: E402
from pcb_model import load  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT.parents[1] / "jlcpcb"


def lookup(code):
    body = json.dumps({"keyword": code, "currentPage": 1, "pageSize": 5}).encode()
    req = urllib.request.Request(API, data=body, headers={"Content-Type": "application/json", "User-Agent": "Mozilla/5.0"})
    with urllib.request.urlopen(req, timeout=30) as r:
        lst = (json.load(r).get("data") or {}).get("componentPageInfo", {}).get("list") or []
    for c in lst:
        if c.get("componentCode") == code:
            return c
    return None


def price_at(c, qty):
    for t in c.get("componentPrices") or []:
        if t["startNumber"] <= qty and (t["endNumber"] == -1 or qty <= t["endNumber"]):
            return t["productPrice"]
    return (c.get("componentPrices") or [{}])[0].get("productPrice", 0)


def main():
    boards = int(sys.argv[1]) if len(sys.argv) > 1 else 20
    import os
    os.chdir(ROOT)
    t, fps, pads = load()
    smt = {ref: f for ref, f in fps.items() if f["layer"] == "F.Cu" and ref in PARTS}
    missing = [r for r, f in fps.items() if f["layer"] == "F.Cu" and r not in PARTS and r not in NOT_ASSEMBLED
               and not any(p["type"] == "thru_hole" for p in pads if p["ref"] == r)]
    if missing:
        print("WARNING: top-side SMT parts without LCSC code:", missing)
    OUT.mkdir(parents=True, exist_ok=True)

    groups = defaultdict(list)
    for ref in sorted(smt):
        groups[PARTS[ref]].append(ref)
    with open(OUT / "bom_jlc.csv", "w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh)
        w.writerow(["Comment", "Designator", "Footprint", "LCSC Part #"])
        for (code, comment), refs in sorted(groups.items(), key=lambda kv: kv[1][0]):
            w.writerow([comment, ",".join(refs), fps[refs[0]]["name"].split(":")[-1], code])
    with open(OUT / "cpl_jlc.csv", "w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh)
        w.writerow(["Designator", "Mid X", "Mid Y", "Layer", "Rotation"])
        for ref in sorted(smt):
            f = smt[ref]
            # KiCad Y grows downward; JLC expects Y up from the board origin (bottom-left of a 120 mm board)
            w.writerow([ref, f"{f['x']:.3f}mm", f"{120 - f['y']:.3f}mm", "Top", f"{f['a'] % 360:.0f}"])

    total, ext = 0.0, []
    print(f"{'LCSC':>10} {'type':9} {'qty':>4} {'order':>5} {'unit$':>7} {'cost$':>7}  part")
    for (code, comment), refs in sorted(groups.items(), key=lambda kv: kv[1][0]):
        c = lookup(code)
        if not c:
            print(f"{code:>10} NOT FOUND")
            continue
        need = len(refs) * boards
        order = max(need + (c.get("lossNumber") or 0), c.get("leastPatchNumber") or 0)
        unit = price_at(c, order)
        cost = unit * order
        total += cost
        k = kind(c)
        if k == "extended":
            ext.append(code)
        print(f"{code:>10} {k:9} {need:>4} {order:>5} {unit:>7.4f} {cost:>7.2f}  {comment} ({len(refs)}/bo) stock={c.get('stockCount')}")
    joints = sum(1 for p in pads if p["ref"] in smt and p["type"] == "smd" and p["layers"] and "F.Cu" in p["layers"])
    print(f"\nboards={boards}: SMT parts ${total:.2f}, extended parts {len(ext)} -> fee ${3 * len(ext)}, "
          f"joints {joints}/bo -> ${joints * boards * 0.0017:.2f}")
    print("wrote", OUT / "bom_jlc.csv", "and", OUT / "cpl_jlc.csv")


if __name__ == "__main__":
    main()
