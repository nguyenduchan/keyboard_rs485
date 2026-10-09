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

    # --- Nguồn 24 V: bảo vệ + buck 5 V + LDO 3,3 V (cấu hình thương mại, linh kiện phổ biến) ---
    # J_PWR → F1 (PPTC 1812) → FBIN (ferrite) → VIN_PROTECT [TVS1, C19, C15] → DREV (SS34 chống ngược) → +24V
    # XL1509-5.0 (SOP-8, bản cố định): FB nối thẳng ngõ ra +5V_RAW, ~EN nối GND (luôn chạy).
    y0 = 40.0
    r0805 = "Resistor_SMD:R_0805_2012Metric"
    c0805 = "Capacitor_SMD:C_0805_2012Metric"
    c1206 = "Capacitor_SMD:C_1206_3216Metric"
    fb0805 = "Inductor_SMD:L_0805_2012Metric"
    b.place("Connector_Generic:Conn_01x02", "J_PWR", "KF301-5.08-2P", 25, y0, footprint="Panel:TerminalBlock_2P_5.08")
    b.place("Device:Fuse", "F1", "mSMD050-33V (PPTC 0.5A 33V)", 40, y0, footprint="Fuse:Fuse_1812_4532Metric")
    b.place("Device:FerriteBead", "FBIN", "GZ2012D601TF 600R", 55, y0, footprint=fb0805)
    b.place("Device:D_TVS", "TVS1", "SMAJ28CA", 70, y0 + 16, footprint="Diode_SMD:D_SMA")
    b.place("Device:C", "C19", "2.2uF 50V X7R", 70, y0, footprint=c1206)
    b.place("Device:C", "C15", "100nF 50V", 70, y0 + 28, footprint=c0805)
    b.place("Device:D", "DREV", "SS34", 85, y0, rot=180, footprint="Diode_SMD:D_SMA")
    b.place("Device:C", "C4", "470uF 35V RVT1V471M1010", 100, y0, footprint="Capacitor_SMD:CP_Elec_10x10.5")
    b.place(
        "Connector_Generic:Conn_01x02",
        "NT1",
        "GND_STAR",
        100,
        y0 + 30,
        footprint="NetTie:NetTie-2_SMD_Pad2.0mm",
    )
    b.place("Regulator_Switching:XL1509-5.0", "U4", "XL1509-5.0E1", 118, y0, footprint="Package_SO:SOIC-8_3.9x4.9mm_P1.27mm")
    b.place("Device:C", "C16", "2.2uF 50V X7R", 106, y0, footprint=c1206)
    b.place("Device:C", "C17", "100nF 50V", 106, y0 + 14, footprint=c0805)
    b.place("Device:D", "D8", "SS34", 132, y0 + 14, rot=90, footprint="Diode_SMD:D_SMA")
    b.place("Device:L", "L1", "SWPA6045S470MT 47uH", 146, y0, footprint="Inductor_SMD:L_Taiyo-Yuden_NR-60xx")
    b.place("Device:C", "C2", "220uF 10V RVT1A221M0605", 162, y0, footprint="Capacitor_SMD:CP_Elec_6.3x5.4")
    b.place("Device:C", "C5", "100nF", 162, y0 + 14, footprint=c0805)
    b.place("Device:FerriteBead", "FB2", "GZ2012D101TF 100R", 162, y0 - 12, footprint=fb0805)
    b.place("Device:C", "C22", "220uF 10V RVT1A221M0605", 178, y0, footprint="Capacitor_SMD:CP_Elec_6.3x5.4")
    b.place("Device:FerriteBead", "FB5V", "GZ2012D101TF 100R", 178, y0 - 12, footprint=fb0805)

    b.place("Regulator_Linear:AMS1117-3.3", "U5", "AMS1117-3.3", 194, y0, footprint="Package_TO_SOT_SMD:SOT-223-3_TabPin2")
    b.place("Device:C", "C7", "10uF", 188, y0 + 14, footprint=c0805)
    b.place("Device:C", "C3", "22uF", 210, y0, footprint=c0805)
    b.place("Device:C", "C18", "100nF", 210, y0 + 14, footprint=c0805)
    b.place("Device:FerriteBead", "FB1", "GZ2012D101TF 100R", 210, y0 - 12, footprint=fb0805)

    b.power_flag("power:+24V", 100, y0 - 10)
    b.power_flag("power:GND", 110, y0 + 40)
    b.power_flag("power:+5V", 178, y0 - 25)
    b.power_flag("power:+3V3", 225, y0 - 25)

    b.net_label("J_PWR", "1", "+24V_IN")
    b.net_label("J_PWR", "2", "GND_PWR")
    b.net_label("F1", "1", "+24V_IN")
    b.net_label("F1", "2", "+24V_FUSE")
    b.net_label("FBIN", "1", "+24V_FUSE")
    b.net_label("FBIN", "2", "VIN_PROTECT")
    b.net_label("TVS1", "1", "VIN_PROTECT")
    b.net_label("TVS1", "2", "GND_PWR")
    b.net_label("C19", "1", "VIN_PROTECT")
    b.net_label("C19", "2", "GND_PWR")
    b.net_label("C15", "1", "VIN_PROTECT")
    b.net_label("C15", "2", "GND_PWR")
    b.net_label("DREV", "2", "VIN_PROTECT")  # A
    b.net_label("DREV", "1", "+24V")  # K
    b.net_label("C4", "1", "+24V")
    b.net_label("C4", "2", "GND_PWR")
    b.net_label("NT1", "1", "GND_PWR")
    b.net_label("NT1", "2", "GND")
    b.net_label("U4", "1", "+24V")
    b.net_label("U4", "2", "+5V_SW")
    b.net_label("U4", "3", "+5V_RAW")  # FB bản 5.0 cảm nhận thẳng ngõ ra
    b.net_label("U4", "4", "GND_PWR")  # ~EN mức thấp = chạy
    for _p in ("5", "6", "7", "8"):
        b.net_label("U4", _p, "GND_PWR")
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

    # --- MCU + RS485 ---
    # STM32G030F6P6 TSSOP-20: chỉ cần UART+DE, 3 dây TM1638, còi, SWD.
    my = 90.0
    b.place(
        "MCU_ST_STM32G0:STM32G030F6Px",
        "U1",
        "STM32G030F6P6",
        80,
        my,
        footprint="Package_SO:TSSOP-20_4.4x6.5mm_P0.65mm",
    )
    b.place("Device:C", "CVDD1", "100nF", 65, my + 20, footprint=c0805)
    b.place("Device:C", "CNRST", "100nF", 75, my - 15, footprint=c0805)
    # DAPLink Thế Giới Module, hàng 5 chân, bỏ RST. Cáp 1×4 cắm thẳng, chân 1 = 3V3.
    # J_SWD: 3V3, SWD, SCK, GND. J_LOG: 3V3, RXD (← TX chip), TXD (→ RX chip), GND.
    b.place(
        "Connector_Generic:Conn_01x04",
        "J_SWD",
        "DAP SWD (DNP)",
        42,
        my,
        footprint="Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical",
    )
    b.place(
        "Connector_Generic:Conn_01x04",
        "J_LOG",
        "DAP COM (DNP)",
        58,
        my,
        footprint="Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical",
    )

    b.place("Interface_UART:SP3485EN", "U3", "SP3485EN", 140, my, footprint="Package_SO:SOIC-8_3.9x4.9mm_P1.27mm")
    b.place("Device:R", "R47A", "47", 120, my + 12, footprint=r0805)
    b.place("Device:R", "R47B", "47", 120, my - 12, footprint=r0805)
    b.place("Device:R", "RDE", "10k", 155, my - 12, footprint=r0805)
    b.place("Device:C", "CM3", "100nF", 155, my + 12, footprint=c0805)
    b.place("Device:R", "RBIAS_A", "560", 195, my + 8, footprint=r0805)
    b.place("Device:R", "RBIAS_B", "560", 195, my - 8, footprint=r0805)
    b.place("Device:R", "RTERM", "120", 210, my, footprint=r0805)
    b.place("Jumper:SolderJumper_2_Open", "JTERM", "120R", 210, my + 12, footprint="Jumper:SolderJumper-2_P1.3mm_Open_RoundedPad1.0x1.5mm")
    b.place("Device:D_TVS", "TVS2", "PSM712-LF-T7", 225, my, rot=90, footprint="Package_TO_SOT_SMD:SOT-23-3")
    b.place("Connector_Generic:Conn_01x03", "J485", "KF301-5.08-3P", 245, my, footprint="Panel:TerminalBlock_3P_5.08")

    # STM32G030F6P6 TSSOP-20 (DS12991): VDD 4, VSS 5, NRST 6 (pull-up nội ~40k + CNRST).
    # PA1 DE, PA2 TX, PA3 RX (USART2). PA4 STB, PA5 CLK, PA6 DIO (open-drain, kéo lên +5V_LED,
    # các chân FT chịu 5 V). PA7 còi. PA13/PA14 SWD.
    # PB6 (chân 20) USART1_TX, PB7 (chân 1) USART1_RX — log qua DAPLink, không chung USART2.
    b.net_label("U1", "4", "+3V3")
    b.net_label("U1", "5", "GND")
    b.net_label("U1", "6", "NRST")
    b.net_label("U1", "8", "RS485_DE")
    b.net_label("U1", "9", "UART_TX")
    b.net_label("U1", "10", "UART_RX")
    b.net_label("U1", "11", "TM_STB")
    b.net_label("U1", "12", "TM_CLK")
    b.net_label("U1", "13", "TM_DIO")
    b.net_label("U1", "14", "BZ_ON")
    b.net_label("U1", "1", "LOG_RX")
    b.net_label("U1", "20", "LOG_TX")
    b.net_label("U1", "18", "SWDIO")
    b.net_label("U1", "19", "SWCLK")
    for _nc in ("2", "3", "7", "15", "16", "17"):
        b.no_connect("U1", _nc)
    b.net_label("CVDD1", "1", "+3V3")
    b.net_label("CVDD1", "2", "GND")
    b.net_label("CNRST", "1", "NRST")
    b.net_label("CNRST", "2", "GND")
    b.net_label("J_SWD", "1", "+3V3")
    b.net_label("J_SWD", "2", "SWDIO")
    b.net_label("J_SWD", "3", "SWCLK")
    b.net_label("J_SWD", "4", "GND")
    b.net_label("J_LOG", "1", "+3V3")
    b.net_label("J_LOG", "2", "LOG_TX")
    b.net_label("J_LOG", "3", "LOG_RX")
    b.net_label("J_LOG", "4", "GND")

    b.net_label("R47A", "1", "UART_TX")
    b.net_label("R47A", "2", "MAX_DI")
    b.net_label("R47B", "1", "UART_RX")
    b.net_label("R47B", "2", "MAX_RO")
    b.net_label("U3", "1", "MAX_RO")
    b.net_label("U3", "2", "RS485_DE")
    b.net_label("U3", "3", "RS485_DE")
    b.net_label("U3", "4", "MAX_DI")
    b.net_label("U3", "5", "GND")
    b.net_label("U3", "6", "RS485_A_BUS")
    b.net_label("U3", "7", "RS485_B_BUS")
    b.net_label("U3", "8", "+3V3")
    b.net_label("RDE", "1", "RS485_DE")
    b.net_label("RDE", "2", "GND")
    b.net_label("CM3", "1", "+3V3")
    b.net_label("CM3", "2", "GND")
    b.net_label("RBIAS_A", "1", "+5V")
    b.net_label("RBIAS_A", "2", "RS485_A_BUS")
    b.net_label("RBIAS_B", "1", "RS485_B_BUS")
    b.net_label("RBIAS_B", "2", "GND")
    # Termination: A -- JTERM (hàn tắt) -- RTERM 120R -- B
    b.net_label("JTERM", "1", "RS485_A_BUS")
    b.net_label("JTERM", "2", "RS485_TERM")
    b.net_label("RTERM", "1", "RS485_TERM")
    b.net_label("RTERM", "2", "RS485_B_BUS")
    b.net_label("TVS2", "1", "RS485_A_BUS")
    b.net_label("TVS2", "2", "RS485_B_BUS")
    b.net_label("TVS2", "3", "GND")
    b.net_label("J485", "1", "RS485_A_BUS")
    b.net_label("J485", "2", "RS485_B_BUS")
    b.net_label("J485", "3", "GND")

    # --- UI: TM1638 quét 2 module 4 số, 8 LED trạng thái và 16 phím ---
    # SEG1–SEG8 = đoạn a–g, dp (đồng thời KS1–KS8 quét phím). SEG9 = anode chung 8 LED trạng thái.
    # GRID1–GRID8 = cathode digit 1–8 (LED trạng thái Dk chung cathode với digit k).
    # Phím: KSx ↔ K1 (hàng phím 1–2) hoặc K2 (hàng 3–4). K1–K3 có pull-down 10k nội.
    ty = 140.0
    b.place("Panel:TM1638", "U6", "TM1638", 70, ty, footprint="Package_SO:SOIC-28W_7.5x17.9mm_P1.27mm")
    b.place("Device:C", "CLED1", "100nF", 40, ty - 8, footprint=c0805)
    b.place("Device:C", "CLED2", "100nF", 40, ty, footprint=c0805)
    b.place("Device:C", "CLED3", "10uF", 40, ty + 8, footprint=c0805)
    b.place("Device:R", "RSTB", "4.7k", 100, ty - 8, footprint=r0805)
    b.place("Device:R", "RCLK", "4.7k", 100, ty, footprint=r0805)
    b.place("Device:R", "RDIO", "4.7k", 100, ty + 8, footprint=r0805)
    b.net_label("U6", "4", "+5V_LED")
    b.net_label("U6", "15", "+5V_LED")
    b.net_label("U6", "18", "GND")
    b.net_label("U6", "25", "GND")
    b.net_label("U6", "28", "TM_STB")
    b.net_label("U6", "27", "TM_CLK")
    b.net_label("U6", "26", "TM_DIO")
    b.net_label("U6", "1", "KEY_K1")
    b.net_label("U6", "2", "KEY_K2")
    b.no_connect("U6", "3")
    b.no_connect("U6", "14")
    for i in range(1, 9):
        b.net_label("U6", str(4 + i), f"SEG{i}")
    b.net_label("U6", "13", "LED_ROW")
    for i, pin in zip(range(1, 9), ("24", "23", "22", "21", "20", "19", "17", "16")):
        b.net_label("U6", pin, f"GRID_{i}")
    for ref in ("CLED1", "CLED2", "CLED3"):
        b.net_label(ref, "1", "+5V_LED")
        b.net_label(ref, "2", "GND")
    for ref, net in (("RSTB", "TM_STB"), ("RCLK", "TM_CLK"), ("RDIO", "TM_DIO")):
        b.net_label(ref, "1", "+5V_LED")
        b.net_label(ref, "2", net)

    # Module 4 số 0,56" CC (5641AS): 1 E, 2 D, 3 DP, 4 C, 5 G, 6 DIG4, 7 B, 8 DIG3, 9 DIG2, 10 F, 11 A, 12 DIG1.
    mod_seg = {"11": "SEG1", "7": "SEG2", "4": "SEG3", "2": "SEG4", "1": "SEG5", "10": "SEG6", "5": "SEG7", "3": "SEG8"}
    mod_dig = {"12": 0, "9": 1, "8": 2, "6": 3}
    for m, (ref, y) in enumerate((("DS1", 112.0), ("DS2", 84.0))):
        b.place(
            "Connector_Generic:Conn_01x12",
            ref,
            "5641AS (4 digit 0.56in CC)",
            12.0,
            y,
            footprint="Panel:Display_4digit_0.56in_THT",
        )
        for pin, seg in mod_seg.items():
            b.net_label(ref, pin, seg)
        for pin, d in mod_dig.items():
            b.net_label(ref, pin, f"GRID_{4 * m + d + 1}")

    # Keypad 4×4: SW pin1/4 = KSx (SEGx), pin2/3 = K1 hoặc K2. Firmware nhận một phím một lúc.
    sx, sy = 170.0, ty - 5
    for i in range(1, 17):
        b.place(
            "Switch:SW_Push",
            f"SW{i}",
            "B3F-4055",
            sx + ((i - 1) % 4) * 12.7,
            sy + ((i - 1) // 4) * 12.7,
            footprint="Panel:SW_Tactile_12mm_THT",
        )
        row, col = (i - 1) // 4, (i - 1) % 4
        b.net_label(f"SW{i}", "1", f"SEG{(row % 2) * 4 + col + 1}")
        b.net_label(f"SW{i}", "2", f"KEY_K{row // 2 + 1}")

    # J_KEY: giắc bàn phím màng (dán), nối song song ma trận tact. 1 K1, 2 K2, 3..10 KS1..KS8.
    # Phím màng hàng r, cột c (r, c = 0..3) nối K(r // 2 + 1) với KS((r % 2) * 4 + c + 1), trùng SW.
    b.place(
        "Connector_Generic:Conn_01x10",
        "J_KEY",
        "Membrane 1x10 2.54",
        sx + 70,
        sy + 19,
        footprint="Panel:Membrane_1x10_P2.54_Vertical_Slot",
    )
    b.net_label("J_KEY", "1", "KEY_K1")
    b.net_label("J_KEY", "2", "KEY_K2")
    for i in range(1, 9):
        b.net_label("J_KEY", str(2 + i), f"SEG{i}")

    # Status LEDs D10-D17: anode chung SEG9 (LED_ROW), cathode chung GRID của digit tương ứng.
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
        b.net_label(ref, "1", "GRID_" + str(i + 1))
        b.net_label(ref, "2", "LED_ROW")

    # Còi báo phím: còi chủ động 5 V 12×9,5 mm (có mạch dao động sẵn), PA7 bật 30–50 ms mỗi lần nhận phím.
    # QBZ hút cực âm còi; RBZG giữ còi tắt khi MCU reset.
    bzx, bzy = 115.0, 205.0
    b.place("Device:Buzzer", "BZ1", "Active buzzer 5V 12x9.5", bzx, bzy, footprint="Buzzer_Beeper:Buzzer_12x9.5RM7.6")
    b.place("Transistor_FET:2N7002", "QBZ", "2N7002", bzx, bzy + 16, footprint="Package_TO_SOT_SMD:SOT-23")
    b.place("Device:R", "RBZG", "100k", bzx - 14, bzy + 22, footprint=r0805)
    b.net_label("BZ1", "1", "+5V")
    b.net_label("BZ1", "2", "BZ_DRV")
    b.net_label("QBZ", "3", "BZ_DRV")
    b.net_label("QBZ", "2", "GND")
    b.net_label("QBZ", "1", "BZ_ON")
    b.net_label("RBZG", "1", "BZ_ON")
    b.net_label("RBZG", "2", "GND")

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
