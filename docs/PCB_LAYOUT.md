# PCB — Layout 120 × 120 mm (All-in-One)

Hai hàng 7-seg sát nhau. Khe tới bàn phím là **8 mm** để cắt CNC mặt máy. Cột phải tách **nguồn (trên)** và **MCU + RS485 (dưới)**. Giắc hướng ra cạnh phải. **Rev C đã đi dây đủ 2 lớp** (autorouter PathFinder — xem mục Đi dây tự động), DRC KiCad: 0 lỗi clearance, 0 kết nối hở.

## Thông số tổng

| Thông số | Giá trị |
|----------|---------|
| Kích thước bo mạch | **120 × 120 mm** (X = 0…120, Y = 0…120) |
| Lớp | 2 layer (Top + Bottom), FR4 1.6 mm |
| **Mặt trên (Top, F.Cu)** | **Toàn bộ chip và SMD**: STM32, TM1638, buck, LDO, RS485, R/C, giắc, còi. Stencil một mặt |
| **Mặt dưới (Bottom, B.Cu)** | **Chỉ phím + LED**: tact 12 mm, LED 3 mm, 2 module 7-seg 4 số. Thân linh kiện hướng xuống |
| Lỗ ốc | 4 × **M3**, drill **3.2 mm** |
| Vùng vành | **5 mm** từ mép |

Hệ tọa độ file KiCad: X phải, **Y tăng xuống dưới trên bản vẽ** (SVG `cy` lớn = phía dưới hình). **Cạnh trên = Y nhỏ. Cạnh dưới = Y lớn.**

## Mặt dưới — phím và LED (B.Cu)

| Thành phần | Footprint | Ghi chú |
|------------|-----------|---------|
| 2× module 4 số 0,56" CC | `Panel:Display_4digit_0.56in_THT` (5641AS) | Thân **50,4 × 19,0 mm**, hàng chân **15,24 mm**, bước digit 12,7 mm, tâm X=**40**. DS1 **Y=15,375** (digit 1–4), DS2 **Y=35,475** (digit 5–8) |
| Keypad 4×4 | `Panel:SW_Tactile_12mm_THT` | SW13 **Y=59** … SW1 **Y=107**, pitch **16**, tâm X=40. Khe thân tới 7-seg **8 mm** |
| 8× LED trạng thái | `Panel:LED_3mm_THT` | Cột **X=79.5**, D10 **Y=12** → D17 **Y=68** |
| LED driver | **U6 TM1638** trên **Top** | SOP-28, dưới cột LED, 3 dây STB/CLK/DIO tới STM32 |

Decal tủ: lỗ tròn **Ø3 mm** cho LED 3 mm (thay vùng 0805).

## Mặt trên — chip, nguồn, MCU (F.SilkS)

Khe giữa hai phím khoảng 3,6 mm, không đặt 0805. Cột phải rộng khoảng 36 mm, chia đôi theo chiều dọc: nguồn ở nửa trên, MCU và RS485 ở nửa dưới, silk để hở giữa hai vùng.

```
X=0    cạnh trái, lỗ M3 tại X=5
X=70…88  CLED1–3 và RSTB/RCLK/RDIO sát U6
X=40   tâm keypad, 7-seg và còi BZ1 (Y=50)
X=78   U6 TM1638 (Y=86)
X=95   U1 STM32G030F6P6 (Y=86), CVDD1/CNRST bên phải, J2 phía dưới
X=79.5 LED trạng thái
X=82…118  BUCK (Y 4–46), 24V+LDO (Y 48–76), MCU+RS485 (Y 78–116)
X=115  J_PWR (Y=16) và J485 (Y=102), hướng ra cạnh phải
X=120  cạnh phải, lỗ M3 tại X=115
Y=120  cạnh dưới, lỗ M3 tại Y=115
```

