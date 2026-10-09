# Định dạng khối cấu hình runtime ("file chạy")

Khối nhị phân PC app biên dịch từ dự án rồi nạp xuống bàn phím. Nguồn chuẩn: `firmware/App/Inc/rtcfg.h` (có `_Static_assert` kiểm tra kích thước). Mọi số nhiều byte là **little-endian**, cấu trúc **không đệm** (packed).

- Kích thước cố định **1918 byte** (`RT_BLOB_SIZE`). Flash dành cho mỗi bản 1984 byte.
- Phiên bản cấu trúc `schema = 1`. Mỗi lần thêm / đổi trường phải **tăng schema**; firmware từ chối khối khác schema (`RT_E_SCHEMA`).
- Địa chỉ Modbus trong khối là **địa chỉ thô, bắt đầu từ 0**. Việc quy đổi `D100` → địa chỉ Modbus theo hãng PLC do PC app làm khi biên dịch.
- CRC-32 chuẩn IEEE 802.3 (đa thức 0xEDB88320, giá trị đầu và cuối 0xFFFFFFFF; vector thử: `"123456789"` → `0xCBF43926`) tính trên **1914 byte đầu**, lưu ở 4 byte cuối.

## Bố cục

| Offset | Kích thước | Trường |
|-------:|-----------:|--------|
| 0 | 4 | `magic` = `0x54524B50` (byte `50 4B 52 54` = "PKRT") |
| 4 | 2 | `schema` = 1 |
| 6 | 2 | `length` = 1918 |
| 8 | 2 | `revision` — PC app tăng mỗi lần nạp (đọc lại ở thanh ghi 117) |
| 10 | 2 | `flags` (dự trữ, 0) |
| 12 | 22 | `comm` |
| 34 | 8 | `sec` |
| 42 | 1600 | `page[32]` (mỗi trang 50 byte) |
| 1642 | 80 | `fkey[10]` (8 byte) |
| 1722 | 64 | `led[8]` (8 byte) |
| 1786 | 128 | `alarm[16]` (8 byte) |
| 1914 | 4 | `crc32` |

### `comm` (22 byte)

| Offset | Trường | Giá trị |
|-------:|--------|---------|
| 0 | `mode` | 0 master, 1 slave |
| 1 | `plc_profile` | chỉ để PC app ghi nhớ hồ sơ PLC |
| 2 | `plc_id` | master: trạm PLC 1..247 |
| 3 | `kbd_id` | slave: địa chỉ bàn phím 1..247 |
| 4 | `baud_idx` | 0:2400 1:4800 2:9600 3:19200 4:38400 5:57600 6:115200 |
| 5 | `parity` | 0 không, 1 chẵn, 2 lẻ |
| 6 | `stop` | 1 hoặc 2 |
| 7 | `order` | thứ tự word mặc định 1 ABCD, 2 CDAB, 3 BADC, 4 DCBA |
| 8 | `timeout_ms` (u16) | 20..5000 |
| 10 | `retries` | 0..5 |
| 11 | `poll_ms` (u16) | nghỉ giữa hai chu kỳ hỏi, 0..5000 |
| 13 | `flags` | bit0 đọc lại sau mọi lệnh ghi, bit1 dùng 8 phím mở rộng K3 |
| 14 | `brightness` | 0..7 |
| 15 | `beep` | bit0 tiếng phím, bit1 lỗi, bit2 cảnh báo |
| 16 | `idle_s` (u16) | về trang RUN + đăng xuất sau N giây không bấm (0 = không) |
| 18 | `run_page` | chỉ số trang RUN |
| 19 | `page_count` | 0..32 |
| 20 | `rsvd[2]` | 0 |

### `sec` (8 byte)

`pwd_user` (u16), `pwd_tech` (u16): 0 = không mật khẩu, 1..9999; `flags`; `rsvd[3]`.

### `page` (50 byte) = `top` (24) + `bot` (24) + `access` (1) + `rsvd` (1)

`access` = quyền cần để vào trang (0 tự do, 1 mật khẩu vận hành, 2 mật khẩu kỹ thuật; chỉ có tác dụng nếu mật khẩu tương ứng khác 0).

### Đối tượng `rt_obj_t` (24 byte)

