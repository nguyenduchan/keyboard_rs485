#!/usr/bin/env python3
"""Obsolete. Do not run: it restores an old placement and draws copper.

Current board comes from build_schematic.py plus the text layout tools.
"""

from __future__ import annotations

import re
import uuid
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
NETLIST = ROOT / "panel-keyboard.net"
SCH = ROOT / "panel-keyboard.kicad_sch"
PCB = ROOT / "panel-keyboard.kicad_pcb"

# Bottom-side placement (mm), origin bottom-left per PCB_LAYOUT.md
PLACEMENT: dict[str, tuple[float, float, int]] = {
    "J_PWR": (19.0, 8.0, 0),
    "J485": (80.0, 8.0, 0),
    "F1": (28.0, 12.0, 0),
    "C15": (34.0, 12.0, 0),
    "TVS1": (34.0, 18.0, 90),
    "Q17": (40.0, 12.0, 0),
    "R17": (40.0, 18.0, 0),
    "C4": (46.0, 12.0, 0),
    "C14": (46.0, 18.0, 0),
    "NT1": (52.0, 15.0, 0),
    "C16": (58.0, 12.0, 0),
    "C17": (58.0, 18.0, 0),
    "U4": (22.0, 132.0, 0),
    "D8": (28.0, 128.0, 90),
    "L1": (34.0, 132.0, 90),
    "C2": (40.0, 128.0, 0),
    "C5": (46.0, 128.0, 0),
    "R36": (52.0, 132.0, 0),
    "R37": (52.0, 138.0, 0),
    "R38": (58.0, 132.0, 0),
    "FB5V": (62.0, 128.0, 0),
    "U5": (15.0, 125.0, 0),
    "C7": (18.0, 132.0, 0),
    "C3": (12.0, 128.0, 0),
    "C18": (12.0, 134.0, 0),
    "FB33": (72.0, 125.0, 0),
    "CVDDA": (96.0, 125.0, 0),
    "U1": (15.0, 118.0, 0),
    "CVDD1": (10.0, 115.0, 0),
    "RNST": (22.0, 115.0, 0),
    "CNRST": (28.0, 115.0, 0),
    "J2": (8.0, 118.0, 90),
    "U3": (58.0, 135.0, 0),
    "R47A": (52.0, 132.0, 0),
    "R47B": (52.0, 138.0, 0),
    "RDE": (64.0, 132.0, 0),
    "CM3": (64.0, 138.0, 0),
    "L485": (70.0, 135.0, 0),
    "RBIAS_A": (76.0, 138.0, 0),
    "RBIAS_B": (76.0, 132.0, 0),
    "RTERM": (82.0, 135.0, 0),
    "JTERM": (86.0, 135.0, 0),
    "TVS2": (88.0, 10.0, 90),
    "DISP1": (39.0, 100.5, 0),
    "DISP2": (39.0, 70.5, 0),
}

