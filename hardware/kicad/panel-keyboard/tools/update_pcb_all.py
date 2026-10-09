"""Schematic → PCB: zones, front THT, bottom SMD only."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from zone_layout import BOTTOM_POS, FRONT_POS

PY = Path(
    r"C:\Users\duchan.nguyen\AppData\Local\Programs\KiCad\10.0\bin\python.exe"
)
TOOLS = Path(__file__).resolve().parent

OBSOLETE = (
    "FB24",
    "FB33",
    "CIN_H",
    "CIN_L",
    "COUT_H",
    "C5V1",
    "C5V2",
    "C3V1",
    "C3V2",
    "D1",
    "DISP1",
    "DISP2",
    "RBOT0",
    "U2",
    "R33A",
    "R33B",
    "R33C",
    "RPU",
    "CTM1",
    "CTM2",
    "J2",
)


def run(cmd: list[str], required: bool = True) -> bool:
    print("+", " ".join(cmd))
    r = subprocess.run(cmd)
    if r.returncode != 0:
        msg = f"exit {r.returncode}"
        if required:
            print("FAILED (required):", msg)
            return False
        print("WARN (skipped):", msg)
    return True


def main() -> int:
    if not run([sys.executable, str(TOOLS / "build_schematic.py")]):
        return 1
    run([sys.executable, str(TOOLS / "remove_fp_refs.py"), *OBSOLETE], required=False)
    failed: list[str] = []
    for ref in sorted(set(BOTTOM_POS) | set(FRONT_POS)):
        if not run([str(PY), str(TOOLS / "add_one_fp.py"), ref], required=False):
            failed.append(ref)
    run([str(PY), str(TOOLS / "assign_nets.py")], required=False)
    run([str(PY), str(TOOLS / "migrate_front_tht.py")], required=False)
    run([str(PY), str(TOOLS / "layout_bottom_batch.py")], required=False)
    run([sys.executable, str(TOOLS / "apply_layout_positions.py")], required=False)
    run([str(PY), str(TOOLS / "fix_front_layout.py")], required=False)
    run([sys.executable, str(TOOLS / "verify_bottom_layout.py")], required=False)
    run([sys.executable, str(TOOLS / "verify_front_layout.py")], required=False)
    if failed:
        print(f"Footprint sync failed for {len(failed)} refs (đóng KiCad, chạy lại):", ", ".join(failed[:12]), "...")
        return 1
    print("Done. Reload PCB in KiCad (File → Revert if already open).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
