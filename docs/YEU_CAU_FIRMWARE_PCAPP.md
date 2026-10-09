# Yêu cầu firmware và phần mềm cài đặt (PC app) — Panel Keyboard RS485

Mục tiêu: bàn phím + màn 8 số + 8 LED làm **thiết bị nhập liệu cho PLC qua Modbus RTU**, cài đặt theo cách các hãng màn HMI làm (OP320/MD204L, Weintek EasyBuilder, Delta DOPSoft, Kinco DTools…): chọn hãng PLC → gán địa chỉ theo kiểu PLC (`D100`) → biên dịch → nạp xuống → mô phỏng / giám sát.

> Phần so sánh với sản phẩm thương mại dựa trên hiểu biết chung, chưa đối chiếu tài liệu từng hãng. Bảng quy đổi địa chỉ theo hãng phải kiểm tra bằng tài liệu chính hãng và PLC thật (xem `PLC_PROFILES.md` khi có).

Mức ưu tiên: **P1** = bản đầu (MVP), **P2** = bản 2, **P3** = khi có khách cần.


> **Tình trạng (firmware rev C, 2026-10-09):** phần firmware P1 và phần lớn P2 đã làm (`firmware/`, 1244 kiểm tra trên PC). Chưa làm: bootloader RS485 (FW-80/81), cập nhật `keyboard.ioc`; chống sụt áp chỉ dựa vào POR/PDR vì G030 không có PVD/BOR. Giao thức và định dạng chi tiết: `CONFIG_PROTOCOL.md`, `RUNTIME_FORMAT.md`. Chưa thử trên bo thật.

## 0. Nguyên tắc kiến trúc

| ID | Nguyên tắc |
|----|-----------|
| A-01 | **Một phần cứng, một firmware** cho mọi hãng PLC. Firmware chỉ biết **địa chỉ Modbus thô**. |
| A-02 | **Quy đổi địa chỉ theo hãng nằm trong PC app** (bước biên dịch), dưới dạng **hồ sơ dữ liệu** (JSON), không phải code. Thêm hãng = cập nhật PC app, không nạp lại firmware. |
| A-03 | PC app nạp xuống một **file chạy (runtime config)** nhị phân có **số phiên bản cấu trúc + CRC32**. File dự án (tên tag, ghi chú) ở trên PC. |
| A-04 | Mặc định **Modbus master** (bàn phím ghi thẳng vào PLC, thợ không viết code giao tiếp); có **chế độ slave** cho PLC đã làm master bus hoặc PLC không làm slave được. |
| A-05 | Mọi thay đổi cấu trúc file chạy / giao thức cấu hình phải **tăng số phiên bản**; PC app đọc phiên bản để tương thích ngược. |

## 1. Yêu cầu firmware

### 1.1 Nền tảng

| ID | Yêu cầu | Ưu tiên |
|----|---------|---------|
| FW-01 | STM32G030F6P6, vòng lặp chính tick 1 ms, không RTOS. Module: `tm1638`, `ui`, `params`, `mb_master`, `mb_slave`, `rs485`, `storage`, `buzzer`, `leds`, `bootloader` | P1 |
| FW-02 | Watchdog phần cứng (IWDG), brown-out reset; khởi động lại an toàn, không ghi gì xuống PLC khi khởi động | P1 |
| FW-03 | Màn khởi động 1 s: phiên bản firmware, chế độ (`NSt`/`SLv`), địa chỉ trạm | P1 |
| FW-04 | Khởi tạo lại TM1638 định kỳ (chống treo sau ESD) | P1 |

### 1.2 Truyền thông Modbus RTU

