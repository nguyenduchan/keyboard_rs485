# Panel Keyboard Config — PC app (C# / WPF)

Phần mềm cài đặt cho bàn phím RS485 theo `docs/YEU_CAU_FIRMWARE_PCAPP.md`. Quy trình giống màn HMI:
**chọn hồ sơ PLC → gán địa chỉ kiểu PLC (`D100`) → kiểm tra/biên dịch → nạp xuống bàn phím**.

> **Phạm vi:** app chỉ **bắt tay với bàn phím qua RS485 để đọc thông tin và gửi cấu hình** (file chạy `.pkb`). App **không nạp firmware**; nạp firmware làm riêng (ST-Link/SWD). Mục "nạp" trong tài liệu/mã nguồn nghĩa là gửi cấu hình, không phải flash.

## Chạy

```
dotnet run --project src/PanelKbd.App
dotnet test tests/PanelKbd.Tests
```

Cần .NET 8 SDK, Windows. Tham số: `PanelKbdConfig.exe [file.pkproj] [--tab N]`.

## Đã làm (mức P1)

| Mã | Nội dung |
|----|----------|
| PC-01 | Tạo/mở/lưu dự án `.pkproj` (JSON, có số phiên bản; file mới hơn phần mềm bị từ chối) |
| PC-10…13 | Hồ sơ PLC (Modbus chung, Delta DVP, Mitsubishi FX3U, Siemens S7-200 SMART), số trạm/baud/parity/stop/timeout/thử lại/chu kỳ hỏi, thứ tự word, kiểu đánh địa chỉ 0 / 1 / 40001, ô nhập địa chỉ bắt đầu cho Siemens |
| PC-20…22 | Hồ sơ là file JSON (`profiles/`), có cờ *đã kiểm tra* + nguồn tài liệu; bảng đối chiếu địa chỉ PLC ↔ Modbus (dec/hex/4xxxx), xuất CSV |
| PC-23 | Thêm/ghi đè hồ sơ không cần cài lại: thả JSON vào `profiles/` cạnh exe hoặc `%APPDATA%\PanelKbdConfig\profiles` |
| PC-31…33 | Hình bàn phím: bấm hàng số / phím ▲▼F1F2K3 / LED để sửa; xem trước 7 đoạn; trang (thêm, nhân bản, xoá, đổi thứ tự); màn RUN; mật khẩu |
| PC-40…42 | Kiểm tra: địa chỉ sai/ngoài vùng, ghi vào vùng chỉ đọc, 32 bit chồng thanh ghi, min>max, không đủ ô hiện, ký tự 7 đoạn không hiện được, trang đích không tồn tại…; bấm đúp lỗi để nhảy tới đối tượng; đồng hồ dung lượng; tạo file chạy `.pkb` + CRC32 |
| PC-50, 51 | **Tập lệnh cấu hình qua Modbus** (`ConfigClient`): `Handshake` (hỏi thanh ghi 100 mỗi 100 ms trong cửa sổ 3 s sau cấp nguồn), `ReadInfo` (100–117), `ReadDiagnostics` (130–135), `ReadConfig` (sao lưu khối 1918 B kiểm CRC32), `Download` (ghi → đọc lại so sánh), lệnh 120: `APPLY` / `RELOAD` / `FACTORY` / `IDENTIFY` / `EXIT` / `ENTER_CONFIG` / `CLEAR_DIAG`, dịch mã kết quả 121 và exception 04, thử phím–màn–LED–còi qua thanh ghi slave 0–27. Không nạp firmware |
| PC-50, 51 | Chọn cổng COM, tự tìm bàn phím, nhận diện, đọc thông tin, gửi cấu hình (ghi → đọc lại so sánh → ÁP DỤNG), thanh tiến trình, nhật ký khung hex TX/RX |
| PC-60…62 | **Tab Mô phỏng:** bàn phím ảo chạy đúng thiết kế (gõ số + ENT ghi, ESC, mật khẩu, min/max, phím F, LED, chuyển trang) trên bộ nhớ PLC ảo sửa được; bấm chuột hoặc gõ phím máy tính — không cần PLC hay bàn phím thật |
| PC-61, FW-65 | **Tab Thử phần cứng:** (A) PC làm master đọc phím bấm thật, ghi hàng số/LED theo bản đồ thanh ghi firmware hiện tại, kèm chạy thử tự động; (B) **PLC ảo trên cổng COM** (Modbus slave FC01–06/15/16) để thử bàn phím ở chế độ master mà không có PLC. Có "Firmware ảo" để thử giao diện khi chưa có bàn phím |
| — | **Bố cục phần cứng** (tab Thiết kế): ẩn/hiện hàng 7 đoạn, từng LED, từng phím cho board không lắp đủ; kiểm tra/biên dịch bỏ qua phần không lắp |
| — | **Thiết bị ảo** trong bộ nhớ chạy đúng giao thức cấu hình để thử khi chưa có phần cứng/firmware |

Chưa làm (P2/P3): bảng tag, undo/redo, tiếng Anh, cảnh báo, đọc ngược, nạp hàng loạt, bootloader, PDF, hồ sơ Xinje/Inovance.

## Cấu trúc

| Đường dẫn | Nội dung |
|-----------|----------|
| `src/PanelKbd.Core` | Mô hình dự án, hồ sơ + quy đổi địa chỉ, kiểm tra, biên dịch file chạy, giao thức cấu hình, thiết bị ảo (không phụ thuộc giao diện) |
| `src/PanelKbd.App` | Giao diện WPF (MVVM nhẹ) |
| `tests/PanelKbd.Tests` | xUnit: quy đổi địa chỉ, kiểm tra, biên dịch, nạp xuống thiết bị ảo |
| `profiles/*.json` | Hồ sơ PLC |
| `docs/` | `PLC_PROFILES.md` (giao thức và định dạng file chạy nằm ở `../docs/CONFIG_PROTOCOL.md`, `../docs/RUNTIME_FORMAT.md` — nguồn chuẩn là `firmware/App/Inc/cfgsrv.h`) |
| `tools/shots.ps1` | Chụp ảnh từng tab để kiểm tra giao diện |

## Lưu ý quan trọng

- Hồ sơ Delta/Mitsubishi/Siemens viết từ hiểu biết chung, **chưa kiểm tra bằng PLC thật** (`"verified": false`) — app cảnh báo khi dùng. Đối chiếu sổ tay chính hãng trước khi đưa lên máy.
- Giao thức cấu hình và định dạng file chạy theo `../docs/CONFIG_PROTOCOL.md` và `../docs/RUNTIME_FORMAT.md` (khớp `firmware/App/Inc/cfgsrv.h`); đổi gì thì tăng số phiên bản (A-05).
- Mật khẩu lưu dạng rõ trong dự án và file chạy: chỉ chống thao tác nhầm.
