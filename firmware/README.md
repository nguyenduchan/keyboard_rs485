# Firmware bàn phím — Modbus RTU slave (STM32Cube)

STM32G030F6P6 + TM1638 + SIT3485. Project theo cấu trúc **STM32CubeMX / CMake** (HAL + LL của STM32CubeG0),
mở được bằng **STM32CubeIDE 2.x** hoặc VS Code (STM32Cube extension). Bàn phím chạy như một thiết bị
vào/ra Modbus RTU: PLC đọc phím, ghi 7 đoạn và LED.

## Bản đồ thanh ghi (Holding Register, FC03 / FC06 / FC16)

| PLC | Offset | Quyền | Nội dung |
|-----|--------|-------|----------|
| 40001 | 0x0000 | R | Trạng thái 16 phím tức thời. Bit n = phím S(n+1) (SW1…SW16), 1 = nhấn |
| 40002 | 0x0001 | R | Bộ đếm sự kiện: +1 mỗi phím mới nhấn (sau debounce), tràn 65535 → 0 |
| 40003 | 0x0002 | R/W | Hàng 7 đoạn 1 (DS1, hàng trên), 0…9999 |
| 40004 | 0x0003 | R/W | Hàng 7 đoạn 2 (DS2, hàng dưới), 0…9999 |
| 40005 | 0x0004 | R/W | 8 LED đơn: bit 0…7 = LED 1…8 (D10…D17, từ trên xuống). Byte cao bị bỏ qua (đọc lại = 0) |

| Trường hợp | Exception |
|------------|-----------|
| Function code khác 03/06/16 | 01 Illegal Function |
| Ghi vào 40001/40002, hoặc địa chỉ ngoài 40001…40005 | 02 Illegal Data Address |
| Ghi 7 đoạn > 9999, số lượng thanh ghi = 0 / quá lớn, byte count sai | 03 Illegal Data Value |

FC16 kiểm tra **toàn bộ** giá trị trước khi ghi: một giá trị sai thì không thanh ghi nào bị đổi.
Địa chỉ 0 (broadcast) được nhận cho lệnh ghi, không trả lời.

Mặc định: **slave ID 1, 9600 baud, 8N1** — `App/Inc/config.h`.

## Hoạt động

- **Phím:** đọc TM1638 mỗi 5 ms, cần 4 mẫu giống nhau (20 ms) mới nhận. Mỗi phím mới nhấn: bật bit, tăng bộ đếm, còi kêu 40 ms.
  Tổ hợp phím gây "phím ma" (3 góc hình chữ nhật trong ma trận không diode) bị bỏ qua, giữ trạng thái cũ.
- **7 đoạn:** tách số bằng `/` và `%`, tắt số 0 ở đầu (`12` → `  12`).
- **Hiển thị:** ghi TM1638 ngay khi PLC đổi giá trị và ghi lại mỗi 250 ms để tự phục hồi sau nhiễu.
- **Khởi động:** sáng hết 0,6 s (lamp test), sau đó hiển thị 0, LED tắt.
- **Modbus RTU:** USART2 + DE phần cứng (HAL_RS485Ex_Init), nhận/phát từng byte bằng LL trong ngắt,
  chốt khung bằng khoảng lặng t3.5 đo bằng TIM14 (USART2 của G030 không có receiver timeout).
- **IWDG** ~500 ms (bật bằng LL sau lamp test).
- Clock: HSI 16 MHz, không PLL.

## Cấu trúc

