# Giắc cắm PCB + cáp — tương thích PLC

Nguồn và RS485 dùng **cọc vít (terminal vít) bước 5,08 mm** hàn thẳng vào bo, dây đấu thẳng bằng vít (đầu cos 0,5 mm² hoặc dây trần). **Tách 2 giắc**: nguồn và RS485 — dễ đấu dây, giảm nhầm 24 V lên bus Modbus. Các giắc còn lại (bàn phím màng, nạp, log) là **header chân đực 2,54 mm**.

## 1. Trên PCB (mặt trên bo, trong tủ)

| Ref | Loại | Linh kiện / mã | Footprint | Ghi chú silk |
|-----|------|----------------|-----------|--------------|
| **J_PWR** | Cọc vít **2P**, bước **5,08 mm**, cửa dây ngang ra cạnh phải bo | KF301-5.08-2P (Kefa/Degson) | `Panel:TerminalBlock_2P_5.08` (lỗ Ø1,3, pad 2,4) | `+24V` / `GND` |
| **J485** | Cọc vít **3P**, bước **5,08 mm**, cửa dây ngang ra cạnh phải bo | KF301-5.08-3P (Kefa/Degson) | `Panel:TerminalBlock_3P_5.08` (lỗ Ø1,3, pad 2,4) | `A` / `B` / `SG` |
| **J_KEY** | Header **đực** 1×10, bước 2,54 mm, thẳng | Pin header đực 1x10 | `Panel:Membrane_1x10_P2.54_Vertical_Slot` (header + khe phay 2,6 × 28 mm) | `MEMBRANE`, chân 1 = K1 |
| **J_SWD** | Header **đực** 1×4, 2,54 mm, thẳng | Pin header đực 1x4 | `PinHeader_1x04_P2.54mm_Vertical` | 1 +3V3, 2 SWDIO, 3 SWCLK, 4 GND |
| **J_LOG** | Header **đực** 1×4, 2,54 mm, thẳng | Pin header đực 1x4 | `PinHeader_1x04_P2.54mm_Vertical` | 1 +3V3, 2 TX chip (PB6), 3 RX chip (PB7), 4 GND |

Mô hình 3D: cọc vít dùng `TerminalBlock_Phoenix MKDS-1,5-N-5.08 Horizontal` của KiCad (tương đương hình dạng KF301), header dùng `PinHeader_2.54mm Vertical`. Tất cả đã gán vào footprint và PCB.

**J_KEY — bàn phím màng:** đặt mặt trên, cạnh trái theo KiCad = cạnh phải nhìn từ mặt tủ (X=11,4, Y 18–41); đuôi màng đi xuyên khe cánh tủ 4 × 29 mm rồi khe phay PCB ngay cạnh header. Chân: 1 K1, 2 K2, 3–10 KS1–KS8 (xem `SO_DO_NGUYEN_LY.md` mục 5). Lý do vị trí: `PCB_LAYOUT.md` mục *Giắc bàn phím màng*.

**Hướng lắp:** cọc vít hàn **mặt trên (F.Cu)**, cùng phía với chip, thân sát cạnh phải bo, cửa dây hướng ra cạnh phải (tủ), vít xiết nhìn từ trên xuống. Header đực J_KEY / J_SWD / J_LOG hàn mặt trên, thẳng đứng.

## 2. Sơ đồ chân

### J_PWR (2P)

| Pin | Net | Nối tới |
|-----|-----|---------|
| 1 | +24V_IN | F1 → FBIN → VIN_PROTECT (TVS) → DREV → C4 → buck (`PWR_24V_PROTECT_BUCK_GUIDE.txt`) |
| 2 | GND | **STAR GND** (mọi return về đây trước) |

### J485 (3P)

| Pin | Net | Nối tới |
|-----|-----|---------|
| 1 | RS485_A_BUS | TVS2, JTERM, bias 560R |
| 2 | RS485_B_BUS | TVS2, JTERM, bias 560R |
| 3 | SG | Signal GND / reference Modbus (nối plane GND tại **một điểm** gần J485, không tách đảo nguồn) |

*Pin 3 = reference cho bias failsafe và shield cáp — cùng potential với GND tủ tại panel.*

## 3. Dây đấu (đấu thẳng vào cọc vít)

Hai đường dây riêng; đầu PLC vẫn dùng terminal vít + đầu cos (ferrule 0,5 mm²). Đầu panel đấu thẳng vào cọc vít, không cần phích.

### Dây 1 — Nguồn (J_PWR)

| Hạng mục | Thông số |
|----------|----------|
| Dây | **2 × 0,5 mm²** (AWG 20), đỏ (+24V) / đen (0V) |
| Chiều dài | Theo tủ (thường 0,3–2 m) |
| Đầu panel | Đầu cos pin 0,5 mm² (ferrule) xiết vào cọc vít J_PWR |
| Đầu PLC | Cos tròn **0,5 mm²** → terminal **+24V / 0V** nguồn tủ (cùng CB phụ tải panel) |

