# Hồ sơ PLC (`profiles/*.json`)

Mỗi hồ sơ là một file JSON, nạp khi khởi động từ `profiles/` cạnh exe rồi `%APPDATA%\PanelKbdConfig\profiles` (cùng `id` thì thư mục sau ghi đè).

```json
{
  "id": "delta_dvp", "name": "…", "vendor": "…",
  "verified": false,                 // đã thử bằng PLC thật chưa
  "source": "tên sổ tay, trang",
  "notes": "…",
  "defaultWordOrder": "CDAB",        // ABCD | CDAB
  "needsUserOffset": false,          // true: người dùng nhập địa chỉ PLC ứng với thanh ghi Modbus 0 (Siemens HoldStart)
  "offsetLabel": "",
  "isGeneric": false,                // true: bật chọn kiểu đánh số 0 / 1 / 40001
  "areas": [
    { "name": "D", "kind": "Word", "table": "Holding", "radix": 10,
      "min": 0, "max": 4095, "modbusStart": 4096, "stride": 1 }
  ]
}
```

- `kind`: `Word` | `Bit`. `table`: `Coil` | `Discrete` | `Holding` | `Input`.
- Địa chỉ Modbus = `modbusStart + (n − min) / stride` (với `needsUserOffset`: `min` được thay bằng offset người dùng).
- Cùng `name` có thể xuất hiện nhiều lần cho các dải khác nhau (ví dụ D0–D4095 và D4096–D9999 của Delta).
- `radix: 8` cho X/Y bát phân. Cú pháp địa chỉ: `D100`, `D100.3` (bit trong thanh ghi), `M10`.

## Trạng thái các hồ sơ

| Hồ sơ | Kiểm tra | Ghi chú |
|-------|----------|---------|
| Modbus chung | ✔ (theo chuẩn) | HR/IR/C/DI |
| Delta DVP | ✖ | S, X, Y, M, T, C, D theo bảng địa chỉ trong sổ tay DVP — đối chiếu dòng PLC |
| Mitsubishi FX3U/FX3UC | ✖ | M, S, TS, CS, Y, X (bát phân), D, R, TN, CN0..199 — bảng mặc định JY997D26201G mục 9.3 trang 95 |
| Mitsubishi FX3G/FX3GC | ✖ | như trên, theo trang 94 (T/C/Y/X/R khác dải) |
| Siemens S7-200 SMART | ✖ | VW theo HoldStart của MBUS_SERVER |

**Không đưa hồ sơ lên máy thật trước khi đối chiếu tài liệu chính hãng và thử bằng PLC thật**, rồi mới đổi `verified` thành `true`.
