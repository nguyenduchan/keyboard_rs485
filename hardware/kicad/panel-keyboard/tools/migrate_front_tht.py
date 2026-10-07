"""Migrate all front-panel refs to THT footprints (one subprocess per ref)."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from zone_layout import FRONT_POS

PY = Path(
    r"C:\Users\duchan.nguyen\AppData\Local\Programs\KiCad\10.0\bin\python.exe"
)
TOOLS = Path(__file__).resolve().parent
MIGRATE = TOOLS / "migrate_one_front_fp.py"


def main() -> int:
    failed: list[str] = []
    for ref in sorted(FRONT_POS):
        r = subprocess.run([str(PY), str(MIGRATE), ref])
        if r.returncode != 0:
            failed.append(ref)
    if failed:
        print("FAILED:", ", ".join(failed))
        return 1
    print("All front THT footprints OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
