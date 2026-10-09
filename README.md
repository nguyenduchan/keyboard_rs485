# Bàn phím công nghiệp All-in-One (PCB 120 × 120 mm)

Mạch **STM32G030F6P6 + RS485 (SIT3485)**, rev C — linh kiện phổ biến. **TM1638** quét 2 module LED 4 số, 8 LED trạng thái và 16 phím. Còi chủ động báo nhận phím. **Mặt trên (F.Cu):** toàn bộ **chip và SMD**, stencil một lần. **Mặt dưới (B.Cu):** chỉ **phím, LED 3 mm, 2 module 7 đoạn 4 số**. PCB đã đi dây 2 lớp (autorouter PathFinder, `tools/pathfinder.cpp`), DRC KiCad sạch. Cột phải tách **nguồn (trên)** và **MCU + RS485 (dưới)**. Chống phím ma: `docs/CHONG_PHIM_MA.md`.

## Cấu trúc thư mục

| Thư mục / file | Nội dung |
|----------------|----------|
| `docs/SO_DO_NGUYEN_LY.md` | Sơ đồ nguyên lý, ghép chân, netlist |
| `docs/PCB_LAYOUT.md` | Tọa độ mm, mặt trên / mặt dưới, checklist |
| `docs/CHONG_PHIM_MA.md` | Lọc ma trận, tách vùng nguồn |
| `hardware/kicad/panel-keyboard/tools/front_layout_spec.py` | **Chuẩn tọa độ mặt trước** |
| `docs/CONFIG_PROTOCOL.md`, `docs/RUNTIME_FORMAT.md` | giao thức cấu hình + bản đồ thanh ghi + khối cấu hình (cho PC app) |
| `firmware/` | firmware STM32G030F6P6 (xem `firmware/README.md`) |
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

Hai module 4 số 0,56" (5641AS, thân 50,4 × 19,0 mm, bước digit 12,7 mm) đặt tâm **X=40** trùng tâm keypad. Hàng chân cách 15,24 mm. Khe giữa mép dưới thân 7-seg và mép trên thân phím là **8 mm**.

| Y | 16 | 32 | 48 | 64 | | 80 |
|---:|:---:|:---:|:---:|:---:|:---|:---:|
| 12 | | | | | | D10 |
| 15 | DS1 module 4 số (digit 1–4, tâm X=40) | | | | | |
| 20 | | | | | | D11 |
| 28 | | | | | | D12 |
| 35 | DS2 module 4 số (digit 5–8, tâm X=40) | | | | | |
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

Script so PCB với `front_layout_spec.py` (dung sai **0,01 mm** — lỗ phay mặt tủ tính từ tọa độ này):

```text
python hardware/kicad/panel-keyboard/tools/verify_front_layout.py
```

Tiêu chí:

- 16 nút + 2 module 4 số + 8 LED đúng tọa độ  
- Khe **thân phím ↔ thân module 7-seg dưới (DS2)** ≥ **8 mm** (đủ lưỡi CNC khoảng 3 mm và gân mặt máy)

Chuẩn hóa footprint trên PCB: `tools/fix_front_layout.py` (sau `update_pcb_all.py`).

---

## Chip và SMD — mặt trên (F.Cu)

**U6 TM1638** (SOP-28) nằm dưới cột LED trạng thái, quét 7-seg, LED 3 mm và phím. **U1 STM32G030F6P6** ngay bên phải, nối 3 dây STB/CLK/DIO (kéo lên 4,7k). Còi **BZ1** mặt trên tại (40, 50). Cột phải **X ≈ 82…118** tách hai khối: nguồn phía trên, MCU + RS485 phía dưới. **J_PWR** (115, 16) và **J485** (115, 102) hướng ra **cạnh phải**.

| Vùng | X (mm) | Y (mm) | Nội dung |
|------|--------|--------|----------|
| Dưới LED trạng thái | 70–90 | 74–99 | U6 TM1638, CLED1–3, RSTB/RCLK/RDIO |
| MCU | 90–104 | 80–96 | U1, CVDD1, CNRST, J_SWD và J_LOG (không lắp) |
| Còi | 34–58 | 44–56 | BZ1, QBZ, RBZG |
| **BUCK_5V** | 82–118 | 4–46 | J_PWR, U4 XL1509 (SOP-8), L1, D8, C2, C22 |
| **24V_PROTECT** | 82–118 | 48–76 | C4, F1, FBIN, TVS1, DREV, U5 |
| **MCU_LOGIC** | 72–118 | 78–116 | U1, U3, J485 |

Chi tiết: `docs/PCB_LAYOUT.md`, `tools/zone_layout.py`.

---

## Cơ khí tủ

- Lỗ phay mặt tủ **theo biến thể** (số module 7 đoạn, số LED, số phím / bàn phím màng), mọi số đo nguyên mm, nhìn từ mặt tủ: `hardware/mechanical/cnc/` (DXF cho máy CNC, SVG, CSV, decal). Sinh và kiểm tra: `python hardware/kicad/panel-keyboard/tools/cnc_panel.py --check` — xem `docs/CNC_MAT_MAY.md`.  
- Chip nằm **mặt trên**. Mặt dưới chỉ có phím và LED, hướng ra mặt máy  

## Mở thiết kế

KiCad 10+: `hardware/kicad/panel-keyboard/panel-keyboard.kicad_pro`
