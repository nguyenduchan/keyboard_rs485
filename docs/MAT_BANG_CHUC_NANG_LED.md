# Bảng gán nhãn 8 LED trạng thái (in trên Decal)

Cột phải cụm phím **X = 79.5 mm**. Trên bản vẽ KiCad, **Y nhỏ = trên**. D1/D10 bắt đầu sát cạnh trên.

| LED | Tọa độ PCB (X,Y) mm | Màu gợi ý | Nhãn mặc định | Ý nghĩa |
|-----|---------------------|-----------|---------------|---------|
| D1 | 79.5, **12** | Xanh | RUN | Máy đang chạy |
| D2 | 79.5, 20 | Đỏ | STOP | Dừng / lệnh stop |
| D3 | 79.5, 28 | Vàng/Đỏ | FAULT | Lỗi |
| D4 | 79.5, 36 | Xanh | AUTO | Chế độ tự động |
| D5 | 79.5, 44 | Vàng | MAN | Chế độ tay |
| D6 | 79.5, 52 | Xanh | RDY | Sẵn sàng |
| D7 | 79.5, 60 | Vàng | COMM | RS485 hoạt động |
| D8 | 79.5, **68** | Đỏ | ALM | Cảnh báo |

Ref PCB **D10–D17**. `tools/front_layout_spec.py`