| ID | Yêu cầu | Ưu tiên |
|----|---------|---------|
| FW-10 | Tốc độ 2400–115200; parity None/Even/Odd; stop 1/2; khung 8 bit dữ liệu | P1 |
| FW-11 | Khoảng nghỉ 3,5 ký tự giữa khung (timer), điều khiển chân DE, thời gian quay bus cấu hình được | P1 |
| FW-12 | **Master:** lệnh 01, 02, 03, 04, 05, 06, 15, 16; số trạm PLC 1–247; timeout và số lần thử lại cấu hình được (mặc định 200 ms, 2 lần) | P1 |
| FW-13 | **Bộ lập lịch hỏi vòng:** chỉ đọc đối tượng **đang hiện trên màn** + bit LED + bit cảnh báo; **gộp địa chỉ liền nhau** thành một lệnh (≤ 125 thanh ghi / ≤ 2000 bit); chu kỳ hỏi cấu hình được | P1 |
| FW-14 | **Hàng đợi ghi ưu tiên:** lệnh ghi chen trước vòng hỏi; tuỳ chọn **đọc lại xác nhận** sau khi ghi | P1 |
| FW-15 | Xử lý mã lỗi Modbus (exception) và timeout: hiện `E-CO` / `E-Ad` (sai địa chỉ) / `E-Fn`; **giữ giá trị cũ** trên màn; tự phục hồi khi PLC trả lời lại | P1 |
| FW-16 | **Slave:** lệnh 03, 04, 06, 16 (+ 01, 05 cho bit); địa chỉ trạm 1–247; bảng thanh ghi mục 1.8 | P1 |
| FW-17 | Bộ đếm chẩn đoán: số khung tốt, timeout, lỗi CRC, exception, lần lỗi gần nhất | P2 |

### 1.3 Kiểu dữ liệu

| ID | Yêu cầu | Ưu tiên |
|----|---------|---------|
| FW-20 | Bit: coil, input rời, **bit trong thanh ghi** (`D100.3`) | P1 |
| FW-21 | int16, uint16, int32, uint32; **thứ tự word chọn được** (ABCD / CDAB, thêm BADC / DCBA ở P2) | P1 |
| FW-22 | Số chữ số thập phân cố định (0–3) theo từng đối tượng | P1 |
| FW-23 | BCD 16/32 bit, float32, thang đo tuyến tính (gain/offset) | P2 |

### 1.4 Đối tượng chạy (giống các đối tượng của HMI)

| ID | Đối tượng | Mô tả | Ưu tiên |
|----|----------|-------|---------|
| FW-30 | **Trang** | Tối đa 32 trang; mỗi trang = đối tượng hàng trên + hàng dưới (4 số mỗi hàng, hoặc ghép 8 số cho giá trị lớn) | P1 |
| FW-31 | **Hiện số** | Đọc từ PLC, có thập phân, dấu âm | P1 |
| FW-32 | **Nhập số** | Có min/max, thập phân, cấp mật khẩu, ghi khi bấm ENT | P1 |
| FW-33 | **Nhãn tĩnh** | Chữ 7 đoạn tối đa 4 ký tự (`LEn`, `Cnt`, `SPd`) | P1 |
| FW-34 | **Hiện trạng thái bit** | Hiện `On`/`OFF` hoặc 2 nhãn tuỳ chọn | P2 |
| FW-35 | **Phím chức năng** (F1, F2, phím mở rộng K3) | Giữ = 1 / đảo / Set / Reset / ghi hằng số / chuyển trang / trang trước–sau | P1 |
| FW-36 | **LED** | Theo bit PLC; sáng / nhấp nháy; đảo mức; có thể gán LED `COMM` nội bộ | P1 |
| FW-37 | **Cảnh báo** | Tối đa 16 bit → hiện mã `A-01`…, nháy LED, kêu còi; phím xác nhận (ack); bit xác nhận ghi về PLC | P2 |
| FW-38 | **Màn RUN** | Trang chính (ví dụ hàng trên = giá trị thực, hàng dưới = giá trị đặt); không bấm phím quá N giây thì quay về RUN | P1 |

### 1.5 Thao tác nhập (theo thói quen bàn phím biến tần / HMI)

| ID | Yêu cầu | Ưu tiên |
|----|---------|---------|
| FW-40 | Bố cục phím mặc định: `7 8 9 ESC / 4 5 6 ▲ / 1 2 3 ▼ / F1 0 F2 ENT` (phím ▲▼F1F2 có thể đổi chức năng) | P1 |
| FW-41 | Gõ số dịch từ phải sang trái; ESC = xoá 1 số, giữ ESC = huỷ; giữ 0 = đổi dấu | P1 |
| FW-42 | ENT: kiểm tra min/max → ghi → xác nhận (nháy + bíp ngắn); ngoài giới hạn → `E-Hi` / `E-Lo` + bíp dài | P1 |
| FW-43 | ▲▼ chuyển trang, giữ để lặp có tăng tốc; nhập số ngay từ màn RUN vào ô "giá trị đặt" | P1 |
| FW-44 | Chống dội 20–30 ms, chỉ nhận **một phím một lúc**, khoá phím | P1 |
| FW-45 | Còi: click khi bấm (bật/tắt), bíp lỗi, bíp cảnh báo theo bit PLC | P1 |

