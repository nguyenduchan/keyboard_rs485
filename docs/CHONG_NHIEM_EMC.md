# Review chống nhiễu (EMC/ESD) — Panel 24V + RS485

Môi trường: tủ điện công nghiệp (contactor, biến tần, mạt sắt), nguồn **24 V DC** bus dài, đường truyền **RS485 / Modbus**.

## 1. Đánh giá thiết kế ban đầu

| Khu vực | Đã có | Thiếu / rủi ro | Mức ưu tiên |
|---------|--------|----------------|-------------|
| Đầu vào 24V | **PWR_24V_PROTECT** (rev C): F1 PPTC, **FBIN** ferrite, TVS1, C19/C15, **DREV** SS34, **C4 470µF/35V**, NT1 (`PWR_24V_PROTECT_BUCK_GUIDE.txt`) | Dây nguồn xoắn, tách motor; shield 1 đầu | Trung bình |
| Buck 24→5V | L + C out | Thiếu tụ sát chân IN/SW/OUT; vùng GND không quy định via stitch; xung SW couple sang RS485 | **Cao** |
| 3.3V MCU | AMS1117 + tụ cơ bản | VDDA tách bằng ferrite; NRST lọc nhiễu; decouple VDD STM32G030 | Trung bình |
| Quét 7-seg | TM1638 trên +5V_LED, 3 dây STB/CLK/DIO | Dòng GRID đỉnh khoảng 150 mA, quét 1/8 | Trung bình |
| RS485 | TVS A/B, 120Ω, 470Ω UART | Thiếu **bias failsafe** khi bus rời; thiếu **CM choke**; TVS 6V chung chưa tối ưu cho line differential; DE mặc định RX | **Cao** |
| Mạch phím | THT, TM1638 quét (K1/K2 pull-down nội) | Phím ma do nhiễu buck — xem `CHONG_PHIM_MA.md` | **Cao** |
| PCB | GND pour bottom | Chưa tách **return** nguồn / RS485 / digital; RS485 chưa guard / diff pair | **Cao** |

## 2. Nguyên tắc bố trí (layout)

1. **Điểm sao GND**: **GND_PWR** tại **J_PWR pin 2** và **NT1** (C4−) — return buck/24V về **GND_PWR**; logic (MCU, RS485 SG) về **GND** qua NetTie.
2. **Luồng năng lượng**: `J_PWR → F1 → FBIN → TVS → DREV → bulk → XL1509` trong **khối nguồn nửa trên cột phải**; **SIT3485 + TVS + bias** sát **J485** trong **khối MCU phía dưới**, đường A/B ngắn, **không song song** dây SW buck (`+5V_SW`).
3. **RS485**: route **A/B cặp differential** (mục tiêu ~120 Ω diff nếu có thể), **via stitch GND** hai bên, **keep-out 2 mm** dưới vùng connector.
4. **Buck**: đặt **100nF + 10µF** trong vòng **2 mm** chân VIN/FB/GND; **≥ 8 via GND** quanh IC và cuộn cảm.
5. **STM32**: 100nF trên VDD; dây SEG/GRID ngắn, kèm GND phía dưới.

## 3. Linh kiện bổ sung (tóm tắt)

Chi tiết refdes xem `hardware/bom.csv` và sơ đồ cập nhật trong `SO_DO_NGUYEN_LY.md`.

| Chức năng | Linh kiện | Ghi chú |
|-----------|-----------|---------|
| OCP | **F1** MF-MSMF350 | PPTC 2920 |
| CM đầu vào | **L2** ACM2012-201 | Cả +24V và return qua CM |
| Clamp chậm | (rev C bỏ MOV RV1 — TVS1 SMAJ28A đảm nhận) | — |
| Clamp nhanh | **TVS1** SMAJ28A | → **GND_PWR** |
| Anti-reverse | **DREV** SS34 | Sau VIN_PROTECT |
| Bulk 24V | **C4** 1000µF; C19 + **C15** sát TVS1 | NT1 sao mass |
| Rail 5V | **FB2** + **C22** 220µF; **FB5V** → +5V_LED | Theo guide + nhánh display |
| Rail 3,3V | **FB1** + **CVDD1** sát VDD | Sau AMS1117 |
| NRST | 10k + **100nF** NRST | Chống reset giả do nhiễu. G030 không có BOOT0 riêng |
| Segment | **U6 TM1638** | SEG nối thẳng anode, GRID hút cathode |
| RS485 ESD | **SM712** (hoặc 2× SMBJ6.0CA) tại connector | Bảo vệ A/B so với GND |
| RS485 CM | **Common-mode choke 90Ω CM** (dòng ≥100mA) | Trước TVS, sau IC |
| Failsafe bias | **560Ω** A→+5V, **560Ω** B→GND (sau choke) | Bus idle khi không master |
| Termination | **120Ω** + jumper **JTERM** | Chỉ bật ở node cuối bus |
| UART | **47Ω** series TX/RX (thay 470Ω — 470 quá lớn cho edge rate) | Giảm overshoot, vẫn đủ drive |
| DE/~RE | **10k pulldown** + 0Ω series GPIO | Mặc định **nhận**, tránh bus chiếm |
| SIT3485 decap | 100nF sát VCC | Bắt buộc |

## 4. RS485 — sơ đồ khối sau cải tiến

```
SIT3485 A/B ──► (560R A→5V, 560R B→GND) ──► [SM712] ──► J485 A/B/SG
                    │                              │
                    └── [JTERM 120R A-B] (optional)
```

## 5. Đầu vào nguồn — sau cải tiến

```
J_PWR ── F1 ── FBIN ── VIN_PROTECT (TVS) ── DREV ── C4 ── XL1509
J_PWR GND ═══ GND_PWR ═══ NT1 ═══ GND
+5V_RAW ── FB2 ── +5V (C22) ── FB5V ── +5V_LED
+3V3_LDO ── FB1 ── +3V3 (CVDD1 tại chân VDD)
```

## 6. Kiểm thử gợi ý (sau lắp ráp)

| Test | Tiêu chí |
|------|----------|
| Ripple 5V | < 100 mVpp khi quét 7-seg (oscilloscope, GND spring tại CLED) |
| ESD RS485 | ±4 kV contact vào A/B (IEC 61000-4-2) — không reset MCU, bus phục hồi |
| Burst 24V | Couple 1 kV vào dây nguồn dài — không treo firmware |
| Modbus | Đọc/ghi register liên tục khi contactor đóng cắt gần bo |

## 7. Những gì **không** khuyến nghị (tránh over-engineering)

- Cách ly DC-DC riêng cho RS485 (chỉ cần nếu audit EMC bắt buộc Class B dây ngoài).
- TVS từng phím bấm (decal + ESD từ người vận hành — xử lý firmware debounce + layout GND là đủ).
- Cap lớn A–B song song (dễ lệch biên Modbus tốc cao) — chỉ dùng CM choke + TVS đúng chỗ.
