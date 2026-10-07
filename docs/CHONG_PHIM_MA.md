# Chống phím ma & nhiễu bàn phím công nghiệp

## Hiện tượng

- **Phím ma (false trigger):** MCU/TM1638 báo nhấn khi không có người bấm — thường do **nhiễu điện** (buck, relay tủ, ESD), không phải ghost vật lý ma trận.
- **Ghost ma trận:** nhấn 2 phím cùng hàng/cột có thể đọc nhầm phím thứ 3 — TM1638 quét ma trận; firmware nên **chỉ chấp nhận 1 phím** hoặc dùng ma trận đủ diode (bàn phím PC); panel 4×4 dùng **lọc + debounce**.

## Cách làm phổ biến (panel công nghiệp)

| Biện pháp | Trên bo keyboard |
|-----------|------------------|
| Tách vùng PCB | **24V protect** / **buck** / **MCU** + **khe Y 82–90 mm** — không đặt TM1638 dưới buck |
| Một mặt SMT | Toàn SMD **mặt sau**; phím + LED + 7-seg **THT mặt trước** — stencil một lần |
| Lọc ma trận tại IC quét | **RK1–4 100Ω** + **CK1–4 100nF** trên hàng; **RC1–4** + **CC1–4** trên cột (sát U2) |
| Lọc nguồn TM1638 | **CTM1 100nF**, **CTM2 10µF** trên **+5V_LED** |
| Bus TM1638 | **R33 33Ω** CLK/STB/DIO; **RPU 4.7k** DIO |
| Mặt tủ | Decal che phím; **khung kim loại nối GND** (1 điểm) nếu có |
| Firmware | Debounce **≥ 20 ms**, đọc 2 lần giống nhau, bỏ qua burst &lt; 3 ms |

## Layout

Chi tiết 3 vùng chữ nhật: `docs/PCB_LAYOUT.md`.

## Route

- Dây **KEY_R/KEY_C:** via gần **SW THT**, chạy trong **ZONE_MCU**, **không** song song **L1 / +5V_SW**.
- **GND** dưới ma trận: pour liên tục; via stitch quanh TM1638.
- **RS485** và **buck** ở mép dưới, cách khe MCU.

## Kiểm thử

1. Nguồn 24 V, không bấm phím — đọc TM1638 1 phút: **0 sự kiện**.
2. Contactor tủ đóng cắt gần bo — không reset, không phím ảo.
3. Bấm từng phím — không dính / nhảy cột.