FOOTPRINTS: dict[str, str] = {
    "J_PWR": "TerminalBlock_Phoenix:TerminalBlock_Phoenix_MC_1,5-2-5.08-G",
    "J485": "TerminalBlock_Phoenix:TerminalBlock_Phoenix_MC_1,5-3-5.08-G",
    "F1": "Fuse:Fuse_2920_7451Metric",
    "C15": "Capacitor_SMD:C_0805_2012Metric",
    "TVS1": "Diode_SMD:D_SMA",
    "Q17": "Package_TO_SOT_SMD:SOT-23",
    "R17": "Resistor_SMD:R_0805_2012Metric",
    "C4": "Capacitor_SMD:CP_Elec_10x10.5",
    "C14": "Capacitor_SMD:C_0805_2012Metric",
    "NT1": "NetTie:NetTie-2_SMD_Pad2.0mm",
    "C16": "Capacitor_SMD:C_0805_2012Metric",
    "C17": "Capacitor_SMD:C_0805_2012Metric",
    "D8": "Diode_SMD:D_SOD-123",
    "R36": "Resistor_SMD:R_0805_2012Metric",
    "R37": "Resistor_SMD:R_0805_2012Metric",
    "R38": "Resistor_SMD:R_0805_2012Metric",
    "TVS2": "Package_TO_SOT_SMD:SOT-23-3",
    "FB5V": "Inductor_SMD:L_0805_2012Metric",
    "FB33": "Inductor_SMD:L_0805_2012Metric",
    "U4": "Package_TO_SOT_SMD:TO-263-5_TabPin3",
    "L1": "Inductor_SMD:L_1210_3225Metric",
    "C2": "Capacitor_SMD:C_1206_3216Metric",
    "C5": "Capacitor_SMD:C_0805_2012Metric",
    "C7": "Capacitor_SMD:C_0805_2012Metric",
    "C3": "Capacitor_SMD:C_0805_2012Metric",
    "C18": "Capacitor_SMD:C_0805_2012Metric",
    "U5": "Package_TO_SOT_SMD:SOT-223-3_TabPin2",
    "CVDDA": "Capacitor_SMD:C_0805_2012Metric",
    "U1": "Package_SO:TSSOP-20_4.4x6.5mm_P0.65mm",
    "CVDD1": "Capacitor_SMD:C_0805_2012Metric",
    "RNST": "Resistor_SMD:R_0805_2012Metric",
    "CNRST": "Capacitor_SMD:C_0805_2012Metric",
    "J2": "Connector_PinHeader_1.27mm:PinHeader_1x04_P1.27mm_Vertical",
    "U3": "Package_SO:SOIC-8_3.9x4.9mm_P1.27mm",
    "R47A": "Resistor_SMD:R_0805_2012Metric",
    "R47B": "Resistor_SMD:R_0805_2012Metric",
    "RDE": "Resistor_SMD:R_0805_2012Metric",
    "CM3": "Capacitor_SMD:C_0805_2012Metric",
    "L485": "Inductor_SMD:DLW21SN900SQ2L",
    "RBIAS_A": "Resistor_SMD:R_0805_2012Metric",
    "RBIAS_B": "Resistor_SMD:R_0805_2012Metric",
    "RTERM": "Resistor_SMD:R_0805_2012Metric",
    "JTERM": "Jumper:SolderJumper-2_P1.3mm_Open_RoundedPad1.0x1.5mm",
    "DISP1": "Connector_PinHeader_2.54mm:PinHeader_1x12_P2.54mm_Vertical",
    "DISP2": "Connector_PinHeader_2.54mm:PinHeader_1x12_P2.54mm_Vertical",
    "SW": "Button_Switch_SMD:SW_SPST_Tactile_12x12mm",
    "RLED": "Resistor_SMD:R_0805_2012Metric",
}


def parse_netlist(path: Path) -> dict[tuple[str, str], str]:
    text = path.read_text(encoding="utf-8")
    net_map: dict[tuple[str, str], str] = {}
    for net_block in re.findall(r"\(net\b.*?\)\s*\)", text, re.DOTALL):
        name_m = re.search(r'\(name "([^"]+)"', net_block)
        if not name_m:
            continue
        name = name_m.group(1)
        for ref_m, pin_m in zip(
            re.findall(r'\(ref "([^"]+)"', net_block),
            re.findall(r'\(pin "([^"]+)"', net_block),
        ):
            net_map[(ref_m, pin_m)] = name
    return net_map


def parse_footprints_from_sch(path: Path) -> dict[str, str]:
    text = path.read_text(encoding="utf-8")
    fps: dict[str, str] = {}
    for block in re.findall(r'\(symbol\b.*?\n\t\)', text, re.DOTALL):
        ref_m = re.search(r'\(property "Reference" "([^"]+)"', block)
        fp_m = re.search(r'\(property "Footprint" "([^"]+)"', block)
        if ref_m and fp_m and fp_m.group(1):
            fps[ref_m.group(1)] = fp_m.group(1)
    return fps


def uid() -> str:
    return str(uuid.uuid4())


