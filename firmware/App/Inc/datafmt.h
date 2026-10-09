/*
 * Chuyển đổi giữa thanh ghi Modbus (kiểu dữ liệu, thứ tự word/byte, hệ số) và số nguyên hiển thị.
 * "Số hiển thị" là số nguyên có dấu; vị trí dấu chấm thập phân do trường `decimals` của đối tượng quyết định
 * (với số nguyên PLC: 1250 + decimals 1 hiển thị 125.0; với float: 125.0 x 10^decimals).
 */
#ifndef DATAFMT_H
#define DATAFMT_H
#include "rtcfg.h"

/* w[0], w[1] = thanh ghi theo thứ tự trên dây (địa chỉ n, n+1). Trả về false nếu dữ liệu không hợp lệ (BCD sai, NaN...). */
bool df_decode(const rt_obj_t *o, uint8_t def_order, const uint16_t w[2], int32_t *disp);
/* Số hiển thị -> thanh ghi (rt_nregs(o) thanh ghi). false nếu vượt phạm vi kiểu dữ liệu. Không dùng cho đối tượng bit. */
bool df_encode(const rt_obj_t *o, uint8_t def_order, int32_t disp, uint16_t w[2]);

/* ghép / tách 32 bit theo thứ tự (RT_ORD_ABCD..DCBA) */
uint32_t df_join32(uint8_t order, uint16_t a, uint16_t b);
void     df_split32(uint8_t order, uint32_t v, uint16_t *a, uint16_t *b);

#endif
