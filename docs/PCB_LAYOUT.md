# PCB — Layout 100 × 150 mm (All-in-One)

## Thông số tổng

| Thông số | Giá trị |
|----------|---------|
| Kích thước bo mạch | **100 × 150 mm** |
| Lớp | 2 layer (Top + Bottom), FR4 1.6 mm |
| **Mặt trước (Top)** | **Chỉ THT:** phím 12 mm, LED 3 mm, **8× 7-seg đơn** — **không SMD** |
| **Mặt sau (Bottom)** | **Chỉ SMD** (stencil một mặt): nguồn, MCU, TM1638, RS485, R/C 0805 |
| Lỗ ốc | 4 × **M3**, drill **3.2 mm** |
| Vùng vành | **5 mm** từ mép |

Hệ tọa độ file KiCad: X phải, **Y tăng xuống dưới trên bản vẽ** (SVG `cy` lớn = phía dưới hình). **Cạnh trên = Y nhỏ. Cạnh dưới = Y lớn.**

## Mặt trước — 100% chân cắm (THT)

| Thành phần | Footprint | Ghi chú |
|------------|-----------|---------|
| 8× 7-seg đơn | `Panel:LED7seg_1digit_0.56in_CC_THT` | Cạnh trên hình: **Y=14** (DS1–4), **Y=36** (DS5–8), cột X=23/39/55/71 |
| Keypad 4×4 | `Panel:SW_Tactile_12mm_THT` | SW13 **Y=52** … SW1 **Y=100**, pitch **16**, tâm X=47 |
| 8× LED trạng thái | `Panel:LED_3mm_THT` | Cột phải **X=86.5**, D10 **Y=12** (sát cạnh trên) → D17 **Y=68** |
| Điện trở LED | **RLED1–8** trên **Bottom** | Dây từ chân LED xuống B.Cu |

Decal tủ: lỗ tròn **Ø3 mm** cho LED 3 mm (thay vùng 0805).

## Mặt sau — 3 vùng chữ nhật (B.SilkS)

Từ **trên xuống** trên bản vẽ KiCad (Y tăng):

```
Y=0    cạnh trên hình
Y=12   D10 (X=86.5)     DS1–DS4 (Y=14, X=23..71)
Y=36   DS5–DS8
Y=52   SW13–SW16        cột LED xuống D17 (Y=68)
Y=100  SW1–SW4
Y=74   cột MCU X=78–97 (dưới LED, cao tới Y=131)
Y=108  ┌ bảo vệ 4–40 ┬ buck 42–76 ┐
Y=131  └────────────┴─────────────┘
Y=133  Nguồn 24V              RS485       (silk)
Y=138  +24V  0V               A  B  0V
Y=146  [J_PWR]                [J485]
Y=150  cạnh dưới hình
```

| Vùng | X (mm) | Y (mm) | Linh kiện chính |
|------|--------|--------|-----------------|
| **24V_PROTECT** | **4–40** | **108–131** | F1, L2, RV1, TVS1, Q12, C4, NT1, J_PWR |
| **BUCK_5V** | **42–76** | **108–131** | U4 XL1509, L1, D8, C2, FB2, C22, FB5V |
| **MCU_LOGIC** | **78–97** | **74–131** | **U1** STM32, U2, U5, U3, lọc phím, J485 |
| **Khe** | 4–76 | **106–108** | Không SMD giữa keypad và mạch nguồn |

Chi tiết tọa độ: `tools/zone_layout.py`. Vẽ viền vùng: `tools/fix_front_layout.py`.

## SMT thủ công / stencil

1. **Stencil chỉ Bottom** — reflow toàn SMD mặt sau.  
2. **Mặt trước:** lắp **SW THT → 8× 7-seg đơn → LED 3 mm** (mỏ hàn).  
3. Không cần stencil Top.

## Chống phím ma

Xem **`docs/CHONG_PHIM_MA.md`**: RK/RC 100Ω + CK/CC 100nF tại TM1638, tách vùng PCB, debounce firmware.

## Mặt trước (bảng layout)

Lưới tọa độ: **`README.md`** — *Mặt trước F.Cu*.

## Checklist

- [ ] Top: **không** paste SMD (chỉ THT)
- [ ] Bottom: stencil + reflow; 3 vùng silk đúng
- [ ] Khe Y 70–76: không route buck/24V xuyên lên THT
- [ ] Matrix: RK/CK/RC/CC populated
- [ ] `CHONG_PHIM_MA.md` test false trigger

## KiCad

`python tools/build_schematic.py` → `python tools/update_pcb_all.py` (đóng PCB trong KiCad trước).
