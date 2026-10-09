# PCB — Layout 120 × 120 mm (All-in-One)

Hai hàng 7-seg sát nhau. Khe tới bàn phím là **8 mm** để cắt CNC mặt máy. Cột phải tách **nguồn (trên)** và **MCU + RS485 (dưới)**. Giắc hướng ra cạnh phải. **Linh kiện đã xếp lại theo nhóm, chưa đi dây** (autorouter PathFinder — xem mục Đi dây tự động; bản đã đi dây trước đó nằm ở scratchpad, không còn trong repo).

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
| 2× module 4 số 0,56" CC | `Panel:Display_4digit_0.56in_THT` (5641AS) | Thân **50,4 × 19,05 mm**, hàng chân **15,24 mm**, bước digit 12,7 mm, tâm X=**40**. DS1 **Y=15** (digit 1–4), DS2 **Y=35** (digit 5–8) — tâm nguyên mm cho lỗ phay |
| Keypad 4×4 | `Panel:SW_Tactile_12mm_THT` | SW13 **Y=59** … SW1 **Y=107**, pitch **16**, tâm X=**42** (dịch phải 2 mm so với 7-seg để lỗ phay vuông không sát lỗ vít M3). Tact B3F-4055: 4 chân bước 12,5 × 5 (lỗ Ø1,3), 2 lỗ định vị NPTH Ø1,7 cách nhau 9 mm; hai chân cùng hàng cách 12,5 mm là một cực (theo sơ đồ Omron); SW1–SW12 chân 12,5 dọc theo Y, **SW13–SW16 xoay chân 12,5 dọc theo X** (tránh còi BZ1) — chỉ khác chân, thân và mũ vuông như nhau. Khe thân tới 7-seg **8,5 mm** |
| 8× LED trạng thái | `Panel:LED_3mm_THT` | Cột **X=80**, D10 **Y=12** → D17 **Y=68** |
| LED driver | **U6 TM1638** trên **Top** | SOP-28, dưới cột LED, 3 dây STB/CLK/DIO tới STM32 |

**Mọi tâm phím / LED / module nằm trên số nguyên mm** để lỗ phay mặt máy có kích thước và tọa độ nguyên mm. Lỗ phay theo biến thể: `docs/CNC_MAT_MAY.md`. Kiểm tra: `python tools/verify_front_layout.py` (dung sai 0,01 mm) và `python tools/cnc_panel.py --check`.

## Mặt trên — chip, nguồn, MCU (F.SilkS)

Khe giữa hai phím khoảng 3,6 mm, không đặt 0805. Cột phải rộng khoảng 36 mm, chia đôi theo chiều dọc: nguồn ở nửa trên, MCU và RS485 ở nửa dưới, silk để hở giữa hai vùng.

```
X=0    cạnh trái, lỗ M3 tại X=5
X=7.0…9.6  khe phay đuôi bàn phím màng (Y 15.5…43.5)
X=11.4 J_KEY header 1×10 (bàn phím màng), mặt trên
X=70…88  CLED1–3 và RSTB/RCLK/RDIO sát U6
X=40   tâm 7-seg (keypad X=42); còi BZ1 tâm (71, 36) ở vùng trống mặt trên
X=78   U6 TM1638 (Y=86)
X=95   U1 STM32G030F6P6 (Y=86), CVDD1/CNRST bên phải, J_SWD (93,2 / 97,2) và J_LOG (97,6 / 97,2) phía dưới
X=80   LED trạng thái
X=82…118  BUCK (Y 4–46), 24V+LDO (Y 48–76), MCU+RS485 (Y 78–116)
X=115  J_PWR (Y=16) và J485 (Y=102), hướng ra cạnh phải
X=120  cạnh phải, lỗ M3 tại X=115
Y=120  cạnh dưới, lỗ M3 tại Y=115
```

