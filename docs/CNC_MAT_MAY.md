# Lỗ phay CNC mặt tủ theo biến thể

Một PCB dùng cho mọi biến thể; chỉ khác số linh kiện mặt trước được lắp. Lỗ phay mặt tủ sinh tự động bởi `hardware/kicad/panel-keyboard/tools/cnc_panel.py` từ cùng tọa độ với PCB (`front_layout_spec.py`, `zone_layout.py`), nên sửa layout là bản vẽ CNC đổi theo.

```text
python hardware/kicad/panel-keyboard/tools/cnc_panel.py --check            # kiểm tra cả 162 biến thể
python hardware/kicad/panel-keyboard/tools/cnc_panel.py --standard         # xuất các biến thể chuẩn
python hardware/kicad/panel-keyboard/tools/cnc_panel.py D1-L6-K12          # xuất một biến thể bất kỳ
```

Kết quả nằm ở `hardware/mechanical/cnc/`: `<biến thể>.dxf` (đưa máy CNC, lớp `CUT` = lỗ phay, `MOUNT` = lỗ vít, `BOARD_REF` = viền bo để tham chiếu, không cắt), `.svg` (xem / in), `.csv` (bảng tọa độ), `_decal.svg` (decal 120 × 120).

## Tên biến thể

`D<module>-L<led>-<phím>`

| Phần | Giá trị | Lắp linh kiện nào |
|------|---------|-------------------|
| `D0` / `D1` / `D2` | 0, 1, 2 module 4 số | `D1` = chỉ DS1 (hàng trên, digit 1–4) |
| `L0` … `L8` | số LED trạng thái | lắp từ D10 (trên cùng) xuống |
| `K0` / `K4` / `K8` / `K12` / `K16` | số phím tact | theo hàng, từ hàng sát 7 đoạn (SW13–SW16) xuống |
| `M` | bàn phím màng qua J_KEY | không lắp tact; mặt tủ vùng phím để nguyên làm tấm đỡ |

Biến thể chuẩn đã xuất: `D2-L8-K16`, `D2-L8-M`, `D2-L8-K0`, `D1-L4-K16`, `D1-L4-M`, `D2-L0-K16`, `D0-L8-K16`, `D0-L0-M`.

## Hệ tọa độ bản vẽ

**Nhìn từ mặt tủ**, gốc ở góc trên-trái vùng bo 120 × 120, mm. Vì phím / LED / 7 đoạn nằm ở mặt dưới PCB nên X lật gương so với KiCad: `X_mặt_tủ = 120 − X_KiCad`, Y giữ nguyên. DXF dùng Y hướng lên (`Y_dxf = 120 − Y`).

## Lỗ phay (D2-L8-K16, nhìn từ mặt tủ)

| Lỗ | Kích thước | Vị trí | Linh kiện / khe hở |
|----|-----------|--------|--------------------|
| Cửa sổ 7 đoạn, 2 module | 52 × 40 | X 54…106, Y 5…45 | thân 50,4 × 39,05; hở 0,8 ngang / 0,48 dọc |
| Cửa sổ 7 đoạn, 1 module | 52 × 20 | X 54…106, Y 5…25 | thân 50,4 × 19,05 |
| Phím | vuông 13 × 13 | tâm X 54 / 70 / 86 / 102 (mặt tủ), Y 59 / 75 / 91 / 107 | chụp trong vuông 11,8 × 11,8; hở 0,6 |
| LED | Ø4 | tâm X 40, Y 12 / 20 / … / 68 | LED 3 mm; hở 0,5 |
| Khe đuôi màng (chỉ `-M`) | 4 × 29 | X 110…114, Y 15…44 | trùng khe PCB 2,6 × 28 |
| Lỗ vít M3 | Ø3 | tâm (5, 5), (115, 5), (5, 115), (115, 115) | định vị bo |

Hai hàng module chỉ cách nhau 0,95 mm nên dùng chung một cửa sổ (gân 0,95 mm thì tôn không đứng được).

## Quy tắc kiểm tra (mọi biến thể)

- Mọi kích thước và tọa độ **nguyên mm**: tâm lỗ tròn và đường kính nguyên; mép lỗ chữ nhật nguyên. Vì vậy tâm phím, LED, module trên PCB phải nằm trên số nguyên mm (rev C đã dời 7 đoạn Y 15,375 / 35,475 → 15 / 35 và LED X 79,5 → 80).
- Khe hở linh kiện ↔ mép lỗ **0,25…1,0 mm** mỗi phía (lọt được khi lệch lỗ vít, vẫn khít).
- Gân tôn giữa hai lỗ **≥ 3 mm**; lỗ cách mép vùng bo ≥ 3 mm; lỗ cách tâm vít đủ chỗ cho trụ đệm lục giác 5,5 mm + gân 3 mm.

Kết quả hiện tại: **162/162 biến thể đạt**.

## Giả định cần xác nhận

- Mũ phím A14 vuông (lõi 10 × 10, cao 4,8) kèm chụp trong; chụp trong ngoài 11,8 × 11,8, cao 3,9 (`KEY_CAP`, theo bản vẽ nhà bán, nên đo lại mẫu thật); lỗ vuông 13 × 13 (`KEY_HOLE`). Bàn phím đã dịch +2 mm (`KEY_AXIS_X` = 42) để góc lỗ vuông cách lỗ vít MH3 đủ gân.
- Lỗ vít Ø3 vừa khít để định vị bo (vít M3 thường lọt; vít to thì doa Ø3,2). Lỗ Ø4 sẽ làm cửa sổ lệch tới 0,5 mm.
- Bản vẽ chỉ có mặt phẳng X–Y. Chiều cao trụ đệm (để mặt module 7 đoạn, nắp phím và LED ngang mặt tủ) phụ thuộc độ dày tôn và loại nắp phím, chưa chốt.
