# Bàn phím công nghiệp All-in-One (PCB 120 × 120 mm)

Mạch **STM32G030C8 + RS485**. MCU tự quét phím. **HT16K33A** quét 7-seg và LED. **Mặt trên (F.Cu):** toàn bộ **chip và SMD**, stencil một lần. **Mặt dưới (B.Cu):** chỉ **phím, LED 3 mm, 8× 7-seg**. Cột phải tách **nguồn (trên)** và **MCU + RS485 (dưới)**. Chống phím ma: `docs/CHONG_PHIM_MA.md`.

## Cấu trúc thư mục

| Thư mục / file | Nội dung |
|----------------|----------|
| `docs/SO_DO_NGUYEN_LY.md` | Sơ đồ nguyên lý, ghép chân, netlist |
| `docs/PCB_LAYOUT.md` | Tọa độ mm, mặt trên / mặt dưới, checklist |
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

## Phím và LED — mặt dưới (B.Cu)

KiCad vẽ trang với **Y tăng xuống dưới** (đỉnh hình = Y nhỏ). Từ trên xuống: **7-seg → khe CNC 8 mm → keypad**. LED trạng thái ở cột phải của cụm phím. Thân phím và LED hướng xuống mặt dưới.

Cả hai hàng 7-seg sát nhau, pitch **13,7 mm** (thân 12,70 mm + khe 1 mm), X = 19,45 / 33,15 / 46,85 / 60,55, tâm **X=40** trùng tâm keypad. Thân cao 19,05 mm, hàng chân cách 15,24 mm. Khe giữa mép dưới thân 7-seg và mép trên thân phím là **8 mm**.

| Y | 16 | 32 | 48 | 64 | | 79.5 |
|---:|:---:|:---:|:---:|:---:|:---|:---:|
| 12 | | | | | | D10 |
| 15.4 | DS1–DS4 (X=19.45 / 33.15 / 46.85 / 60.55) | | | | | |
| 20 | | | | | | D11 |
| 28 | | | | | | D12 |
| 35.5 | DS5–DS8 (cùng X với hàng trên) | | | | | |
| 36 | | | | | | D13 |
| 44 | | | | | | D14 |
| 52 | | | | | | D15 |
| 59 | SW13 | SW14 | SW15 | SW16 | | |
| 60 | | | | | | D16 |
| 68 | | | | | | D17 |
| 75 | SW9 | SW10 | SW11 | SW12 | | |
| 91 | SW5 | SW6 | SW7 | SW8 | | |
| 107 | SW1 | SW2 | SW3 | SW4 | | |

### Kiểm tra layout

Script so PCB với `front_layout_spec.py` (dung sai **±1 mm**):

```text
python hardware/kicad/panel-keyboard/tools/verify_front_layout.py
```

Tiêu chí:

- 16 nút + 8 digit + 8 LED đúng tọa độ  
- Khe **thân phím ↔ thân hàng 7-seg dưới (DS5–8)** ≥ **8 mm** (đủ lưỡi CNC khoảng 3 mm và gân mặt máy)

Chuẩn hóa footprint trên PCB: `tools/fix_front_layout.py` (sau `update_pcb_all.py`).

---

## Chip và SMD — mặt trên (F.Cu)

**U6 HT16K33A** nằm dưới cột LED trạng thái và quét 7-seg cùng LED 3 mm. Tụ bypass và trở phím nằm sát chân chip tương ứng. Cột phải **X ≈ 82…118** tách hai khối: nguồn phía trên, MCU + RS485 phía dưới. **J_PWR** (115, 16) và **J485** (115, 102) hướng ra **cạnh phải**.

| Vùng | X (mm) | Y (mm) | Nội dung |
|------|--------|--------|----------|
| Dưới LED trạng thái | 72–88 | 74–96 | U6 HT16K33A, CLED, I2C |
| Sát STM32 | 78–102 | 100–110 | RK/RC/RPK và lọc RS485 |
| **BUCK_5V** | 82–118 | 4–46 | J_PWR, U4 XL1509, C2, C22 |
| **24V_PROTECT** | 82–118 | 48–76 | C4, F1, Q17, U5 |
| **MCU_LOGIC** | 72–118 | 78–116 | U1, U3, J485 |

Chi tiết: `docs/PCB_LAYOUT.md`, `tools/zone_layout.py`.

---

## Cơ khí tủ

- Khoét tủ **105 × 105 mm** — `hardware/mechanical/cabinet_cutout.svg`  
- Decal **120 × 120 mm** — khe **8 mm** giữa 7-seg và phím, **8× Ø3 mm** LED  
- Chip nằm **mặt trên**. Mặt dưới chỉ có phím và LED, hướng ra mặt máy  

## Mở thiết kế

KiCad 10+: `hardware/kicad/panel-keyboard/panel-keyboard.kicad_pro`
