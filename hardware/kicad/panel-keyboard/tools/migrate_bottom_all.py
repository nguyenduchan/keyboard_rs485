"""Place all BOTTOM_POS footprints (one subprocess per ref)."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from zone_layout import BOTTOM_POS

PY = Path(
    r"C:\Users\duchan.nguyen\AppData\Local\Programs\KiCad\10.0\bin\python.exe"
)
TOOLS = Path(__file__).resolve().parent
ONE = TOOLS / "migrate_one_bottom_fp.py"


def main() -> int:
    failed: list[str] = []
    for ref in sorted(BOTTOM_POS):
        r = subprocess.run([str(PY), str(ONE), ref])
        if r.returncode != 0:
            failed.append(ref)
    if failed:
        print("FAILED:", ", ".join(failed))
        return 1
    print(f"OK {len(BOTTOM_POS)} bottom refs")
    return 0


if __name__ == "__main__":
    sys.exit(main())