### Dây 2 — Modbus RS485 (J485)

| Hạng mục | Thông số |
|----------|----------|
| Giắc | Cọc vít **5,08 mm, 3 pin** |
| Cáp | **Belden 9841** hoặc tương đương: **1 pair xoắn + shield**, 24 AWG (0,5 mm²) |
| Ghép màu (gợi ý) | Pin1 Xanh → **A**; Pin2 Trắng → **B**; Pin3 Drain bọc → **SG** |
| Đầu PLC | A/B (D+/D−) + SG/0V theo manual; **shield nối 1 điểm** tại tủ/PLC |

**Thứ tự chân J485 (pin dưới lên theo hướng nhìn từ cạnh phải bo):** pin **1 = A**, **2 = B**, **3 = SG** — ghi rõ silk để thợ không đảo.

**Không** gom 24 V chung với pair A/B trong **một giắc 5P** — dễ nhầm khi bảo trì.

## 3.1 Mã linh kiện cọc vít (thay thế tương đương)

| Vai trò | Kefa/Degson (phổ biến VN) | Ghi chú |
|---------|---------------------------|---------|
| J_PWR (2P) | KF301-5.08-2P | Bước 5,08 mm, chân Ø1,0–1,3 |
| J485 (3P) | KF301-5.08-3P | Cùng bước 5,08, 3P |

## 4. DB9 / “cổng COM” — khác gì Modbus trên PLC?

**Cổng COM trên PC** (DB9 cái) gần như luôn là **RS-232**, không phải **RS-485**:

| | RS-232 (COM / DB9 PC) | RS-485 (Modbus PLC / panel bạn) |
|--|------------------------|----------------------------------|
| Điện | 1 máy ↔ 1 thiết bị, ± điện áp lớn | Bus **A/B** differential, nhiều slave |
| Giắc | **DB9** (DE-9) quen thuộc | **Cọc vít** 5,08 mm |
| Cáp | Dây thẳng / null-modem | **Cặp xoắn** + bọc |
| Khoảng cách | Vài mét | Hàng trăm mét |

Bạn hay thấy DB9 vì **USB–RS232**, **HMI cũ**, **máy CNC**, **cân điện tử** — một đầu DB9, **một thiết bị một cáp**, không phải bus tủ điện.

### Khi **nên** dùng DB9

1. **Panel / hộp nối với PC** qua **USB converter RS232** (debug, cấu hình tool), không phải bus Modbus tủ.  
2. **Thiết bị bán sẵn** ghi rõ *“RS-232 DB9 male/female, PC COM”* — bắt buộc theo hãng.  
3. **Cáp dịch có sẵn** (DB9 ↔ DB9 null modem) — chỉ khi tầng vật lý vẫn là **RS-232**.  
4. **RS-485 qua DB9** chỉ khi **manual ghi pinout** (A/B/GND) và **cả hai đầu** cùng chuẩn nội bộ (ví dụ một số **USB–RS485** dongle: DB9 pin 3=A, pin 8=B — *không thống nhất giữa hãng*).

### Khi **không** nên DB9 (panel All-in-One gắn tủ)

- PLC Modbus: **terminal A/B**, không DB9.  
- Cần **24 V** riêng — DB9 không có chuẩn nguồn công nghiệp.  
- Nhiều slave trên một bus — RS-485 + cáp xoắn, không COM 1:1.

**Panel dự án này:** giữ **J_PWR + J485 cọc vít 5,08 mm**; nếu cần debug laptop, dùng **USB–RS485** đấu dây vào **cùng A/B** tại tủ (hoặc hộp phụ), không thay J485 bằng DB9 trên face panel.

## 5. So sánh giắc panel (tóm tắt)

| Giải pháp | Đánh giá |
|-----------|----------|
| **2 cọc vít 5,08 mm** (đang dùng) | Trùng thói quen tủ điện; dây tách nguồn/signal; PLC đấu cos — **phù hợp nhất** |
| 1 giắc 4P (24V,GND,A,B) | Gọn nhưng dễ cắm nhầm, EMC kém hơn |
| DB9 trên mặt panel Modbus | Chỉ khi có **chuẩn cáp nội bộ**; dễ nhầm với COM RS-232 |

## 6. Kiểm tra sau lắp

1. J_PWR: đo **+24V** pin 1 so với pin 2 trước khi đấu J485.  
2. J485 idle: đo **A − B ≈ +200 mV ~ +5 V** (bias failsafe, tùy PLC).  
3. Modbus: **9600 8N1** (hoặc đúng cấu hình PLC), đảo A/B nếu không online.

## 7. Tọa độ PCB

Xem `docs/PCB_LAYOUT.md` — **J_PWR** (115, 16) và **J485** (115, 102) mm, cạnh phải bo.
