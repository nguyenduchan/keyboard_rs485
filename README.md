# Bàn phím công nghiệp All-in-One (100 × 150 mm)

Mạch **STM32F030F4 + TM1638 + RS485**. **Mặt trước (F.Cu):** toàn **chân cắm (THT)** — phím, LED 3 mm, **8× 7-seg đơn**. **Mặt sau (B.Cu):** **SMD một lần stencil** — 3 vùng **24V / buck / MCU**. Chống phím ma: `docs/CHONG_PHIM_MA.md`.

## Cấu trúc thư mục

| Thư mục / file | Nội dung |
|----------------|----------|
| `docs/SO_DO_NGUYEN_LY.md` | Sơ đồ nguyên lý, ghép chân, netlist |
| `docs/PCB_LAYOUT.md` | Tọa độ mm, vùng mặt sau, checklist |
| `docs/CHONG_PHIM_MA.md` | Lọc ma trận, tách vùng nguồn |
| `hardware/kicad/panel-keyboard/tools/front_layout_spec.py` | **Chuẩn tọa độ mặt trước** |
| `hardware/bom.csv` | BOM |

## Đồng bộ schematic + PCB

```text
python hardware/kicad/panel-keyboard/tools/update_pcb_all.py
python hardware/kicad/panel-keyboard/tools/verify_front_layout.py
```

`update_pcb_all.py` gọi `migrate_front_tht.py` (một process/ref, tránh crash `pcbnew` trên Windows) rồi `fix_front_layout.py` + gán net.

(Đóng KiCad trước khi chạy.) Quy tắc: `.cursor/rules/kicad-schematic-pcb-sync.mdc`

---

## Mặt trước F.Cu (100 × 150 mm)

KiCad vẽ trang với **Y tăng xuống dưới** (đỉnh hình = Y nhỏ). Từ trên xuống: **7-seg → keypad → mạch**. LED trạng thái cột phải, bắt đầu sát cạnh trên.

| Y | 23 | 39 | 55 | 71 | | 86.5 |
|---:|:---:|:---:|:---:|:---:|:---|:---:|
| 12 | | | | | | D10 |
| 14 | DS1 | DS2 | DS3 | DS4 | | |
| 20 | | | | | | D11 |
| 28 | | | | | | D12 |
| 36 | DS5 | DS6 | DS7 | DS8 | | D13 |
| 44 | | | | | | D14 |
| 52 | SW13 | SW14 | SW15 | SW16 | | D15 |
| 60 | | | | | | D16 |
| 68 | SW9 | SW10 | SW11 | SW12 | | D17 |
| 84 | SW5 | SW6 | SW7 | SW8 | | |
| 100 | SW1 | SW2 | SW3 | SW4 | | |

### Kiểm tra layout

Script so PCB với `front_layout_spec.py` (dung sai **±1 mm**):

```text
python hardware/kicad/panel-keyboard/tools/verify_front_layout.py
```

Tiêu chí:

- 16 nút + 8 digit + 8 LED đúng tọa độ  
- Khe **keypad ↔ hàng 7-seg dưới (DS5–8)** ≥ **8 mm** (tâm hàng phím cao nhất + 6 mm ≤ mép dưới digit − 10 mm)

Chuẩn hóa footprint trên PCB: `tools/fix_front_layout.py` (sau `update_pcb_all.py`).

---

## Layout mặt sau (Bottom) — đáy trang KiCad (Y lớn)

Khối bảo vệ và buck **Y = 108–131** (cao 23 mm, có khe đi dây). Khối MCU là cột phải **X = 78–97**, **Y = 74–131** (dưới LED trạng thái). Sát cạnh dưới (**Y = 146**): **J_PWR** (trái) và **J485** (phải). Dải **Y = 133–140** trên B.SilkS để chữ «Nguồn 24V / +24V / 0V» và «RS485 / A / B / 0V».

| Vùng | X (mm) | Y (mm) | Nội dung |
|------|--------|--------|----------|
| **24V_PROTECT** | 4–40 | 108–131 | F1, L2, MOV, TVS, Q12, C4, NT1 |
| **BUCK_5V** | 42–76 | 108–131 | U4 XL1509, L1, D8, C2, FB2, C22 |
| **MCU_LOGIC** | 78–97 | 74–131 | U1 STM32, U2, U5, U3, lọc phím |
| **Giắc** | 22 và 86 | **146** | J_PWR, J485 — chữ silk ngay phía trên |

Chi tiết: `docs/PCB_LAYOUT.md`, `tools/zone_layout.py`.

---

## Cơ khí tủ

- Khoét **85 × 135 mm** — `hardware/mechanical/cabinet_cutout.svg`  
- Decal **100 × 150 mm** — lỗ phím, **8× lỗ 7-seg đơn**, **8× Ø3 mm** LED  
- MCU **U1** chỉ trên **B.Cu** — không lộ mặt trước  

## Mở thiết kế

KiCad 10+: `hardware/kicad/panel-keyboard/panel-keyboard.kicad_pro`
