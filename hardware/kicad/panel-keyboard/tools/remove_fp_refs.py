"""Remove footprint blocks from .kicad_pcb by reference (no pcbnew Remove)."""

from __future__ import annotations

import re
import sys
from pathlib import Path

BOARD = Path(__file__).resolve().parents[1] / "panel-keyboard.kicad_pcb"


def remove_refs(text: str, refs: set[str]) -> tuple[str, list[str]]:
    removed: list[str] = []
    out: list[str] = []
    i = 0
    lines = text.splitlines(keepends=True)
    while i < len(lines):
        line = lines[i]
        if line.lstrip().startswith("(footprint "):
            start = i
            depth = 0
            block_lines: list[str] = []
            while i < len(lines):
                block_lines.append(lines[i])
                depth += lines[i].count("(") - lines[i].count(")")
                i += 1
                if depth <= 0 and block_lines:
                    break
            block = "".join(block_lines)
            m = re.search(r'\(property "Reference" "([^"]+)"', block)
            if m and m.group(1) in refs:
                removed.append(m.group(1))
                continue
            out.extend(block_lines)
            continue
        out.append(line)
        i += 1
    return "".join(out), removed


def main() -> int:
    refs = set(sys.argv[1:]) if len(sys.argv) > 1 else set()
    if not refs:
        print("usage: remove_fp_refs.py REF ...")
        return 2
    text = BOARD.read_text(encoding="utf-8")
    new_text, removed = remove_refs(text, refs)
    BOARD.write_text(new_text, encoding="utf-8")
    print("removed:", ", ".join(removed))
    return 0


if __name__ == "__main__":
    sys.exit(main())
