import re
from pathlib import Path

text = Path(
    r"C:\Users\duchan.nguyen\AppData\Local\Programs\KiCad\10.0\share\kicad\symbols\MCU_ST_STM32F0.kicad_sym"
).read_text(encoding="utf-8")
idx = text.find('(symbol "STM32G030F6Px_1_1"')
end = text.find('(symbol "STM32G030F6Px_2_1"', idx)
chunk = text[idx:end]
for block in re.findall(r"\(pin\b", chunk):
    pass
for m in re.finditer(r'\(name "([^"]+)".*?\(number "([^"]+)"', chunk, re.DOTALL):
    name = m.group(1).split()[0]
    if name in {"PA2", "PA3", "PA4", "PA5", "PA6", "NRST", "VDD", "VSS", "PA13", "PA14"}:
        print(m.group(2), name)
