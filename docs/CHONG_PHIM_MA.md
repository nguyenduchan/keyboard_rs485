# Chống phím ma & nhiễu bàn phím công nghiệp

## Hiện tượng

- **Phím ma (false trigger):** MCU báo nhấn khi không có người bấm — thường do **nhiễu điện** (buck, relay tủ, ESD), không phải ghost vật lý ma trận.
- **Ghost ma trận:** nhấn 2 phím cùng hàng/cột có thể đọc nhầm phím thứ 3. Firmware **chỉ chấp nhận 1 phím**. Panel 4×4 dùng **lọc + debounce**.

## Cách làm phổ biến (panel công nghiệp)

| Biện pháp | Trên bo keyboard |
|-----------|------------------|
| Tách vùng PCB | Trở phím sát GPIO của STM32; **buck / 24V** nửa trên cột phải **X 82…118**; **MCU** nửa dưới |
| Một mặt SMT | Toàn chip và SMD **mặt trên**; phím + LED + 7-seg **mặt dưới** — stencil một lần |
| Lọc ma trận tại MCU | **RK/RC 100Ω** nối tiếp sát GPIO; **RPK 10k** kéo cột lên +3V3. Debounce firmware ≥ 20 ms |
| Nguồn quét LED | **CLED 100nF** trên VDD của **U6**, rail **+5V_LED** sau **FB5V** |
| Mặt tủ | Decal che phím; **khung kim loại nối GND** (1 điểm) nếu có |
| Firmware | Debounce **≥ 20 ms**, đọc 2 lần giống nhau, bỏ qua burst &lt; 3 ms |

## Layout

Chi tiết 3 vùng chữ nhật: `docs/PCB_LAYOUT.md`.

## Route

- Dây **KEY_R/KEY_C** (khi route): via gần **SW THT**, không song song **L1 / +5V_SW**.
- **GND** dưới ma trận: pour liên tục; via stitch quanh U1.
- **Buck** nằm khối nguồn phía trên, **RS485** nằm khối MCU phía dưới, không đè courtyard phím.

## Kiểm thử

1. Nguồn 24 V, không bấm phím — đọc ma trận 1 phút: **0 sự kiện**.
2. Contactor tủ đóng cắt gần bo — không reset, không phím ảo.
3. Bấm từng phím — không dính / nhảy cột.