### 1.6 Bảo mật

| ID | Yêu cầu | Ưu tiên |
|----|---------|---------|
| FW-50 | Mật khẩu 2 cấp (vận hành / kỹ thuật) gán cho trang hoặc ô nhập; tự đăng xuất khi hết thời gian chờ | P1 |
| FW-51 | Khoá menu cài đặt bằng mật khẩu kỹ thuật; khoá đọc ngược cấu hình từ PC (mật khẩu) | P2 |

### 1.7 Giao diện cấu hình cho PC app

| ID | Yêu cầu | Ưu tiên |
|----|---------|---------|
| FW-60 | **Chế độ cấu hình:** vào bằng menu `PC` hoặc khi nhận yêu cầu trong 3 s đầu sau cấp nguồn; khi đó là slave **thông số cố định** (9600 8N1, trạm 247); màn hiện `PC` | P1 |
| FW-61 | Vùng **thông tin thiết bị** (chỉ đọc): model, phiên bản firmware, phiên bản cấu trúc, serial, khả năng (số trang tối đa, có K3…) | P1 |
| FW-62 | Đọc/ghi **khối cấu hình** theo đoạn (≤ 60 thanh ghi/lệnh) vào vùng đệm | P1 |
| FW-63 | Thanh ghi lệnh: `ÁP DỤNG` (kiểm CRC32 rồi mới lưu), `NẠP LẠI`, `MẶC ĐỊNH NHÀ MÁY`, `NHẬN DIỆN` (bíp + nháy), `THOÁT` | P1 |
| FW-64 | Lưu flash **2 bản A/B** + CRC; mất điện khi đang lưu vẫn còn bản cũ; hỏng cả hai → mặc định + `E-cF` | P1 |
| FW-65 | Chế độ thử từ PC: đọc phím, bật LED/còi, ghi số lên màn | P2 |

### 1.8 Bảng thanh ghi chế độ slave (PLC làm master)

| Địa chỉ | Nội dung | Ưu tiên |
|---------|----------|---------|
| 0 | Mã phím vừa nhấn | P1 |
| 1 | Bộ đếm số lần nhấn (PLC so sánh để biết lần nhấn mới) | P1 |
| 2–3 | Giá trị vừa nhập (32 bit) | P1 |
| 4 | Cờ "có giá trị mới" (PLC ghi 0 để xoá) + số thứ tự ô vừa sửa | P1 |
| 10–13 | Giá trị hiện hàng trên / hàng dưới, số thập phân | P1 |
| 14 | 8 bit LED + 8 bit nhấp nháy | P1 |
| 15 | Lệnh còi, độ sáng 0–7 | P1 |
| 20–27 | Chế độ thô: ghi trực tiếp từng đoạn của 8 số | P2 |
| 100+ | Vùng cấu hình (giống FW-61…63) | P1 |

### 1.9 Menu cài đặt trên máy

| ID | Yêu cầu | Ưu tiên |
|----|---------|---------|
| FW-70 | Vào bằng giữ ESC + ENT khi cấp nguồn + mật khẩu | P1 |
| FW-71 | Mục: chế độ, số trạm PLC/bàn phím, tốc độ, parity, timeout, độ sáng, còi, đổi mật khẩu, mặc định nhà máy, xem phiên bản, `PC` (vào chế độ cấu hình) | P1 |
| FW-72 | Sửa nhanh địa chỉ/min/max của từng ô (khi không có PC) — địa chỉ Modbus thô | P2 |
| FW-73 | Chế độ kiểm tra phần cứng: sáng hết đoạn/LED, hiện mã phím, bíp | P1 |

### 1.10 Cập nhật firmware

