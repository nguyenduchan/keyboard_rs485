# Giao thức cấu hình và bản đồ thanh ghi (Modbus RTU)

Nguồn chuẩn: `firmware/App/Inc/cfgsrv.h`, `firmware/App/Src/cfgsrv.c`. Địa chỉ thanh ghi là **địa chỉ thô bắt đầu từ 0**; các lệnh hỗ trợ: FC01, 02, 03, 04, 05, 06, 15, 16. Thanh ghi FC03 và FC04 trả cùng dữ liệu.

## 1. Vào chế độ cấu hình

Cổng RS485 của PC (USB–RS485) nối vào giắc J485.

| Cách | Thông số | Trạm |
|------|----------|------|
| **Cửa sổ 3 giây đầu sau khi cấp nguồn** | 9600 8N1 | **247** |
| Menu trên máy: mục `PC` | 9600 8N1 | 247 |
| Lệnh `ENTER_CONFIG` (6) khi đang chạy slave | giữ nguyên thông số đang dùng | địa chỉ riêng của bàn phím |

Trong 3 giây đầu sau cấp nguồn, bàn phím chỉ **lắng nghe** (master chưa phát gì). PC gửi một yêu cầu hợp lệ tới trạm 247 (ví dụ đọc thanh ghi 100) → bàn phím trả lời, chuyển sang **chế độ cấu hình** và ở lại đó: màn hiện `PC`, master không khởi động. Nếu **không có** yêu cầu nào trong 3 giây, bàn phím chạy bình thường theo cấu hình.

Thoát: lệnh `EXIT` (khởi động lại sau ~80 ms). Không có yêu cầu nào trong 120 s thì tự khởi động lại.

Quy trình đề nghị cho PC app: cấp nguồn bàn phím → liên tục gửi "đọc thanh ghi 100" tới trạm 247 mỗi ~100 ms tới khi có phản hồi → đọc thông tin thiết bị → nạp khối → `APPLY` → `EXIT`.

## 2. Bản đồ thanh ghi

### 2.1 Thông tin thiết bị (chỉ đọc)

| Địa chỉ | Nội dung |
|--------:|----------|
| 100 | `0x504B` ("PK") |
| 101 | model (`0x0001` = PK-4x4-G030) |
| 102 | phiên bản firmware: (major << 8) \| minor |
| 103 | patch |
| 104 | phiên bản cấu trúc khối cấu hình (schema) |
| 105, 106 | số serial rút gọn từ UID (hi, lo) |
| 107 | khả năng: bit1 master, bit2 slave, bit4 đang ở chế độ cấu hình |
| 108 | số trang tối đa (32) |
| 109 | (số phím F tối đa << 8) \| số LED (10, 8) |
| 110 | kích thước khối cấu hình (1918) |
| 111 | dung lượng tối đa mỗi bản trong flash (1984) |
| 112 | trạng thái: bit0 cấu hình đã nạp từ flash, bit1 mất liên lạc PLC, bit2 có thay đổi chưa lưu, bit3 chế độ cấu hình, bit4 nguồn ổn |
| 113 | kết quả lệnh gần nhất |
| 114 | mã lỗi gần nhất |
| 115 | thời gian chạy (giây, 16 bit thấp) |
| 116 | số thứ tự bản đang lưu trong flash (16 bit thấp) |
| 117 | `revision` của cấu hình đang chạy |

### 2.2 Lệnh

| Địa chỉ | |
|--------:|--|
| 120 | **ghi** mã lệnh; đọc lại = lệnh gần nhất |
| 121 | kết quả lệnh (chỉ đọc) |

