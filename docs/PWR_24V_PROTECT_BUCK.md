# Khối nguồn 24 V — Panel Keyboard

Bo keyboard dùng **cùng khối** với Fiberboard, mô tả đầy đủ trong:

`D:\Project\embedded\fiberboard\pcb\PWR_24V_PROTECT_BUCK_GUIDE.txt`

## Áp dụng trên panel keyboard

| Khối | Ref trên bo | Ghi chú |
|------|-------------|---------|
| VIN domino | **J_PWR** 2P 5,08 mm | Thay J1 fiberboard |
| PWR_24V_PROTECT | F1 (PPTC 1812), **FBIN**, TVS1, C19, C15, **DREV** SS34, C4 470 µF/35 V, **NT1** | Rev C: bỏ CM choke L2, MOV RV1, P-FET Q17 (linh kiện hiếm). C15 sát TVS1 |
| PWR_BUCK | U4 XL1509-5.0 **SOP-8**, C16/C17, D8 SS34 (SMA), L1 47 µH 6045, C2/C5, **FB2**, **C22** | FB nối +5V_RAW, ~EN nối GND_PWR (rev B sai: R38 kéo ~EN lên 24 V). C16/C17 sát VIN |
| Rail display | **FB5V**, **CLED1–3** | +5V_LED cho TM1638 và còi |
| LDO 3,3 V | U5, C7, C3/C18, **FB1**, **CVDD1** | C7 sát chân vào, C3/C18 sát chân ra, CVDD1 sát VDD STM32 |

## Mạch đất

- **GND_PWR**: domino −, TVS, buck, bulk C4−.
- **GND**: MCU, RS485 logic, tụ 5 V/3,3 V phía logic.
- **NT1**: một điểm sao tại cực âm bulk 24 V.

## Đồng bộ schematic → PCB

```text
python hardware/kicad/panel-keyboard/tools/build_schematic.py
<KiCad python> hardware/kicad/panel-keyboard/tools/sync_pcb_from_conn.py remove
<KiCad python> hardware/kicad/panel-keyboard/tools/sync_pcb_from_conn.py add
```

(Đóng file PCB trong KiCad trước khi chạy.)
