#!/usr/bin/env python3
"""Generate panel-keyboard.kicad_sch from SO_DO_NGUYEN_LY.md connectivity."""

from __future__ import annotations

import os
import re
import uuid
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "panel-keyboard.kicad_sch"
KICAD_SYM = Path(
    os.environ.get(
        "KICAD_SYMBOL_DIR",
        r"C:\Users\duchan.nguyen\AppData\Local\Programs\KiCad\10.0\share\kicad\symbols",
    )
)


def new_uuid() -> str:
    return str(uuid.uuid4())


class SchematicBuilder:
    def __init__(self) -> None:
        self.symbols: list[str] = []
        self.wires: list[str] = []
        self.labels: list[str] = []
        self.global_labels: list[str] = []
        self.no_connects: list[str] = []
        self._pin_pos: dict[tuple[str, str], tuple[float, float, int]] = {}
        self.connectivity: list[tuple[str, str, str]] = []

    def place(
        self,
        lib_id: str,
        ref: str,
        value: str,
        x: float,
        y: float,
        rot: int = 0,
        footprint: str = "",
        unit: int = 1,
    ) -> None:
        sym_uuid = new_uuid()
        pins = self._symbol_pins(lib_id, unit)
        pin_uuids: dict[str, str] = {}
        pin_lines: list[str] = []
        for pin_num, (px, py, pr) in pins.items():
            pu = new_uuid()
            pin_uuids[pin_num] = pu
            pin_lines.append(f'\t\t(pin "{pin_num}"\n\t\t\t(uuid "{pu}")\n\t\t)')
            ax, ay, _pr = self._abs_pin(x, y, rot, px, py, pr)
            self._pin_pos[(ref, pin_num)] = (ax, ay, rot)

        fp_prop = (
            f'\n\t\t(property "Footprint" "{footprint}"\n'
            f"\t\t\t(at {x} {y} 0)\n\t\t\t(hide yes)\n"
            f"\t\t\t(show_name no)\n\t\t\t(do_not_autoplace no)\n"
            f"\t\t\t(effects (font (size 1.27 1.27)))\n\t\t)"
            if footprint
            else '\n\t\t(property "Footprint" ""\n\t\t\t(at '
            + f"{x} {y} 0)\n\t\t\t(hide yes)\n\t\t\t(show_name no)\n"
            + "\t\t\t(do_not_autoplace no)\n\t\t\t(effects (font (size 1.27 1.27)))\n\t\t)"
        )

        self.symbols.append(
            f"""\t(symbol
\t\t(lib_id "{lib_id}")
\t\t(at {x} {y} {rot})
\t\t(unit {unit})
\t\t(body_style 1)
\t\t(exclude_from_sim no)
\t\t(in_bom yes)
\t\t(on_board yes)
\t\t(in_pos_files yes)
\t\t(dnp no)
\t\t(uuid "{sym_uuid}")
\t\t(property "Reference" "{ref}"
\t\t\t(at {x + 2.54} {y} 0)
\t\t\t(show_name no)
\t\t\t(do_not_autoplace no)
\t\t\t(effects (font (size 1.27 1.27)) (justify left))
\t\t)
\t\t(property "Value" "{value}"
\t\t\t(at {x} {y + 2.54} 0)
\t\t\t(show_name no)
\t\t\t(do_not_autoplace no)
\t\t\t(effects (font (size 1.27 1.27)))
\t\t){fp_prop}
\t\t(property "Datasheet" ""
\t\t\t(at {x} {y} 0)
\t\t\t(show_name no)
\t\t\t(do_not_autoplace no)
\t\t\t(effects (font (size 1.27 1.27)))
\t\t)
{chr(10).join(pin_lines)}
\t)"""
        )

    def _abs_pin(
        self, sx: float, sy: float, rot: int, px: float, py: float, pin_rot: int
    ) -> tuple[float, float, int]:
        total = (rot + pin_rot) % 360
        if total == 0:
            return sx + px, sy + py, total
        if total == 90:
            return sx - py, sy + px, total
        if total == 180:
            return sx - px, sy - py, total
        if total == 270:
            return sx + py, sy - px, total
        return sx + px, sy + py, total

    def _symbol_pins(self, lib_id: str, unit: int) -> dict[str, tuple[float, float, int]]:
        lib, name = lib_id.split(":", 1)
        if lib == "Panel":
            path = ROOT / "symbols" / "Panel.kicad_sym"
        else:
            path = KICAD_SYM / f"{lib}.kicad_sym"
        text = path.read_text(encoding="utf-8")
        for _ in range(6):
            head = text.find(f'(symbol "{name}"')
            if head < 0:
                break
            ext = re.search(r'\(extends "([^"]+)"\)', text[head : head + 2500])
            if not ext:
                break
            name = ext.group(1)
        sub = f"{name}_1_{unit}"
        idx = text.find(f'(symbol "{sub}"')
        if idx < 0:
            idx = text.find(f'(symbol "{name}_0_1"')
        if idx < 0:
            raise KeyError(f"symbol pins not found: {lib_id}")
        chunk = text[idx : idx + 400000]
        pins: dict[str, tuple[float, float, int]] = {}
        for m in re.finditer(r"\(pin\b", chunk):
            start = m.start()
            end = chunk.find("\n\t\t(pin", start + 4)
            if end < 0:
                end = chunk.find("\n\t)", start)
            block = chunk[start:end if end > start else start + 800]
            num_m = re.search(r'\(number "([^"]+)"', block)
            at_m = re.search(r"\(at\s+([\d.-]+)\s+([\d.-]+)\s+(\d+)", block)
            if num_m and at_m:
                pins[num_m.group(1)] = (
                    float(at_m.group(1)),
                    float(at_m.group(2)),
                    int(at_m.group(3)),
                )
        return pins

    def net_label(self, ref: str, pin: str, net: str, stub: float = 3.81) -> None:
        if (ref, pin) not in self._pin_pos:
            return
        self.connectivity.append((ref, pin, net))
        x, y, rot = self._pin_pos[(ref, pin)]
        if rot in (0, 180):
            x2, y2 = x + stub, y
            lx, ly = x + stub / 2, y
        else:
            x2, y2 = x, y + stub
            lx, ly = x, y + stub / 2
        wu = new_uuid()
        lu = new_uuid()
        self.wires.append(
            f"""\t(wire
\t\t(pts (xy {x:.4f} {y:.4f}) (xy {x2:.4f} {y2:.4f}))
\t\t(stroke (width 0) (type default))
\t\t(uuid "{wu}")
\t)"""
        )
        self.labels.append(
            f"""\t(label "{net}"
\t\t(at {lx:.4f} {ly:.4f} 0)
\t\t(effects (font (size 1.27 1.27)) (justify left bottom))
\t\t(uuid "{lu}")
\t)"""
        )

    def global_net(self, net: str, x: float, y: float, shape: str = "passive") -> None:
        gu = new_uuid()
        self.global_labels.append(
            f"""\t(global_label "{net}"
\t\t(shape {shape})
\t\t(at {x} {y} 0)
\t\t(fields_autoplaced yes)
\t\t(effects (font (size 1.27 1.27)) (justify left))
\t\t(uuid "{gu}")
\t)"""
        )

    def no_connect(self, ref: str, pin: str) -> None:
        if (ref, pin) not in self._pin_pos:
            return
        x, y, _rot = self._pin_pos[(ref, pin)]
        self.no_connects.append(
            f"""\t(no_connect
\t\t(at {x:.4f} {y:.4f})
\t\t(uuid "{new_uuid()}")
\t)"""
        )

    def power_flag(self, lib_id: str, x: float, y: float) -> None:
        self.place(lib_id, f"#PWR{len(self.symbols)}", lib_id.split(":")[-1], x, y)

    def build(self) -> str:
        header = """(kicad_sch
\t(version 20260306)
\t(generator "build_schematic.py")
\t(generator_version "1.0")
\t(uuid "e1c0e0c0-0000-4000-8000-000000000001")
\t(paper "A2")
\t(title_block
\t\t(title "Panel Keyboard All-in-One 120x120")
\t\t(date "2026-10-07")
\t\t(rev "B")
\t\t(company "Embedded Keyboard Project")
\t\t(comment 1 "Full schematic — SO_DO_NGUYEN_LY.md")
\t\t(comment 2 "Power block above MCU; J_PWR and J485 on the right edge")
\t\t(comment 3 "Regenerate: python tools/build_schematic.py")
\t)
\t(lib_symbols)
"""
        footer = """
\t(sheet_instances
\t\t(path "/"
\t\t\t(page "1")
\t\t)
\t)
)
"""
        return (
            header
            + "\n".join(self.symbols)
            + "\n"
            + "\n".join(self.wires)
            + "\n"
            + "\n".join(self.labels)
            + "\n"
            + "\n".join(self.global_labels)
            + "\n"
            + "\n".join(self.no_connects)
            + footer
        )


