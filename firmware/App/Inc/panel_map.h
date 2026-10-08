/*
 * Ánh xạ thuần (không đụng phần cứng) giữa dữ liệu TM1638 và thanh ghi Modbus.
 *
 * Phím (docs/SO_DO_NGUYEN_LY.md §5): SWn, n = 1..16
 *   KS = ((n-1) % 8) + 1,  K = 1 nếu n <= 8, K = 2 nếu n >= 9
 *   -> bit (n-1) của thanh ghi 40001.
 *
 * Hiển thị: DS1 (hàng trên) = GRID1..4, DS2 (hàng dưới) = GRID5..8, DIG1 bên trái.
 * LED đơn: bit i của 40005 -> D(10+i), anode SEG9, cathode GRID(i+1).
 */
#ifndef PANEL_MAP_H
#define PANEL_MAP_H

#include <stdint.h>

#define KEY_GHOST 0xFFFFFFFFu

/* Trả về bitmap 16 phím, hoặc KEY_GHOST nếu tổ hợp phím gây "phím ma" (không xác định được) */
uint32_t panel_decode_keys(const uint8_t tm_keys[4]);

/* Dựng 16 byte RAM TM1638 từ 2 giá trị hiển thị và byte LED */
void panel_build_ram(uint16_t row1, uint16_t row2, uint8_t leds, uint8_t ram[16]);

/* Số bit 1 */
uint8_t panel_popcount16(uint16_t v);

#endif
