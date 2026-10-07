# PCB — Layout 120 × 120 mm (All-in-One)

Hai hàng 7-seg sát nhau. Khe tới bàn phím là **8 mm** để cắt CNC mặt máy. Cột phải tách **nguồn (trên)** và **MCU + RS485 (dưới)**. Giắc hướng ra cạnh phải. Không có track/via — chỉ đặt linh kiện, hở courtyard ≥ 1 mm.

## Thông số tổng

| Thông số | Giá trị |
|----------|---------|
| Kích thước bo mạch | **120 × 120 mm** (X = 0…120, Y = 0…120) |
| Lớp | 2 layer (Top + Bottom), FR4 1.6 mm |
| **Mặt trên (Top, F.Cu)** | **Toàn bộ chip và SMD**: STM32, HT16K33A, buck, LDO, RS485, R/C, giắc. Stencil một mặt |
| **Mặt dưới (Bottom, B.Cu)** | **Chỉ phím + LED**: tact 12 mm, LED 3 mm, 8× 7-seg. Thân linh kiện hướng xuống |
| Lỗ ốc | 4 × **M3**, drill **3.2 mm** |
| Vùng vành | **5 mm** từ mép |

Hệ tọa độ file KiCad: X phải, **Y tăng xuống dưới trên bản vẽ** (SVG `cy` lớn = phía dưới hình). **Cạnh trên = Y nhỏ. Cạnh dưới = Y lớn.**

## Mặt dưới — phím và LED (B.Cu)

| Thành phần | Footprint | Ghi chú |
|------------|-----------|---------|
| 8× 7-seg đơn | `Panel:LED7seg_1digit_0.56in_CC_THT` | Thân **12,70 × 19,05 mm**, hàng chân **15,24 mm**. Pitch **13,7 mm**, X=19,45 / 33,15 / 46,85 / 60,55, tâm X=**40**. Hàng trên **Y=15,375**, hàng dưới **Y=35,475** |
| Keypad 4×4 | `Panel:SW_Tactile_12mm_THT` | SW13 **Y=59** … SW1 **Y=107**, pitch **16**, tâm X=40. Khe thân tới 7-seg **8 mm** |
| 8× LED trạng thái | `Panel:LED_3mm_THT` | Cột **X=79.5**, D10 **Y=12** → D17 **Y=68** |
| LED driver | **U6 HT16K33A** trên **Top** | SSOP-28, dưới cột LED, I2C tới STM32 |

Decal tủ: lỗ tròn **Ø3 mm** cho LED 3 mm (thay vùng 0805).

## Mặt trên — chip, nguồn, MCU (F.SilkS)

Khe giữa hai phím khoảng 3,6 mm, không đặt 0805. Cột phải rộng khoảng 36 mm, chia đôi theo chiều dọc: nguồn ở nửa trên, MCU và RS485 ở nửa dưới, silk để hở giữa hai vùng.

```
X=0    cạnh trái, lỗ M3 tại X=5
X=74…86  CLED/I2C sát U6; CVDD1, NRST và trở phím sát U1
X=40   tâm keypad và 7-seg
X=78   U6 HT16K33A (Y=90), CLED và kéo I2C cạnh chip
X=79.5 LED trạng thái
X=82…118  BUCK (Y 4–46), 24V+LDO (Y 48–76), MCU+RS485 (Y 78–116)
X=115  J_PWR (Y=16) và J485 (Y=102), hướng ra cạnh phải
X=120  cạnh phải, lỗ M3 tại X=115
Y=120  cạnh dưới, lỗ M3 tại Y=115
```

| Vùng | X (mm) | Y (mm) | Linh kiện chính |
|------|--------|--------|-----------------|
| Dưới LED trạng thái | 72–88 | **74–96** | **U6** HT16K33A, **CLED**, RSCL, RSDA |
| Sát STM32 | 78–102 | **100–110** | RK/RC/RPK |
| **BUCK_5V** | **82–118** | **4–46** | **J_PWR**, **U4**, C2/C22, D8, L1 |
| **24V_PROTECT** | **82–118** | **48–76** | **C4**, F1, **Q17**, **U5** |
| **MCU_LOGIC** | **72–118** | **78–116** | **U1**, J2, **U3**, **J485** |

Chi tiết tọa độ: `tools/zone_layout.py`. Vẽ viền vùng: `tools/fix_front_layout.py`.

## SMT thủ công / stencil

1. **Stencil chỉ Top** — reflow toàn bộ chip và SMD mặt trên.  
2. **Mặt dưới:** lắp **SW THT → 8× 7-seg đơn → LED 3 mm** (thân hướng xuống, hàn mặt trên).  
3. Không có SMD mặt dưới.

## Chống phím ma

Xem **`docs/CHONG_PHIM_MA.md`**: RK/RC 100Ω sát GPIO, RPK 10k, debounce firmware.

## Mặt trước (bảng layout)

Lưới tọa độ phím và LED: **`README.md`**.

## Checklist

- [ ] Top: stencil + reflow toàn chip và SMD; silk nguồn ở trên, MCU ở dưới, cột phải
- [ ] Bottom: chỉ phím, LED 3 mm và 7-seg
- [ ] Nguồn và MCU tách hai vùng; giắc cạnh phải, không đè courtyard phím / 7-seg
- [ ] Chưa đi dây (không segment/via) — route trong KiCad sau khi duyệt vị trí
- [ ] Matrix: RK/RC/RPK sát GPIO STM32
- [ ] `CHONG_PHIM_MA.md` test false trigger

## KiCad

`python tools/build_schematic.py` → `python tools/update_pcb_all.py` (đóng PCB trong KiCad trước).
