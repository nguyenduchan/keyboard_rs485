# Firmware bàn phím Modbus (STM32G030F6P6 + TM1638 + SP3485)

Viết lại theo `docs/YEU_CAU_FIRMWARE_PCAPP.md`. Làm việc ở hai chế độ chọn bằng cấu hình:

- **Master** (mặc định khi dùng làm thiết bị nhập liệu cho PLC): bàn phím tự đọc / ghi thanh ghi PLC theo cấu hình (trang, ô nhập số, phím F, LED, cảnh báo).
- **Slave**: PLC làm master, đọc phím và ghi số / LED qua bản đồ thanh ghi (`docs/CONFIG_PROTOCOL.md` mục 2.5).

Cấu hình (khối 1918 byte, `docs/RUNTIME_FORMAT.md`) do PC app nạp xuống qua RS485 hoặc sửa nhanh bằng menu trên máy. Thiết bị xuất xưởng là **slave địa chỉ 1, 9600 8N1** để chưa cấu hình thì không bao giờ ghi vào PLC.

## Cấu trúc

| Đường dẫn | Nội dung |
|-----------|----------|
| `Core/`, `Drivers/`, `startup_*.s`, `keyboard.ioc` | nền STM32Cube (HAL + LL). USART2 / TIM14 khởi tạo bằng LL trong `main.c` (tiết kiệm ~1,8 KB flash so với HAL) |
| `App/Inc/config.h` | hằng số biên dịch (giới hạn, thời gian, bản đồ flash) |
| `rtcfg.[ch]`, `cfg.[ch]`, `storage.[ch]`, `crc.[ch]` | khối cấu hình, kiểm tra, lưu flash A/B có CRC |
| `datafmt.[ch]` | thanh ghi ↔ số hiển thị (u16/i16/u32/i32/BCD/float32, thứ tự word, hệ số); float32 chỉ dùng số nguyên |
| `mb_slave.[ch]`, `mb_master.[ch]`, `rs485.[ch]` | Modbus RTU slave / master (thuần logic) và lớp truyền USART2 + DE phần cứng + khoảng lặng t3.5 |
| `engine.[ch]` | chạy master: hỏi vòng gộp địa chỉ, bộ nhớ đệm, ghi / đọc-sửa-ghi, phím F, LED, cảnh báo |
| `cfgsrv.[ch]` | bản đồ thanh ghi slave + giao thức cấu hình cho PC app |
| `keys.[ch]`, `display.[ch]`, `buzzer.[ch]`, `tm1638.c` | phím (chống dội, một phím một lúc), 2×4 số 7 đoạn + 8 LED, còi, driver TM1638 |
| `ui.[ch]`, `menu.[ch]` | màn hình khởi động, trang, nhập số, mật khẩu, slave, chế độ PC, menu cài đặt, kiểm tra phần cứng |
| `app.[ch]`, `hw_misc.c`, `hwif.h` | vòng chính; phần cứng còn lại và giao diện để mô phỏng trên PC |
| `tests/` | unit test + mô phỏng toàn hệ thống trên PC (phần cứng giả, PLC giả) |

## Bản đồ bộ nhớ

| Vùng | Địa chỉ | |
|------|---------|--|
| Ứng dụng | `0x08000000` – `0x08006FFF` (28 KB) | Release hiện **25,7 KB** (còn ~3 KB) |
| Cấu hình bản A / B | `0x08007000` / `0x08007800` | 2 KB mỗi trang |
| RAM | 8 KB | Release dùng 4,5 KB (khối cấu hình 1,9 KB + đệm RS485 0,5 KB + bộ nhớ đệm hỏi vòng) |

## Build

```bat
build.cmd              :: Release (-Os) -> build\Release\keyboard.elf / .hex / .bin
build.cmd Debug
```

`build.cmd` dùng GCC + CMake + Ninja đi kèm STM32CubeIDE (`set CUBEIDE=...` nếu cài chỗ khác). Nạp:

```bat
STM32_Programmer_CLI -c port=SWD mode=UR -w build\Release\keyboard.hex -v -rst
```

## Test trên PC

```bat
tests\run_tests.cmd            :: hoặc: sh tests/run_tests.sh [basic|modbus|display|engine|ui]
```

Cần `gcc` (MinGW). **1244 kiểm tra** gồm: CRC, kiểu dữ liệu và thứ tự word, kiểm tra khối cấu hình, lưu flash (cắt điện ở mọi bước), slave / master / giao thức cấu hình, phím (chống dội, phím ma), hiển thị, còi, engine với PLC giả (gộp địa chỉ, ghi, đọc-sửa-ghi, phím F, cảnh báo, mất liên lạc) và toàn hệ thống qua `app_loop()` (khởi động, nhập số, mật khẩu, trang, slave, chế độ PC, menu).

## Vận hành

- **Khởi động:** màn hiện phiên bản và chế độ, rồi trạm; 3 s đầu chỉ lắng nghe trạm cấu hình 247 (9600 8N1). Master bắt đầu hỏi PLC sau 3 s.
- **Menu cài đặt:** giữ **ESC + ENT** lúc cấp nguồn (hỏi mật khẩu kỹ thuật nếu có). ▲▼ chọn mục, ENT sửa, ESC thoát; thoát sẽ lưu và khởi động lại nếu có thay đổi.
- **Bố cục phím** (nhìn từ mặt trước): `7 8 9 ESC / 4 5 6 ▲ / 1 2 3 ▼ / F1 0 F2 ENT` = SW13..16 / SW9..12 / SW5..8 / SW1..4. Sửa trong `keys.c` nếu khác thực tế.
- **Nhập số:** gõ số (dịch từ phải sang trái), ENT ghi; ESC xoá từng số; giữ ESC ≥ 1 s huỷ; giữ `0` ≥ 0,8 s đổi dấu.
- **Watchdog** IWDG 500 ms bật sau khi khởi tạo.

## Chưa làm / cần kiểm tra trên bo thật

- **Bootloader qua RS485** (FW-80/81) chưa có; hiện nạp bằng SWD. Ứng dụng đã chiếm 25,7 KB nên nếu thêm bootloader 4–6 KB cần giảm thêm vài KB (LTO, bỏ HAL RCC).
- STM32G030 **không có PVD/BOR**: chống sụt áp chỉ nhờ POR/PDR (~1,7 V) + lưu cấu hình A/B có CRC. `hw_power_ok()` luôn trả `true`.
- Chưa thử trên phần cứng: đúng bit K1/K2/K3 và thứ tự KS của TM1638 (theo thư viện TM16xx: byte b chứa KS(2b+1) ở nửa thấp, KS(2b+2) ở nửa cao; bit2 = K1, bit1 = K2, bit0 = K3), thời gian bit-bang, chân DE phần cứng và khoảng lặng t3.5, ghi flash thật, hướng bố cục phím.
- `keyboard.ioc` chưa cập nhật cho USART2 / TIM14 dùng LL; mở bằng CubeMX rồi Generate sẽ ghi đè phần khởi tạo này.
- Mở rộng K3 (SW17..SW24) có sẵn trong firmware nhưng bo hiện chưa có hàng pad; bật bằng `comm.flags` bit1.
