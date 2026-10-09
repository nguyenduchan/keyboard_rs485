# Bảng gán nhãn 8 LED trạng thái (in trên Decal)

Cột LED **X = 80 mm** (KiCad) = **X 40 nhìn từ mặt tủ**, tức là bên trái cụm phím khi đứng trước tủ; nhãn in bên trái LED. **Y nhỏ = trên**. Decal sinh tự động: `hardware/mechanical/cnc/<biến thể>_decal.svg`. D1/D10 bắt đầu sát cạnh trên.

| LED | Tọa độ PCB (X,Y) mm | Màu gợi ý | Nhãn mặc định | Ý nghĩa |
|-----|---------------------|-----------|---------------|---------|
| D1 | 80, **12** | Xanh | RUN | Máy đang chạy |
| D2 | 80, 20 | Đỏ | STOP | Dừng / lệnh stop |
| D3 | 80, 28 | Vàng/Đỏ | FAULT | Lỗi |
| D4 | 80, 36 | Xanh | AUTO | Chế độ tự động |
| D5 | 80, 44 | Vàng | MAN | Chế độ tay |
| D6 | 80, 52 | Xanh | RDY | Sẵn sàng |
| D7 | 80, 60 | Vàng | COMM | RS485 hoạt động |
| D8 | 80, **68** | Đỏ | ALM | Cảnh báo |

Ref PCB **D10–D17**. `tools/front_layout_spec.py`
