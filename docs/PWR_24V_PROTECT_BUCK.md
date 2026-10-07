# Khối nguồn 24 V — Panel Keyboard

Bo keyboard dùng **cùng khối** với Fiberboard, mô tả đầy đủ trong:

`D:\Project\embedded\fiberboard\pcb\PWR_24V_PROTECT_BUCK_GUIDE.txt`

## Áp dụng trên panel keyboard

| Khối | Ref trên bo | Ghi chú |
|------|-------------|---------|
| VIN domino | **J_PWR** 2P 5,08 mm | Thay J1 fiberboard |
| PWR_24V_PROTECT | F1, **L2**, **RV1**, TVS1, C19, C15, Q12, R17, C4, C14, **NT1** | Mức 1 (CM + MOV + 1000 µF) |
| PWR_BUCK | U4, C16/C17, D8, L1, C2/C5, **FB2**, **C22**, R36–R38 | +5V_RAW → +5V |
| Rail display | **FB5V** | +5V → +5V_LED (TM1638 / 7-seg), thêm so với guide |
| LDO 3,3 V | U5, C7, C3/C18, **FB1**, **C21**, CVDDA | MCU + MAX485 IO 3,3 V |

## Mạch đất

- **GND_PWR**: domino −, CM, TVS/MOV, buck, bulk C4−.
- **GND**: MCU, RS485 logic, tụ 5 V/3,3 V phía logic.
- **NT1**: một điểm sao tại cực âm bulk 24 V.

## Đồng bộ schematic → PCB

```text
python hardware/kicad/panel-keyboard/tools/update_pcb_all.py
```

(Đóng file PCB trong KiCad trước khi chạy.)