def main() -> None:
    b = SchematicBuilder()

    # --- PWR_24V_PROTECT + PWR_BUCK (fiberboard PWR_24V_PROTECT_BUCK_GUIDE.txt, level 1) ---
    y0 = 40.0
    b.place("Connector_Generic:Conn_01x02", "J_PWR", "2EDGVC-5.08-02P", 25, y0, footprint="Panel:TerminalBlock_2P_5.08")
    b.place("Device:Fuse", "F1", "MF-MSMF350", 40, y0, footprint="Fuse:Fuse_2920_7451Metric")
    b.place(
        "Device:Filter_EMI_CommonMode",
        "L2",
        "ACM2012-201",
        55,
        y0,
        footprint="Inductor_SMD:L_CommonModeChoke_Coilank_ACM2012",
    )
    b.place("Device:Varistor", "RV1", "14D390K", 55, y0 + 16, footprint="Varistor:Varistor_Panasonic_VF")
    b.place("Device:D_TVS", "TVS1", "SMAJ28A", 70, y0 + 16, footprint="Diode_SMD:D_SMA")
    b.place("Device:C", "C19", "10uF", 70, y0, footprint="Capacitor_SMD:C_0805_2012Metric")
    b.place("Device:C", "C15", "100nF", 70, y0 + 28, footprint="Capacitor_SMD:C_0805_2012Metric")
    b.place("Transistor_FET:Q_PMOS_GSD", "Q17", "AO4407A", 85, y0, footprint="Package_TO_SOT_SMD:SOT-23")
    b.place("Device:R", "R17", "100k", 85, y0 + 18, footprint="Resistor_SMD:R_0805_2012Metric")
    b.place("Device:C", "C4", "1000uF", 100, y0, footprint="Capacitor_SMD:CP_Elec_10x12.5")
    b.place(
        "Connector_Generic:Conn_01x02",
        "NT1",
        "GND_STAR",
        100,
        y0 + 30,
        footprint="NetTie:NetTie-2_SMD_Pad2.0mm",
    )
    b.place("Regulator_Switching:XL1509-5.0", "U4", "XL1509-5.0", 118, y0, footprint="Package_TO_SOT_SMD:TO-263-5_TabPin3")
    b.place("Device:C", "C16", "10uF", 106, y0, footprint="Capacitor_SMD:C_0805_2012Metric")
    b.place("Device:C", "C17", "100nF", 106, y0 + 14, footprint="Capacitor_SMD:C_0805_2012Metric")
    b.place("Device:D", "D8", "SS34", 132, y0 + 14, footprint="Diode_SMD:D_SOD-123")
    b.place("Device:L", "L1", "47uH", 146, y0, footprint="Inductor_SMD:L_1210_3225Metric")
    b.place("Device:C", "C2", "220uF", 162, y0, footprint="Capacitor_SMD:CP_Elec_6.3x5.4")
    b.place("Device:C", "C5", "100nF", 162, y0 + 14, footprint="Capacitor_SMD:C_0805_2012Metric")
    b.place("Device:R", "R36", "3k", 132, y0 + 28, footprint="Resistor_SMD:R_0805_2012Metric")
    b.place("Device:R", "R37", "1k", 118, y0 + 28, footprint="Resistor_SMD:R_0805_2012Metric")
    b.place("Device:R", "R38", "10k", 118, y0 + 14, footprint="Resistor_SMD:R_0805_2012Metric")
    b.place("Device:FerriteBead", "FB2", "BLM21PG121", 162, y0 - 12, footprint="Inductor_SMD:L_0805_2012Metric")
    b.place("Device:C", "C22", "220uF", 178, y0, footprint="Capacitor_SMD:CP_Elec_6.3x5.4")
    b.place("Device:FerriteBead", "FB5V", "BLM21PG121", 178, y0 - 12, footprint="Inductor_SMD:L_0805_2012Metric")

    b.place("Regulator_Linear:AMS1117-3.3", "U5", "AMS1117-3.3", 194, y0, footprint="Package_TO_SOT_SMD:SOT-223-3_TabPin2")
    b.place("Device:C", "C7", "10uF", 188, y0 + 14, footprint="Capacitor_SMD:C_0805_2012Metric")
    b.place("Device:C", "C3", "22uF", 210, y0, footprint="Capacitor_SMD:C_0805_2012Metric")
    b.place("Device:C", "C18", "100nF", 210, y0 + 14, footprint="Capacitor_SMD:C_0805_2012Metric")
    b.place("Device:FerriteBead", "FB1", "BLM21PG121", 210, y0 - 12, footprint="Inductor_SMD:L_0805_2012Metric")

    b.power_flag("power:+24V", 100, y0 - 10)
    b.power_flag("power:GND", 110, y0 + 40)
    b.power_flag("power:+5V", 178, y0 - 25)
    b.power_flag("power:+3V3", 225, y0 - 25)

    # Nets — section 4 of PWR_24V_PROTECT_BUCK_GUIDE.txt
    b.net_label("J_PWR", "1", "+24V_IN")
    b.net_label("J_PWR", "2", "GND_PWR")
    b.net_label("F1", "1", "+24V_IN")
    b.net_label("F1", "2", "+24V_FUSE")
    b.net_label("L2", "1", "+24V_FUSE")
    b.net_label("L2", "2", "VIN_PROTECT")
    b.net_label("L2", "3", "GND_PWR")
    b.net_label("L2", "4", "GND_PWR")
    b.net_label("RV1", "1", "VIN_PROTECT")
    b.net_label("RV1", "2", "GND_PWR")
    b.net_label("TVS1", "1", "VIN_PROTECT")
    b.net_label("TVS1", "2", "GND_PWR")
    b.net_label("C19", "1", "VIN_PROTECT")
    b.net_label("C19", "2", "GND_PWR")
    b.net_label("C15", "1", "VIN_PROTECT")
    b.net_label("C15", "2", "GND_PWR")
    b.net_label("Q17", "2", "VIN_PROTECT")
    b.net_label("Q17", "1", "FET_G")
    b.net_label("R17", "1", "FET_G")
    b.net_label("R17", "2", "GND_PWR")
    b.net_label("Q17", "3", "+24V")
    b.net_label("C4", "1", "+24V")
    b.net_label("C4", "2", "GND_PWR")
    b.net_label("NT1", "1", "GND_PWR")
    b.net_label("NT1", "2", "GND")
    b.net_label("U4", "1", "+24V")
    b.net_label("U4", "2", "+5V_SW")
    b.net_label("U4", "3", "BUCK_FB")
    b.net_label("U4", "4", "BUCK_EN")
    b.net_label("U4", "5", "GND_PWR")
    b.net_label("C16", "1", "+24V")
    b.net_label("C16", "2", "GND_PWR")
    b.net_label("C17", "1", "+24V")
    b.net_label("C17", "2", "GND_PWR")
    b.net_label("D8", "1", "+5V_SW")
    b.net_label("D8", "2", "GND_PWR")
    b.net_label("L1", "1", "+5V_SW")
    b.net_label("L1", "2", "+5V_RAW")
    b.net_label("C2", "1", "+5V_RAW")
    b.net_label("C2", "2", "GND")
    b.net_label("C5", "1", "+5V_RAW")
    b.net_label("C5", "2", "GND")
    b.net_label("FB2", "1", "+5V_RAW")
    b.net_label("FB2", "2", "+5V")
    b.net_label("C22", "1", "+5V")
    b.net_label("C22", "2", "GND")
    b.net_label("R36", "1", "+5V")
    b.net_label("R36", "2", "BUCK_FB")
    b.net_label("R37", "1", "BUCK_FB")
    b.net_label("R37", "2", "GND_PWR")
    b.net_label("R38", "1", "+24V")
    b.net_label("R38", "2", "BUCK_EN")
    b.net_label("FB5V", "1", "+5V")
    b.net_label("FB5V", "2", "+5V_LED")
    b.net_label("U5", "1", "GND")
    b.net_label("U5", "2", "+3V3_LDO")
    b.net_label("U5", "3", "+5V")
    b.net_label("C7", "1", "+5V")
    b.net_label("C7", "2", "GND")
    b.net_label("C3", "1", "+3V3_LDO")
    b.net_label("C3", "2", "GND")
    b.net_label("C18", "1", "+3V3_LDO")
    b.net_label("C18", "2", "GND")
    b.net_label("FB1", "1", "+3V3_LDO")
    b.net_label("FB1", "2", "+3V3")

    # MCU + RS485 (center)
    my = 90.0
    # STM32G030C8T6 LQFP-48: đủ GPIO để tự quét 7-seg, LED trạng thái và phím.
    b.place(
        "MCU_ST_STM32G0:STM32G030C8Tx",
        "U1",
        "STM32G030C8T6",
        80,
        my,
        footprint="Package_QFP:LQFP-48_7x7mm_P0.5mm",
    )
    b.place("Device:C", "CVDD1", "100n", 65, my + 20, footprint="Capacitor_SMD:C_0805_2012Metric")
    b.place("Device:R", "RNST", "10k", 65, my - 15, footprint="Resistor_SMD:R_0805_2012Metric")
    b.place("Device:C", "CNRST", "100n", 75, my - 15, footprint="Capacitor_SMD:C_0805_2012Metric")
    b.place("Connector_Generic:Conn_01x04", "J2", "SWD_1.27", 50, my, footprint="Connector_PinHeader_1.27mm:PinHeader_1x04_P1.27mm_Vertical")

    b.place("Interface_UART:MAX485E", "U3", "MAX485CSA", 140, my, footprint="Package_SO:SOIC-8_3.9x4.9mm_P1.27mm")
    b.place("Device:R", "R47A", "47", 120, my + 12, footprint="Resistor_SMD:R_0805_2012Metric")
    b.place("Device:R", "R47B", "47", 120, my - 12, footprint="Resistor_SMD:R_0805_2012Metric")
    b.place("Device:R", "RDE", "10k", 155, my - 12, footprint="Resistor_SMD:R_0805_2012Metric")
    b.place("Device:C", "CM3", "100n", 155, my + 12, footprint="Capacitor_SMD:C_0805_2012Metric")
    b.place(
        "Device:Filter_EMI_CommonMode",
        "L485",
        "ACM2012-201",
        175,
        my,
        footprint="Inductor_SMD:L_CommonModeChoke_Coilank_ACM2012",
    )
    b.place("Device:R", "RBIAS_A", "560", 195, my + 8, footprint="Resistor_SMD:R_0805_2012Metric")
    b.place("Device:R", "RBIAS_B", "560", 195, my - 8, footprint="Resistor_SMD:R_0805_2012Metric")
    b.place("Device:R", "RTERM", "120", 210, my, footprint="Resistor_SMD:R_0805_2012Metric")
    b.place("Jumper:SolderJumper_2_Open", "JTERM", "120R", 210, my + 12, footprint="Jumper:SolderJumper-2_P1.3mm_Open_RoundedPad1.0x1.5mm")
    b.place("Device:D_TVS", "TVS2", "SM712", 225, my, rot=90, footprint="Package_TO_SOT_SMD:SOT-23-3")
    b.place("Connector_Generic:Conn_01x03", "J485", "2EDGVC-5.08-03P", 245, my, footprint="Panel:TerminalBlock_3P_5.08")

    # STM32G030C8Tx LQFP-48 (DS12991 Fig.6): một cặp VDD/VSS chân 6/7.
    # VBAT và VREF+ nối +3V3 (không pin, không dùng ADC).
    # PA1 DE, PA2 TX, PA3 RX. PA13/PA14 = SWD. PA9/PA10 = I2C tới HT16K33A.
    b.net_label("U1", "4", "+3V3")
    b.net_label("U1", "5", "+3V3")
    b.net_label("U1", "6", "+3V3")
    b.net_label("U1", "7", "GND")
    b.net_label("U1", "10", "NRST")
    b.net_label("U1", "12", "RS485_DE")
    b.net_label("U1", "13", "UART_TX")
    b.net_label("U1", "14", "UART_RX")
    b.net_label("U1", "35", "SWDIO")
    b.net_label("U1", "36", "SWCLK")
    b.net_label("CVDD1", "1", "+3V3")
    b.net_label("CVDD1", "2", "GND")
    b.net_label("RNST", "1", "+3V3")
    b.net_label("RNST", "2", "NRST")
    b.net_label("CNRST", "1", "NRST")
    b.net_label("CNRST", "2", "GND")
    for _nc in ("1", "2", "3", "8", "9", "11"):
        b.no_connect("U1", _nc)
    b.net_label("J2", "1", "+3V3")
    b.net_label("J2", "2", "SWDIO")
    b.net_label("J2", "3", "SWCLK")
    b.net_label("J2", "4", "GND")

    b.net_label("R47A", "1", "UART_TX")
    b.net_label("R47A", "2", "MAX_DI")
    b.net_label("R47B", "1", "UART_RX")
    b.net_label("R47B", "2", "MAX_RO")
    b.net_label("U3", "1", "MAX_RO")
    b.net_label("U3", "2", "MAX_RE")
    b.net_label("U3", "3", "MAX_DE")
    b.net_label("U3", "4", "MAX_DI")
    b.net_label("U3", "5", "GND")
    b.net_label("U3", "6", "RS485_A")
    b.net_label("U3", "7", "RS485_B")
    b.net_label("U3", "8", "+5V")
    b.net_label("RDE", "1", "RS485_DE")
    b.net_label("RDE", "2", "GND")
    b.net_label("U3", "2", "RS485_DE")
    b.net_label("U3", "3", "RS485_DE")
    b.net_label("CM3", "1", "+5V")
    b.net_label("CM3", "2", "GND")
    # Cuộn CM: 1—4 = A, 2—3 = B (ACM2012 / symbol KiCad).
    b.net_label("L485", "1", "RS485_A")
    b.net_label("L485", "4", "RS485_A_BUS")
    b.net_label("L485", "2", "RS485_B")
    b.net_label("L485", "3", "RS485_B_BUS")
    b.net_label("RBIAS_A", "1", "+5V")
    b.net_label("RBIAS_A", "2", "RS485_A_BUS")
    b.net_label("RBIAS_B", "1", "RS485_B_BUS")
    b.net_label("RBIAS_B", "2", "GND")
    b.net_label("RTERM", "1", "RS485_A_BUS")
    b.net_label("RTERM", "2", "RS485_B_BUS")
    b.net_label("JTERM", "1", "RS485_A_BUS")
    b.net_label("JTERM", "2", "RS485_B_BUS")
    b.net_label("TVS2", "1", "RS485_A_BUS")
    b.net_label("TVS2", "2", "RS485_B_BUS")
    b.net_label("TVS2", "3", "GND")
    b.net_label("J485", "1", "RS485_A_BUS")
    b.net_label("J485", "2", "RS485_B_BUS")
    b.net_label("J485", "3", "GND")

    # UI: HT16K33A quét 7-seg và LED trạng thái. STM32 giữ phím và RS485.
    # ROW nguồn dòng, COM hút cathode. Địa chỉ I2C 0x70 (ROW0–2 kéo xuống trong).
    ty = 140.0
    row_mcu = ["42", "43", "44", "45"]
    col_mcu = ["46", "47", "48", "41"]
    r0805 = "Resistor_SMD:R_0805_2012Metric"
    b.place(
        "Panel:HT16K33A",
        "U6",
        "HT16K33A",
        70,
        ty,
        footprint="Panel:HT16K33A_SSOP28",
    )
    b.place("Device:C", "CLED", "100n", 40, ty, footprint="Capacitor_SMD:C_0805_2012Metric")
    b.place("Device:R", "RSCL", "4.7k", 40, ty + 8, footprint=r0805)
    b.place("Device:R", "RSDA", "4.7k", 40, ty + 14, footprint=r0805)
    b.net_label("U6", "1", "GND")
    b.net_label("U6", "28", "+5V_LED")
    b.net_label("CLED", "1", "+5V_LED")
    b.net_label("CLED", "2", "GND")
    b.net_label("U6", "26", "SCL")
    b.net_label("U6", "27", "SDA")
    b.net_label("U1", "29", "SCL")
    b.net_label("U1", "32", "SDA")
    b.net_label("RSCL", "1", "SCL")
    b.net_label("RSCL", "2", "+5V_LED")
    b.net_label("RSDA", "1", "SDA")
    b.net_label("RSDA", "2", "+5V_LED")
    for i in range(8):
        b.net_label("U6", str(i + 2), f"GRID_{i + 1}")
    for row, pin in enumerate(("25", "24", "23", "22", "21", "20", "19", "18")):
        b.net_label("U6", pin, f"SEG{row + 1}")
    b.net_label("U6", "17", "LED_ROW")
    for pin in ("10", "11", "12", "13", "14", "15", "16"):
        b.no_connect("U6", pin)
    for pin in (
        "15", "16", "17", "18", "19", "20", "21", "22", "23", "24",
        "25", "26", "27", "28", "30", "31", "33", "34", "37", "38", "39", "40",
    ):
        b.no_connect("U1", pin)

    fp_seg = {"7": "SEG1", "6": "SEG2", "4": "SEG3", "2": "SEG4", "1": "SEG5", "9": "SEG6", "10": "SEG7", "5": "SEG8"}
    dx, dy_top, dy_bot = 12.0, 112.0, 84.0
    for i in range(1, 9):
        ref = f"DS{i}"
        x = dx + ((i - 1) % 4) * 15.0
        y = dy_top if i <= 4 else dy_bot
        b.place(
            "Connector_Generic:Conn_01x10",
            ref,
            "7SEG_0.56_CC",
            x,
            y,
            footprint="Panel:LED7seg_1digit_0.56in_CC_THT",
        )
        for pin, seg in fp_seg.items():
            b.net_label(ref, pin, seg)
        b.net_label(ref, "3", f"GRID_{i}")
        b.net_label(ref, "8", f"GRID_{i}")

    # Keypad 4x4. Hàng kéo xuống = quét. Cột pull-up, đọc mức thấp khi nhấn.
    sx, sy = 170.0, ty - 5
    for i in range(1, 17):
        b.place(
            "Switch:SW_Push",
            f"SW{i}",
            "Tact_12mm",
            sx + ((i - 1) % 4) * 12.7,
            sy + ((i - 1) // 4) * 12.7,
            footprint="Panel:SW_Tactile_12mm_THT",
        )
        row, col = (i - 1) // 4, (i - 1) % 4
        b.net_label(f"SW{i}", "1", f"KEY_R{row+1}")
        b.net_label(f"SW{i}", "2", f"KEY_C{col+1}")
    kx, ky = 55, ty + 70
    for r in range(1, 5):
        b.place("Device:R", f"RK{r}", "100", kx, ky + (r - 1) * 5, footprint=r0805)
        b.net_label("U1", row_mcu[r - 1], f"ROW{r}")
        b.net_label(f"RK{r}", "1", f"ROW{r}")
        b.net_label(f"RK{r}", "2", f"KEY_R{r}")
    for c in range(1, 5):
        b.place("Device:R", f"RC{c}", "100", kx + 16, ky + (c - 1) * 5, footprint=r0805)
        b.place("Device:R", f"RPK{c}", "10k", kx + 24, ky + (c - 1) * 5, footprint=r0805)
        b.net_label("U1", col_mcu[c - 1], f"COL{c}")
        b.net_label(f"RPK{c}", "1", "+3V3")
        b.net_label(f"RPK{c}", "2", f"COL{c}")
        b.net_label(f"RC{c}", "1", f"COL{c}")
        b.net_label(f"RC{c}", "2", f"KEY_C{c}")

    # Status LEDs D10-D17. Anode chung ROW8, cathode cùng COM của digit tương ứng.
    for i in range(8):
        ref = f"D{10 + i}"
        b.place(
            "Device:LED",
            ref,
            "LED_3mm",
            200 + i * 2.54,
            ty + 10,
            footprint="Panel:LED_3mm_THT",
        )
        b.net_label(ref, "1", "LED_ROW")
        b.net_label(ref, "2", f"GRID_{i + 1}")

    # Global rails
    b.global_net("+24V_IN", 25, 30)
    b.global_net("VIN_PROTECT", 60, 30)
    b.global_net("+24V", 100, 30)
    b.global_net("GND_PWR", 70, 55)
    b.global_net("+5V_RAW", 162, 25)
    b.global_net("+5V", 178, 25)
    b.global_net("+5V_LED", 90, 125)
    b.global_net("+3V3", 205, 75)
    b.global_net("GND", 25, 55)

    OUT.write_text(b.build(), encoding="utf-8")
    conn_path = ROOT / "panel-keyboard.connectivity.json"
    import json

    conn_path.write_text(
        json.dumps(
            [{"ref": r, "pin": p, "net": n} for r, p, n in b.connectivity],
            indent=2,
        ),
        encoding="utf-8",
    )
    print(f"Wrote {OUT}")
    print(f"Wrote {conn_path}")


if __name__ == "__main__":
    main()