| Mã | Lệnh | Điều kiện | Tác dụng |
|---:|------|-----------|----------|
| 1 | `APPLY` | chế độ cấu hình | kiểm tra khối trong RAM **đúng như đã nhận** (magic, schema, độ dài, **CRC32**, phạm vi) rồi lưu flash A/B |
| 2 | `RELOAD` | chế độ cấu hình | nạp lại từ flash vào RAM (bỏ phần vừa ghi) |
| 3 | `FACTORY` | chế độ cấu hình | đặt mặc định xuất xưởng và lưu |
| 4 | `IDENTIFY` | luôn | bíp + nháy màn trong 3 s để nhận ra đúng máy |
| 5 | `EXIT` | luôn | khởi động lại |
| 6 | `ENTER_CONFIG` | luôn | vào chế độ cấu hình, giữ nguyên thông số nối tiếp |
| 7 | `CLEAR_DIAG` | luôn | xoá bộ đếm chẩn đoán |

Kết quả (121): `0` thành công; `0x20` không ở chế độ cấu hình; `0x21` lệnh không hợp lệ; `1..10` lỗi khối cấu hình (`RT_E_*`: 1 magic, 2 schema, 3 độ dài, 4 **CRC**, 5 thông số chung, 6 trang, 7 phím F, 8 LED, 9 cảnh báo, 10 mật khẩu); `0x10 + n` lỗi flash (`n`: 2 xoá, 3 ghi, 4 xác minh). Lệnh thực hiện ngay trong lúc xử lý yêu cầu ghi; `APPLY` mất cỡ 50 ms (xoá + ghi flash).

### 2.3 Cửa sổ khối cấu hình

Thanh ghi **200..1159** (960 thanh ghi = 1920 byte) ánh xạ khối cấu hình 1918 byte: thanh ghi `200 + i` = byte `2i` ở byte **cao**, byte `2i+1` ở byte **thấp** (big-endian chuẩn Modbus). Byte thứ 1919 (đệm) bỏ qua.

- Đọc: mọi lúc (sao lưu cấu hình đang chạy).
- Ghi: **chỉ trong chế độ cấu hình** — ngoài chế độ đó trả exception 04. Ghi tối đa 123 thanh ghi mỗi lệnh FC16 (khuyến nghị 60). Ghi thẳng vào RAM làm việc, **chưa lưu**: phải `APPLY`.
- Nạp đủ 959 thanh ghi (1918 byte) rồi `APPLY`; nếu không hợp lệ thì dùng `RELOAD` để huỷ.
- Một khối đầy đủ cần khoảng 16 lệnh FC16 (≈ 5 s ở 9600).

### 2.4 Chẩn đoán (chỉ đọc)

| Địa chỉ | |
|--------:|--|
| 130 | số giao dịch master thành công |
| 131 | số lần mất liên lạc (hết thời gian) |
| 132 | số lỗi CRC / khung |
| 133 | số exception nhận từ PLC |
| 134 | (loại lỗi gần nhất << 8) \| mã exception |
| 135 | số yêu cầu hợp lệ đã phục vụ (slave) |

### 2.5 Thanh ghi slave: phím, hiển thị, LED, còi

Dùng khi bàn phím chạy **slave** (PLC là master) và cũng dùng được trong chế độ cấu hình để thử phần cứng.