| ID | Yêu cầu | Ưu tiên |
|----|---------|---------|
| FW-80 | **Bootloader riêng** (4–6 KB) cập nhật qua RS485 (bootloader trong ROM của STM32 không điều khiển được chân DE) | P2 |
| FW-81 | Kiểm CRC ảnh firmware, cờ "ứng dụng hợp lệ", không bao giờ ghi đè bootloader; lỗi giữa chừng thì ở lại bootloader | P2 |

### 1.11 Hiệu năng và an toàn

| ID | Yêu cầu | Ưu tiên |
|----|---------|---------|
| FW-90 | Từ lúc bấm ENT đến khi ghi xong vào PLC < 100 ms ở 9600 baud (không lỗi) | P1 |
| FW-91 | Cập nhật màn ≥ 5 lần/giây với giá trị đang hiện | P1 |
| FW-92 | **Chỉ ghi xuống PLC khi người dùng thao tác** (hoặc lệnh có cấu hình rõ); mất liên lạc thì không tự ghi lại giá trị cũ | P1 |
| FW-93 | Tài liệu ghi rõ: **không dùng phím F làm dừng khẩn cấp**; mạch an toàn phải đi dây cứng | P1 |

## 2. Yêu cầu PC app

### 2.1 Dự án

| ID | Yêu cầu | Ưu tiên |
|----|---------|---------|
| PC-01 | Tạo/mở/lưu dự án (JSON có phiên bản); chọn model bàn phím và phiên bản firmware đích | P1 |
| PC-02 | File mẫu theo loại máy; ghi chú phiên bản dự án | P2 |
| PC-03 | Giao diện tiếng Việt + tiếng Anh; undo/redo; sao chép/dán đối tượng | P2 |

### 2.2 Kết nối PLC (giống bước chọn driver của HMI)

| ID | Yêu cầu | Ưu tiên |
|----|---------|---------|
| PC-10 | Chọn **hồ sơ PLC** theo hãng/dòng; luôn có "Modbus chung" (địa chỉ thô) | P1 |
| PC-11 | Số trạm, tốc độ, parity, stop, timeout, thử lại, chu kỳ hỏi | P1 |
| PC-12 | Thứ tự word mặc định theo hồ sơ, cho phép ghi đè; chọn cách đánh địa chỉ (bắt đầu từ 0 / kiểu 40001) | P1 |
| PC-13 | Hồ sơ cần **địa chỉ bắt đầu do người dùng đặt** (Siemens S7-200 SMART/S7-1200, LS) → ô nhập offset | P1 |

### 2.3 Hồ sơ quy đổi địa chỉ

| ID | Yêu cầu | Ưu tiên |
|----|---------|---------|
| PC-20 | Hồ sơ là **file dữ liệu** (JSON): vùng nhớ (D, M, X, Y, V…), kiểu bit/word, địa chỉ Modbus bắt đầu, giới hạn, cơ số (thập phân/bát phân cho X/Y), thứ tự word mặc định | P1 |
| PC-21 | Mỗi hồ sơ có **trạng thái "đã kiểm tra bằng PLC thật"** và **nguồn tài liệu** (tên sổ tay, trang) | P1 |
| PC-22 | Luôn hiện **địa chỉ PLC và địa chỉ Modbus song song** để thợ đối chiếu | P1 |
| PC-23 | Cập nhật hồ sơ không cần cài lại phần mềm (thư mục hồ sơ) | P2 |

### 2.4 Bảng tag và thiết kế

| ID | Yêu cầu | Ưu tiên |
|----|---------|---------|
| PC-30 | Bảng tag: tên, địa chỉ kiểu PLC, kiểu dữ liệu, ghi chú; nhập/xuất CSV; tra chỗ dùng | P2 |
| PC-31 | **Hình bàn phím WYSIWYG**: bấm vào hàng số / phím / LED để sửa thuộc tính | P1 |
| PC-32 | Trình soạn trang: thêm/xoá/sắp xếp trang; chọn đối tượng hàng trên/dưới; xem trước **chữ 7 đoạn** và cảnh báo ký tự không hiện được | P1 |
| PC-33 | Trình soạn phím, LED, màn RUN, mật khẩu | P1 |
| PC-34 | Trình soạn cảnh báo | P2 |

