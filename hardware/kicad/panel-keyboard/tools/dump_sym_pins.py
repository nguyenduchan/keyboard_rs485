import re
import sys
from pathlib import Path

K = Path(r"C:\Users\duchan.nguyen\AppData\Local\Programs\KiCad\10.0\share\kicad\symbols")
lib, name = sys.argv[1].split(":", 1)
text = (K / f"{lib}.kicad_sym").read_text(encoding="utf-8")
idx = text.find(f"(symbol \"{name}_1_1\"")
chunk = text[idx : idx + 20000]
for block in re.finditer(r"\(pin\b", chunk):
    start = block.start()
    end = chunk.find("\n\t\t(pin", start + 4)
    b = chunk[start : end if end > start else start + 600]
    num = re.search(r'\(number "([^"]+)"', b)
    nm = re.search(r'\(name "([^"]+)"', b)
    if num:
        print(num.group(1), nm.group(1) if nm else "")