def load_pcb_keepouts(pcb_text: str) -> str:
    """Keep outline, comments, mounting holes, top LEDs; drop old bottom-only graphics."""
    lines = pcb_text.splitlines()
    out: list[str] = []
    skip = False
    for line in lines:
        if line.strip().startswith("(footprint "):
            fp = line.strip()
            if "MountingHole" in fp or "LED_0805" in fp:
                skip = False
            else:
                skip = True
        if skip:
            if line.strip() == ")":
                skip = False
            continue
        if "(zone" in line and "B.Cu" in pcb_text:
            continue
        out.append(line)
    text = "\n".join(out)
    text = re.sub(r"\(zone\b.*?\n\t\)", "", text, flags=re.DOTALL)
    return text


def simple_fp_block(ref: str, fp: str, x: float, y: float, rot: int, layer: str = "B.Cu") -> str:
    return f"""\t(footprint "{fp}"
\t\t(layer "{layer}")
\t\t(uuid "{uid()}")
\t\t(at {x} {y} {rot})
\t\t(property "Reference" "{ref}" (at 0 0 0) (layer "B.SilkS") (effects (font (size 1 1) (thickness 0.15))))
\t\t(property "Value" "{ref}" (at 0 0 0) (layer "B.Fab") (effects (font (size 1 1) (thickness 0.15)) hide))
\t)"""


def main() -> None:
    net_map = parse_netlist(NETLIST)
    sch_fps = parse_footprints_from_sch(SCH)
    pcb_text = PCB.read_text(encoding="utf-8")

    # keypad + status LED positions
    for i in range(1, 17):
        row, col = (i - 1) // 4, (i - 1) % 4
        PLACEMENT[f"SW{i}"] = (12.0 + col * 16.0, 22.0 + row * 16.0, 0)
    led_y = [77.5, 68.5, 59.5, 50.5, 41.5, 32.5, 23.5, 14.5]
    for i, y in enumerate(led_y):
        PLACEMENT[f"D{10 + i}"] = (88.0, y, 0)
        PLACEMENT[f"RLED{i + 1}"] = (85.0, y, 0)

    new_fps: list[str] = []
    for ref, (x, y, rot) in PLACEMENT.items():
        if ref.startswith("SW"):
            fp = FOOTPRINTS["SW"]
        elif ref.startswith("D1") and ref != "D1":
            fp = "Panel:LED_0805_Status"
            rot_layer = ("F.Cu", rot)
        elif ref.startswith("RLED"):
            fp = FOOTPRINTS["RLED"]
            rot_layer = ("B.Cu", rot)
        else:
            fp = sch_fps.get(ref) or FOOTPRINTS.get(ref, "Resistor_SMD:R_0805_2012Metric")
            rot_layer = ("B.Cu", rot)
        layer, r = rot_layer if ref.startswith("D1") and ref != "D1" else ("B.Cu", rot)
        if ref.startswith("D1") and ref != "D1":
            layer = "F.Cu"
        new_fps.append(simple_fp_block(ref, fp, x, y, r, layer))

    # nets list for pcb header
    nets = sorted({n for n in net_map.values() if n})
    net_lines = ['\t(net 0 "")']
    net_name_to_id = {"": 0}
    for i, name in enumerate(nets, start=1):
        net_name_to_id[name] = i
        esc = name.replace('"', '\\"')
        net_lines.append(f'\t(net {i} "{esc}")')

    # Placement only — copper routes and pours are added later in KiCad.
    tracks: list[str] = []

    insert_at = pcb_text.rfind("(zone")
    if insert_at < 0:
        insert_at = pcb_text.rfind(")")
    body = pcb_text[:insert_at]
    if "(net 0" not in body:
        body = re.sub(
            r"\(net 0[^\n]*\n(?:\t\(net[^\n]*\n)*",
            "\n".join(net_lines) + "\n",
            body,
            count=1,
        )

    out = body + "\n" + "\n".join(new_fps) + "\n" + "\n".join(tracks) + "\n)\n"
    PCB.write_text(out, encoding="utf-8")
    print(f"Updated {PCB} with {len(new_fps)} footprints, {len(nets)} nets")


if __name__ == "__main__":
    main()