### 2.5 Biên dịch và kiểm tra

| ID | Yêu cầu | Ưu tiên |
|----|---------|---------|
| PC-40 | Kiểm tra: trùng địa chỉ, số 32 bit chồng lên thanh ghi khác, min > max, địa chỉ ngoài vùng của hồ sơ, vượt số trang/đối tượng, nhãn sai | P1 |
| PC-41 | Danh sách lỗi/cảnh báo, bấm vào để nhảy tới đối tượng | P1 |
| PC-42 | Quy đổi địa chỉ → tạo **file chạy** (nhị phân + CRC32 + phiên bản); đồng hồ dung lượng còn trống | P1 |

### 2.6 Nạp xuống / đọc ngược

| ID | Yêu cầu | Ưu tiên |
|----|---------|---------|
| PC-50 | Chọn cổng COM (CH340/FTDI), tự tìm bàn phím ở chế độ cấu hình, nút **Nhận diện** | P1 |
| PC-51 | Nạp xuống có thanh tiến trình, đọc lại so sánh, lệnh ÁP DỤNG | P1 |
| PC-52 | Đọc ngược file chạy → dựng lại dự án (mất tên tag nếu không lưu kèm); bảo vệ bằng mật khẩu | P2 |
| PC-53 | **Nạp hàng loạt** (chế độ sản xuất): cắm từng máy, một nút, ghi nhật ký serial | P2 |
| PC-54 | Cập nhật firmware qua bootloader | P2 |

### 2.7 Mô phỏng và giám sát (giống HMI)

| ID | Yêu cầu | Ưu tiên |
|----|---------|---------|
| PC-60 | **Mô phỏng ngoại tuyến:** bàn phím ảo bấm được + bảng thanh ghi PLC ảo | P2 |
| PC-61 | **Mô phỏng trực tuyến:** PC đóng vai bàn phím, nói chuyện với PLC thật qua USB–RS485 | P2 |
| PC-62 | Theo dõi thanh ghi trực tiếp, ghi thử, thống kê truyền thông; **nhật ký khung hex** để gỡ lỗi | P2 |

### 2.8 Tài liệu xuất ra

| ID | Yêu cầu | Ưu tiên |
|----|---------|---------|
| PC-70 | Xuất PDF **phiếu cấu hình**: danh sách trang/ô ↔ thanh ghi PLC, phím, LED | P2 |
| PC-71 | Trang hướng dẫn theo hồ sơ PLC: cách bật Modbus slave, đấu dây A/B/GND, ví dụ | P2 |

### 2.9 Đóng gói

| ID | Yêu cầu | Ưu tiên |
|----|---------|---------|
| PC-80 | Một bộ cài Windows, không cần quyền admin; không phụ thuộc phần mềm của hãng PLC | P1 |
| PC-81 | Mật khẩu bảo vệ file dự án (tuỳ chọn) | P3 |

## 3. Tài liệu dùng chung

1. `CONFIG_PROTOCOL.md` — bảng thanh ghi chế độ cấu hình, lệnh, mã lỗi (FW-60…65). **Đã viết.**
2. `RUNTIME_FORMAT.md` — cấu trúc nhị phân file chạy, giới hạn số trang/đối tượng, phiên bản. **Đã viết.**
3. `PLC_PROFILES.md` + thư mục `profiles/*.json` — hồ sơ quy đổi địa chỉ, có nguồn tài liệu và trạng thái kiểm tra. **Chưa viết.**

## 4. Lộ trình gợi ý

| Bản | Firmware | PC app |
|-----|----------|--------|
| **v1 (P1)** | Master + slave, trang/nhập số/phím F/LED, menu cài đặt, chế độ cấu hình, lưu A/B | Dự án, hồ sơ Modbus chung + Delta DVP, hình bàn phím, biên dịch, nạp xuống |
| **v2 (P2)** | Cảnh báo, BCD/float, chẩn đoán, bootloader | Bảng tag, hồ sơ Mitsubishi / Siemens / Xinje, mô phỏng, nạp hàng loạt, PDF |
| **v3 (P3)** | Driver giao thức riêng của hãng (nếu có khách cần, có thể phải đổi phần cứng sang RS422/RS232) | Bảo vệ dự án, thêm hãng |
