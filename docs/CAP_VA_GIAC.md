# Giắc cắm PCB + cáp thờ công — tương thích PLC

Thiết kế dùng **terminal dạng cắm rút 5,08 mm** (chuẩn tủ điện, trùng bước với nhiều module PLC I/O). **Tách 2 giắc**: nguồn và RS485 — dễ làm cáp, giảm nhầm 24 V lên bus Modbus.

## 1. Trên PCB (mặt trên bo, trong tủ)

| Ref | Loại | Footprint KiCad (gợi ý) | Ghi chú silk |
|-----|------|-------------------------|--------------|
| **J_PWR** | Socket **2P**, pitch **5,08 mm**, cắm rút | `TerminalBlock_Phoenix:PhoenixContact_MC_1,5_2-G-5.08_1x02_P5.08mm_Vertical` hoặc Degson **2EDGVC-5.08-02P** | `+24V` / `GND` |
| **J485** | **Degson 2EDG-5.08**, socket **3P** cắm rút (chuẩn dự án) | PCB: **2EDGVC-5.08-03P** (Vertical); KiCad: footprint `2EDGVC-5.08-03P` / Degson lib | `A` / `B` / `SG` |

**Hướng lắp:** socket hàn **mặt trên (F.Cu)**, cùng phía với chip, miệng cắm hướng vào trong tủ. Thợ cắm phích từ phía chip, không cần tháo ốc panel.

**Màu phích (khuyến nghị mua sẵn):**

- J_PWR: phích **cam/vàng** (2P) — nguồn  
- J485: phích **xanh lá** (3P) — truyền thông  

## 2. Sơ đồ chân

### J_PWR (2P)

| Pin | Net | Nối tới |
|-----|-----|---------|
| 1 | +24V_IN | F1 → L2 CM → VIN_PROTECT (MOV/TVS/Q17) → C4 → buck (`PWR_24V_PROTECT_BUCK_GUIDE.txt`) |
| 2 | GND | **STAR GND** (mọi return về đây trước) |

### J485 (3P)

| Pin | Net | Nối tới |
|-----|-----|---------|
| 1 | RS485_A | Sau L485, TVS2, JTERM |
| 2 | RS485_B | Sau L485, TVS2, JTERM |
| 3 | SG | Signal GND / reference Modbus (nối plane GND tại **một điểm** gần J485, không tách đảo nguồn) |

*Pin 3 = reference cho bias failsafe và shield cáp — cùng potential với GND tủ tại panel.*

## 3. Cáp thờ công (phổ biến, đấu thẳng PLC terminal)

Hai **dây cáp riêng**, mỗi đầu panel gắn **phích 5,08 mm**; đầu PLC **ốc vít + đầu cos** (ferrule 0,5 mm²).

### Cáp 1 — Nguồn (J_PWR)

| Hạng mục | Thông số |
|----------|----------|
| Dây | **2 × 0,5 mm²** (AWG 20), đỏ (+24V) / đen (0V) |
| Chiều dài | Theo tủ (thường 0,3–2 m) |
| Phích panel | Degson **2EDGK-5.08-02P** + đuôi hàn/crimp |
| Đầu PLC | Cos tròn **0,5 mm²** → terminal **+24V / 0V** nguồn tủ (cùng CB phụ tải panel) |

### Cáp 2 — Modbus RS485 (J485) — **2EDG-5.08**

| Hạng mục | Thông số |
|----------|----------|
| Họ giắc | **Degson 2EDG-5.08** (pitch 5,08 mm), **3 pin** |
| Socket trên PCB | **2EDGVC-5.08-03P** (hàn mặt trên, miệng cắm vào tủ) |
| Phích trên cáp | **2EDGK-5.08-03P** (+ nắp **2EDGY-5.08** / đuôi đặt ốc tùy bộ) |
| Cáp | **Belden 9841** hoặc tương đương: **1 pair xoắn + shield**, 24 AWG (0,5 mm²) |
| Ghép màu (gợi ý) | Pin1 Xanh → **A**; Pin2 Trắng → **B**; Pin3 Drain bọc → **SG** |
| Đầu PLC | A/B (D+/D−) + SG/0V theo manual; **shield nối 1 điểm** tại tủ/PLC |

**Thứ tự chân socket 2EDGVC-5.08-03P (nhìn từ mặt cắm, pin dưới lên):** pin **1 = A**, **2 = B**, **3 = SG** — ghi rõ silk **B.SilkS** để thợ không đảo.

**Không** gom 24 V chung với pair A/B trong **một phích 5P** — dễ nhầm khi bảo trì.

## 3.1 Mã linh kiện phích (thay thế tương đương)

| Vai trò | Degson (phổ biến VN) | Phoenix (chuẩn EU) |
|---------|----------------------|---------------------|
| Socket 2P PCB | 2EDGVC-5.08-02P | MC 1,5/ 2-G-5.08 |
| Socket 3P PCB **J485** | **2EDGVC-5.08-03P** (chuẩn dự án) | Thay thế chỉ khi cùng pitch 5,08 + 3P |
| Plug 2P + đuôi dây | 2EDGK-5.08-02P | — |
| Plug 3P **RS485** | **2EDGK-5.08-03P** | Cùng họ **2EDG-5.08** với socket |

## 4. DB9 / “cổng COM” — khác gì Modbus trên PLC?

**Cổng COM trên PC** (DB9 cái) gần như luôn là **RS-232**, không phải **RS-485**:

| | RS-232 (COM / DB9 PC) | RS-485 (Modbus PLC / panel bạn) |
|--|------------------------|----------------------------------|
| Điện | 1 máy ↔ 1 thiết bị, ± điện áp lớn | Bus **A/B** differential, nhiều slave |
| Giắc | **DB9** (DE-9) quen thuộc | **Terminal vít** hoặc phích 5,08 mm |
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

**Panel dự án này:** giữ **J_PWR + J485 5,08 mm**; nếu cần debug laptop, dùng **USB–RS485** đấu dây vào **cùng A/B** tại tủ (hoặc hộp phụ), không thay J485 bằng DB9 trên face panel.

## 5. So sánh giắc panel (tóm tắt)

| Giải pháp | Đánh giá |
|-----------|----------|
| **2 giắc 5,08 mm** (đang dùng) | Trùng thói quen tủ điện; cáp tách nguồn/signal; PLC đấu cos — **phù hợp nhất** |
| 1 giắc 4P (24V,GND,A,B) | Gọn nhưng dễ cắm nhầm, EMC kém hơn |
| DB9 trên mặt panel Modbus | Chỉ khi có **chuẩn cáp nội bộ**; dễ nhầm với COM RS-232 |

## 6. Kiểm tra sau lắp

1. J_PWR: đo **+24V** pin 1 so với pin 2 trước khi cắm J485.  
2. J485 idle: đo **A − B ≈ +200 mV ~ +5 V** (bias failsafe, tùy PLC).  
3. Modbus: **9600 8N1** (hoặc đúng cấu hình PLC), đảo A/B nếu không online.

## 7. Tọa độ PCB

Xem `docs/PCB_LAYOUT.md` — **J_PWR** (115, 16) và **J485** (115, 102) mm, cạnh phải bo.