| Offset | Trường | Ý nghĩa |
|-------:|--------|---------|
| 0 | `type` | 0 không dùng, 1 hiện số, 2 nhập số, 3 nhãn, 4 hiện trạng thái bit |
| 1 | `area` | 0 thanh ghi giữ (FC03/06/16), 1 thanh ghi đầu vào (FC04, chỉ đọc), 2 cuộn dây (FC01/05), 3 đầu vào rời (FC02, chỉ đọc) |
| 2 | `addr` (u16) | địa chỉ Modbus thô |
| 4 | `dtype` | 0 u16, 1 i16, 2 u32, 3 i32, 4 BCD16, 5 BCD32, 6 float32 |
| 5 | `order` | 0 theo mặc định chung, 1..4 như `comm.order` |
| 6 | `decimals` | số chữ số thập phân hiển thị 0..3 |
| 7 | `access` | quyền để sửa: 0 / 1 / 2 |
| 8 | `bit` | 0..15: bit trong thanh ghi; 0xFF: cả thanh ghi / cuộn dây |
| 9 | `flags` | bit0 ghép 8 số (dùng đối tượng trên, bỏ đối tượng dưới), bit1 đọc lại xác nhận, bit2 hiện số 0 đứng đầu |
| 10 | `label[4]` | nhãn 7 đoạn (ASCII; bit7 = bật chấm). Loại 4: nhãn khi bit = 1 |
| 14 | `min` (i32) | giới hạn nhập, theo **đơn vị hiển thị**. Loại 4: nhãn khi bit = 0 (4 ký tự ASCII) |
| 18 | `max` (i32) | giới hạn nhập |
| 22 | `gain` (i16) | hệ số ×1000 giữa giá trị PLC và giá trị hiển thị (0 hoặc 1000 = 1,000) |

Quy tắc kiểm tra (firmware từ chối nếu vi phạm — `RT_E_PAGE`): `INPUT` chỉ được ở vùng 0 hoặc 2 và `min ≤ max`; `decimals ≤ 3`; `bit ≤ 15` hoặc 0xFF; `dtype ≤ 6`. Trang chỉ được kiểm tra với chỉ số < `page_count`.

**Số hiển thị:** số nguyên có dấu; vị trí dấu chấm do `decimals` quyết định. Với số nguyên PLC, giá trị hiển thị = giá trị PLC × `gain`/1000 (1250 với `decimals`=1 hiện 125.0). Với float32, giá trị hiển thị = float × 10^`decimals` × `gain`/1000.

### Phím chức năng `rt_fkey_t` (8 byte): F1..F10 theo thứ tự trong mảng

| Offset | Trường | |
|-------:|--------|--|
| 0 | `action` | 0 không, 1 giữ (nhấn = 1, nhả = 0), 2 đảo, 3 Set, 4 Reset, 5 ghi hằng số, 6 sang trang `value`, 7 trang kế, 8 trang trước |
| 1 | `area` | 0 cuộn dây, 1 bit trong thanh ghi giữ, 2 cả thanh ghi giữ |
| 2 | `addr` (u16) | |
| 4 | `bit` | với `area` = 1 |
| 5 | `value` (u16) | hằng số ghi (action 5) hoặc số trang (action 6) |
| 7 | `access` | |

F1, F2 là hai phím trên bo; F3..F10 là 8 phím K3 (chỉ khi `comm.flags` bit1). Với `area` = 1 firmware **đọc – sửa – ghi** cả thanh ghi.

### LED `rt_led_t` (8 byte): LED 1..8 = D10..D17

`area` (1), `addr` (u16), `bit`, `mode` (0 tắt, 1 sáng khi bit = 1, 2 nháy khi bit = 1, 3 COMM: nháy theo giao tiếp, tắt khi mất liên lạc), `flags` (bit0 đảo mức), `rsvd[2]`.

### Cảnh báo `rt_alarm_t` (8 byte)

`area`, `addr` (u16), `bit` (0xFF = khác 0 là báo), `flags` (bit0 còi, bit1 LED nháy thay vì sáng, bit2 cần xác nhận bằng ESC), `led` (0 không, 1..8), `ack_addr` (u16, cuộn dây ghi 1 khi xác nhận).

Mục cảnh báo "không dùng" = `area`0, `addr` 0, `bit` 0xFF, `flags` 0.

## Mặc định xuất xưởng (khi flash trống / hỏng)

Chế độ **slave**, địa chỉ 1, 9600 8N1, 1 trang nhãn "PLC", LED1 = COMM, không mật khẩu. Thiết bị chưa cấu hình **không bao giờ ghi vào PLC**.

## Lưu trong flash

Hai trang 2 KB tại `0x08007000` (A) và `0x08007800` (B). Mỗi bản = tiêu đề 16 byte (`magic 'PKSL'`, `seq` u32, `len` u16, dự trữ, `crc32` của khối) + khối. Ghi sang trang còn lại, ghi dữ liệu trước, tiêu đề sau cùng; khi khởi động chọn bản hợp lệ có `seq` mới nhất. Mất điện giữa chừng luôn còn bản cũ.
