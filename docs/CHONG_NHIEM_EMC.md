# Review chống nhiễu (EMC/ESD) — Panel 24V + RS485

Môi trường: tủ điện công nghiệp (contactor, biến tần, mạt sắt), nguồn **24 V DC** bus dài, đường truyền **RS485 / Modbus**.

## 1. Đánh giá thiết kế ban đầu

| Khu vực | Đã có | Thiếu / rủi ro | Mức ưu tiên |
|---------|--------|----------------|-------------|
| Đầu vào 24V | **PWR_24V_PROTECT** mức 1: F1, **L2 CM**, **RV1 MOV**, TVS1, C19/C15, Q12, **C4 1000µF**, NT1 (`PWR_24V_PROTECT_BUCK_GUIDE.txt`) | Dây nguồn xoắn, tách motor; shield 1 đầu | Trung bình |
| Buck 24→5V | L + C out | Thiếu tụ sát chân IN/SW/OUT; vùng GND không quy định via stitch; xung SW couple sang RS485 | **Cao** |
| 3.3V MCU | AMS1117 + tụ cơ bản | VDDA tách bằng ferrite; NRST/BOOT lọc nhiễu; decouple từng VDD STM32 | Trung bình |
| TM1638 (CLK/STB/DIO) | Pull-up “optional” | Điện trở series chưa cố định; quét LED 7-seg gây sụt 5V | Trung bình |
| RS485 | TVS A/B, 120Ω, 470Ω UART | Thiếu **bias failsafe** khi bus rời; thiếu **CM choke**; TVS 6V chung chưa tối ưu cho line differential; DE mặc định RX | **Cao** |
| Mạch phím | THT + RK/RC/CK/CC @ TM1638, khe MCU↔buck | Phím ma do nhiễu buck — xem `CHONG_PHIM_MA.md` | **Cao** |
| PCB | GND pour bottom | Chưa tách **return** nguồn / RS485 / digital; RS485 chưa guard / diff pair | **Cao** |

## 2. Nguyên tắc bố trí (layout)

1. **Điểm sao GND**: **GND_PWR** tại **J_PWR pin 2** và **NT1** (C4−) — return buck/24V về **GND_PWR**; logic (MCU, RS485 SG) về **GND** qua NetTie.
2. **Luồng năng lượng**: `J_PWR → F1 → TVS → Q12 → bulk → XL1509` gần **J_PWR** (cạnh dưới PCB); **MAX485 + CM choke + TVS** sát **J485**, đường A/B ngắn, **không song song** dây SW buck (`+5V_SW`).
3. **RS485**: route **A/B cặp differential** (mục tiêu ~120 Ω diff nếu có thể), **via stitch GND** hai bên, **keep-out 2 mm** dưới vùng connector.
4. **Buck**: đặt **100nF + 10µF** trong vòng **2 mm** chân VIN/FB/GND; **≥ 8 via GND** quanh IC và cuộn cảm.
5. **STM32 + TM1638**: decouple 100nF **mỗi chân nguồn**; bus TM1638 **≤ 30 mm**, đi kèm **GND guard** phía dưới.

## 3. Linh kiện bổ sung (tóm tắt)

Chi tiết refdes xem `hardware/bom.csv` và sơ đồ cập nhật trong `SO_DO_NGUYEN_LY.md`.

| Chức năng | Linh kiện | Ghi chú |
|-----------|-----------|---------|
| OCP | **F1** MF-MSMF350 | PPTC 2920 |
| CM đầu vào | **L2** ACM2012-201 | Cả +24V và return qua CM |
| Clamp chậm | **RV1** MOV ~26V | Song song **VIN_PROTECT** |
| Clamp nhanh | **TVS1** SMAJ28A | → **GND_PWR** |
| Anti-reverse | **Q12** + R17 | Tại VIN_PROTECT |
| Bulk 24V | **C4** 1000µF + C14/C19/C15 | NT1 sao mass |
| Rail 5V | **FB2** + **C22** 220µF; **FB5V** → +5V_LED | Theo guide + nhánh display |
| Rail 3,3V | **FB1** + **C21** | Sau AMS1117 |
| NRST/BOOT | 10k + **100nF** NRST; 10k BOOT0→GND | Chống reset giả do nhiễu |
| TM1638 bus | **33Ω ×3** CLK/STB/DIO + **4.7k** pull-up DIO | Giảm overshoot, mặc định idle |
| RS485 ESD | **SM712** (hoặc 2× SMBJ6.0CA) tại connector | Bảo vệ A/B so với GND |
| RS485 CM | **Common-mode choke 90Ω CM** (dòng ≥100mA) | Trước TVS, sau IC |
| Failsafe bias | **560Ω** A→+5V, **560Ω** B→GND (sau choke) | Bus idle khi không master |
| Termination | **120Ω** + jumper **JTERM** | Chỉ bật ở node cuối bus |
| UART | **47Ω** series TX/RX (thay 470Ω — 470 quá lớn cho edge rate) | Giảm overshoot, vẫn đủ drive |
| DE/~RE | **10k pulldown** + 0Ω series GPIO | Mặc định **nhận**, tránh bus chiếm |
| MAX485 decap | 100nF sát VCC | Bắt buộc |

## 4. RS485 — sơ đồ khối sau cải tiến

```
MAX485 A/B ──► [CM choke] ──► (560R A→5V, 560R B→GND) ──► [SM712] ──► J485 A/B/SG
                    │                              │
                    └── [JTERM 120R A-B] (optional)
```

## 5. Đầu vào nguồn — sau cải tiến

```
J_PWR ── F1 ── L2 CM ── VIN_PROTECT (MOV||TVS||Q12) ── C4 ── XL1509
J_PWR GND ═══ GND_PWR ═══ NT1 ═══ GND
+5V_RAW ── FB2 ── +5V (C22) ── FB5V ── +5V_LED
+3V3_LDO ── FB1 ── +3V3 (C21)
```

## 6. Kiểm thử gợi ý (sau lắp ráp)

| Test | Tiêu chí |
|------|----------|
| Ripple 5V | < 100 mVpp @ full LED + quét 7-seg (oscilloscope, GND spring tại TM1638) |
| ESD RS485 | ±4 kV contact vào A/B (IEC 61000-4-2) — không reset MCU, bus phục hồi |
| Burst 24V | Couple 1 kV vào dây nguồn dài — không treo firmware |
| Modbus | Đọc/ghi register liên tục khi contactor đóng cắt gần bo |

## 7. Những gì **không** khuyến nghị (tránh over-engineering)

- Cách ly DC-DC riêng cho RS485 (chỉ cần nếu audit EMC bắt buộc Class B dây ngoài).
- TVS từng phím bấm (decal + ESD từ người vận hành — xử lý firmware debounce + layout GND là đủ).
- Cap lớn A–B song song (dễ lệch biên Modbus tốc cao) — chỉ dùng CM choke + TVS đúng chỗ.