| Địa chỉ | Quyền | Nội dung |
|--------:|:-----:|----------|
| 0 | R | mã phím logic vừa nhấn (bảng dưới) |
| 1 | R | bộ đếm lần nhấn (+1 mỗi lần, tràn về 0) — PLC so sánh để biết có lần nhấn mới |
| 2, 3 | R | giá trị người dùng vừa nhập (int32: 2 = 16 bit cao, 3 = 16 bit thấp) |
| 4 | R/W | cờ: bit0 = **có giá trị mới** (PLC ghi 0 để xoá); bit 8..15 = chỉ số đối tượng |
| 5 | R | bitmap SW1..SW16 đang nhấn (bit n−1 = SWn) |
| 6 | R | bitmap SW17..SW24 (8 bit thấp) |
| 10, 11 | R/W | giá trị hàng trên (int32, hi, lo) |
| 12, 13 | R/W | giá trị hàng dưới (int32, hi, lo) |
| 14 | R/W | LED: bit 0..7 sáng, bit 8..15 nháy |
| 15 | R/W | điều khiển: bit0..2 độ sáng 0..7; **bit8** ghi 1 = bíp ngắn; **bit9** ghi 1 = bíp dài (tự xoá, đọc luôn 0); bit10 = khoá phím (vẫn báo phím, không nhập cục bộ) |
| 16 | R/W | định dạng: bit0..3 số lẻ hàng trên; bit4..7 số lẻ hàng dưới; bit8 ghép 8 số (dùng giá trị 10–11); bit9 hiện số 0 đầu; bit10 / bit11 nháy hàng trên / dưới; bit12 **hiển thị đoạn thô** |
| 20..27 | R/W | đoạn thô của digit 1..8 (8 bit thấp, bit0 = a ... bit6 = g, bit7 = chấm) khi bit12 của thanh ghi 16 bằng 1 |
| 7..9, 17..19, 28..29 | R | dự trữ, đọc ra 0 |

Cuộn dây: **0..7** LED sáng, **8..15** LED nháy (đọc/ghi FC01/05/15); **16** ghi 1 = bíp ngắn.

Ở chế độ slave, người dùng gõ số bằng phím: số hiện ở **hàng dưới** (nháy); `ENT` đưa giá trị vào thanh ghi 2–3 và bật cờ; `ESC` xoá từng số (giữ ≥ 1 s xoá hết); giữ phím `0` ≥ 0,8 s đổi dấu.

#### Mã phím logic (thanh ghi 0)

`1..9` = phím số 1..9; `10` = phím 0; `11` ESC; `12` ENT; `13` ▲; `14` ▼; `15` F1; `16` F2; `17..24` F3..F10 (phím mở rộng).

## 3. Exception

| Mã | Khi |
|---:|-----|
| 01 | lệnh không hỗ trợ |
| 02 | địa chỉ không tồn tại / chỉ đọc |
| 03 | số lượng / giá trị sai (FC03: 1..125; FC16: 1..123; FC05 chỉ nhận 0x0000 / 0xFF00) |
| 04 | ghi cửa sổ cấu hình khi không ở chế độ cấu hình |

FC16 kiểm tra **toàn bộ** địa chỉ trước khi ghi: lỗi thì không thanh ghi nào bị đổi. Trạm 0 (broadcast): chỉ thực hiện lệnh ghi, không trả lời.

## 4. Master: những gì bàn phím gửi xuống PLC

Khi chạy master, bàn phím chỉ phát: FC01/02/03/04 (đọc gộp các địa chỉ liền nhau, khoảng cách ≤ 2, tối đa 100 thanh ghi hoặc 400 bit), FC05 (cuộn dây), FC06 (một thanh ghi), FC16 (số 32 bit, 2 thanh ghi). Ghi bit trong thanh ghi dùng đọc – sửa – ghi (FC03 rồi FC06). **Chỉ ghi khi người dùng thao tác** (nhập số + ENT, phím F, xác nhận cảnh báo); mất liên lạc không tự ghi lại giá trị cũ.

Thời gian chờ và số lần thử lại theo cấu hình (`timeout_ms`, `retries`). Khi mất liên lạc hàng trên nhấp nháy `E-CO` xen kẽ nội dung, các giá trị đang hiện giữ nguyên; tự khôi phục khi PLC trả lời. Mã lỗi hiển thị: `E-CO` mất liên lạc, `E-Ad` PLC báo sai địa chỉ (exception 02), `E-Fn` lệnh không hỗ trợ, `E-dA` dữ liệu sai / đọc lại không khớp, `E-Hi` / `E-Lo` ngoài phạm vi nhập, `E-bU` bận, `E-cF` cấu hình hỏng (đã dùng mặc định), `E-Pd` sai mật khẩu.