| Vùng | X (mm) | Y (mm) | Linh kiện chính |
|------|--------|--------|-----------------|
| Dưới LED trạng thái | 70–90 | **74–99** | **U6** TM1638, CLED1–3, RSTB/RCLK/RDIO |
| MCU | 90–104 | **80–96** | **U1**, CVDD1, CNRST, J2 (DNP) |
| **BUCK_5V** | **82–118** | **4–46** | **J_PWR**, **U4**, C2/C22, D8, L1 |
| **24V_PROTECT** | **82–118** | **48–76** | **C4**, F1, FBIN, TVS1, **DREV**, **U5** |
| **MCU_LOGIC** | **72–118** | **78–116** | **U1**, J2, **U3**, **J485** |
| Còi báo phím | 34–58 | 44–56 | **BZ1** còi chủ động 12 mm (THT, mặt trên, tâm thân 40/50), QBZ, RBZG |

Chi tiết tọa độ: `tools/zone_layout.py`. Vẽ viền vùng: `tools/fix_front_layout.py`.

## SMT thủ công / stencil

1. **Stencil chỉ Top** — reflow toàn bộ chip và SMD mặt trên.  
2. **Mặt dưới:** lắp **SW THT → 2 module 7-seg → LED 3 mm** (thân hướng xuống, hàn mặt trên). Còi BZ1 và 2 giắc cắm từ mặt trên.  
3. Không có SMD mặt dưới.

## Chống phím ma

Xem **`docs/CHONG_PHIM_MA.md`**: TM1638 quét phím, debounce firmware.

## Mặt trước (bảng layout)

Lưới tọa độ phím và LED: **`README.md`**.

## Checklist

- [ ] Top: stencil + reflow toàn chip và SMD; silk nguồn ở trên, MCU ở dưới, cột phải
- [ ] Bottom: chỉ phím, LED 3 mm và 7-seg
- [ ] Nguồn và MCU tách hai vùng; giắc cạnh phải, không đè courtyard phím / 7-seg
- [x] Đi dây 2 lớp, DRC: 0 clearance, 0 unconnected (còn cảnh báo silkscreen đè pad — nhà mạch tự cắt)
- [ ] Cân nhắc phủ đồng GND (zone) mặt dưới cột phải trước khi đặt mạch
- [ ] Sửa cửa sổ hiển thị trên decal / khoét mặt tủ theo module 4 số
- [ ] `CHONG_PHIM_MA.md` test false trigger

## KiCad

Đóng PCB trong KiCad trước. `KPY` = python của KiCad 10 (`.../KiCad/10.0/bin/python.exe`).

```text
python tools/build_schematic.py                 # schematic + connectivity.json
KPY tools/sync_pcb_from_conn.py remove          # bỏ footprint cũ/đổi gói (xoá luôn dây)
KPY tools/sync_pcb_from_conn.py add             # thêm footprint theo zone_layout.py, gán net
```

## Đi dây tự động

Router viết riêng (`tools/pathfinder.cpp`): lưới 0,05 mm, A* 8 hướng (45°) trên 2 lớp + via, thương lượng tắc nghẽn kiểu **PathFinder** (Lee/A* + present/history cost). Chân IC (SOIC/TSSOP/SOT) có **stub thẳng 0,4–1,0 mm** ra khỏi pad rồi mới được rẽ, để hàn tay không bị cầu thiếc. Rule: dây 0,25 mm (tín hiệu) / 0,35 mm (rail) / 0,5 mm (nguồn chính), clearance 0,2 mm, via 0,6/0,3 mm, cách mép 0,5 mm.

```text
g++ -O2 -std=c++14 -o tools/pathfinder.exe tools/pathfinder.cpp
python tools/route_export.py problem.txt        # pad, vật cản, stub -> bài toán
tools/pathfinder.exe problem.txt result.txt 60  # ~30 s
KPY tools/write_tracks.py problem.txt result.txt
kicad-cli pcb drc panel-keyboard.kicad_pcb
```