| Đường dẫn | Nội dung |
|-----------|----------|
| `keyboard.ioc` | Cấu hình STM32CubeMX (chân, USART2 RS485, TIM14, clock) |
| `Core/` | Code kiểu CubeMX: `main.c` (MX_xxx_Init), `stm32g0xx_it.c`, `stm32g0xx_hal_msp.c` |
| `App/Inc/config.h` | Slave ID, baud, parity, debounce, còi, độ sáng |
| `App/Src/app.c` | `app_init()` / `app_loop()` gọi từ `main.c`, IWDG |
| `App/Src/regs.c` | Bảng thanh ghi + kiểm tra quyền ghi |
| `App/Src/modbus_slave.c` | FC03/06/16, CRC, exception (thuần C, test được trên PC) |
| `App/Src/modbus_rtu.c` | Ngắt USART2 + TIM14 (LL) |
| `App/Src/tm1638.c` | Bit-bang STB/CLK/DIO open-drain |
| `App/Src/panel_map.c` | Phím TM1638 → bit, số → mã 7 đoạn, LED → RAM TM1638 |
| `App/Src/panel.c` | Debounce, bộ đếm sự kiện, còi, làm tươi hiển thị |
| `Drivers/` | STM32G0xx HAL/LL driver + CMSIS (từ STMicroelectronics GitHub) |
| `CMakeLists.txt`, `CMakePresets.json`, `cmake/` | Build CMake + Ninja theo mẫu CubeMX |
| `tests/test_modbus.c` | Unit test chạy trên PC |

Code ứng dụng chỉ nằm trong `App/` và các khối `/* USER CODE BEGIN … END */` của `Core/`,
nên sinh lại code từ `keyboard.ioc` bằng STM32CubeMX (toolchain **CMake**) không làm mất code.
`keyboard.ioc` viết tay theo đúng cấu hình trong `main.c`; lần đầu mở bằng CubeMX nên kiểm tra lại tab Pinout
(PA1 = USART2_DE, PA4–PA6 open-drain, PA7 output) trước khi bấm Generate.

## Build

**Dòng lệnh** (dùng GCC + CMake + Ninja đi kèm STM32CubeIDE, không cần cài thêm):

```bat
build.cmd                 :: Release (-Os)  -> build\Release\keyboard.elf / .hex / .bin
build.cmd Debug           :: Debug (-O0 -g3)
build.cmd Release -DMODBUS_SLAVE_ID=3 -DMODBUS_BAUDRATE=19200 -DMODBUS_PARITY=EVEN
```

CubeIDE ở chỗ khác thì đặt `set CUBEIDE=D:\ST\STM32CubeIDE_x.y.z\STM32CubeIDE` trước khi chạy.
Tham số `-D…` được CMake nhớ trong `build\<cấu hình>`; muốn quay về mặc định thì xoá thư mục đó.

Kích thước hiện tại: Release ~7,7 KB flash / 1,8 KB RAM, Debug ~15 KB flash (chip có 32 KB / 8 KB).

**STM32CubeIDE 2.x:** File → Import → STM32 CMake project (hoặc *Existing CMake project*) → chọn thư mục `firmware`,
chọn preset Debug/Release, Build. Debug bằng ST-LINK qua header SWD J2.

**Nạp:** STM32CubeProgrammer → mở `build/Release/keyboard.hex` → Download, hoặc:

```bat
STM32_Programmer_CLI -c port=SWD mode=UR -w build\Release\keyboard.hex -v -rst
```

**Unit test trên PC** (gcc host bất kỳ):

```bash
gcc -std=c11 -Wall -Wextra -IApp/Inc tests/test_modbus.c App/Src/modbus_slave.c App/Src/regs.c App/Src/panel_map.c -o test_modbus && ./test_modbus
```

## Cần kiểm tra trên bo thật

- **Bit K1/K2 của TM1638:** firmware giả định mỗi nhóm 4 bit dữ liệu phím là `bit0 = K3, bit1 = K2, bit2 = K1`
  (giống board LED&KEY phổ biến). Nếu bấm SW1 mà PLC thấy sai bit, đổi `TM1638_K1_BIT` / `TM1638_K2_BIT`
  trong `App/Src/panel_map.c`.
- Độ sáng `DISPLAY_BRIGHTNESS` (0…7) theo màu decal.

## Ví dụ khung (slave 1)

```text
Đọc phím + bộ đếm:   01 03 00 00 00 02 C4 0B
Hàng 1 = 1234:        01 06 00 02 04 D2 AA 97
LED 1 và LED 3 sáng:  01 06 00 04 00 05 08 08
```