| Nhóm (khung silk F.SilkS) | X (mm) | Y (mm) | Linh kiện |
|---------------------------|--------|--------|-----------|
| **INPUT 24V** | 83–118,5 | 4–34,5 | J_PWR, F1, FBIN, TVS1, C19/C15, DREV, C4, C16/C17 |
| **BUCK 5V** | 82–118,5 | 36–64 | U4, L1, D8, C2, C22, C5, FB2, FB5V, NT1 |
| **TM1638** | 70–90,5 | 70,5–102 | U6, CLED1–3, RSTB/RCLK/RDIO |
| **MCU** | 90,5–103,5 | 76–108,5 | U1, CVDD1, CNRST, J_SWD, J_LOG |
| **RS485** | 103,5–118,5 | 72–116,5 | U3, R47A/B, CM3, RDE, TVS2, J485, RTERM/JTERM, RBIAS_A/B |
| **LDO 3V3** | 76–106 | 108,5–117,5 | U5, C7, C18, C3, FB1 |
| BUZZER (còi báo phím) | 64,5–77,5 | 28,5–50,5 | BZ1 tâm (71, 36), QBZ, RBZG — vùng trống mặt trên, không che chân phím nào |
| Giắc màng | 7–13 | 15,5–43,5 | J_KEY + khe phay 2,6 × 28 |

Dòng công suất chảy từ trên xuống (INPUT → BUCK → LDO); giữa BUCK và TM1638 chừa dải Y 64–70 làm kênh đi dây. Khe giữa các courtyard ≥ 1 mm (`placement_issues`), mặt trên không còn chồng nhau. Đặt lại linh kiện theo nhóm: `python tools/place_groups.py` (xóa toàn bộ dây, đi lại bằng autorouter). Hiện bo **chưa đi dây**.

Chi tiết tọa độ: `tools/zone_layout.py`. Vẽ viền vùng: `tools/fix_front_layout.py`.

## Model 3D

Thư mục `hardware/kicad/panel-keyboard/3d/` (sinh bởi `python tools/make_3d_models.py`, VRML): `SW_B3F4055_A14.wrl` (tact B3F-4055 + mũ A14 10×10 + chụp trong 11,8×11,8, tổng cao ≈ 9 mm), `DS_5641AS.wrl` (module 4 số 0,56" có đủ 7 đoạn + chấm). LED 3 mm dùng `LED_D3.0mm` của KiCad; cọc vít dùng Phoenix MKDS 5,08; header dùng PinHeader 2,54. SW13–SW16 gán xoay model 90° theo hướng chân. Xem bằng View → 3D Viewer, hoặc `kicad-cli pcb render --side bottom` (mặt dưới = mặt nhìn từ cánh tủ).

## Giắc bàn phím màng (J_KEY)

Tọa độ dưới đây là tọa độ KiCad (nhìn từ mặt chip). **Nhìn từ mặt tủ thì giắc nằm ở cạnh phải** (X lật gương).

Vì sao đặt ở dải trái (KiCad), cạnh module 7 đoạn:

- Đuôi màng không vòng qua mép bo được (sau mép bo là tôn cánh tủ và trụ đệm), nên đi xuyên **khe trên cánh tủ 4 × 29 mm** rồi **khe phay trên bo** thẳng phía sau. Khe cánh tủ chỉ có ở biến thể màng (`cnc_panel.py`, biến thể `…-M`).
- Dải X < 14,8 là chỗ duy nhất **trống cả hai mặt** đủ dài 28 mm: mặt dưới không có thân 7 đoạn / phím / LED, mặt trên không có linh kiện. Khe cánh tủ cách cửa sổ 7 đoạn 4 mm (gân tôn ≥ 3 mm).
- Header ở **mặt trên** như J_PWR/J485: cắm/rút từ trong tủ khi bo đã bắt vít, thay tấm màng không phải tháo bo.
- Nằm đối diện hai giắc 24 V / RS485 → cáp nguồn, cáp bus và đuôi màng không bắt chéo; dây quét phím xa vùng buck.

Thứ tự lắp: dán màng lên mặt tủ, luồn đuôi qua khe tủ và khe bo → bắt 4 vít M3 → vòng đuôi (bán kính ≥ 3 mm, **không gấp mép**) và cắm vào J_KEY. Đuôi màng cần dài thêm ≈ 20 mm + chiều cao trụ đệm. Mài ba-via mép khe tôn.

Khi dùng màng: không lắp 16 tact SW1–SW16 (hoặc ngược lại) — hai bộ nối song song.

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
- [x] Lỗ phay mặt tủ + decal theo module 4 số, mọi biến thể nguyên mm (`docs/CNC_MAT_MAY.md`)
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
