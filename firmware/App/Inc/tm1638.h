/*
 * Driver TM1638 (3 dây STB/CLK/DIO, bit-bang open-drain).
 *
 * RAM hiển thị 16 byte: byte 2k   = SEG1..SEG8 của GRID(k+1)  (bit0 = SEG1 = nét a)
 *                       byte 2k+1 = SEG9 (bit0), SEG10 (bit1) của GRID(k+1)
 * Dữ liệu phím 4 byte: byte b chứa KS(2b+1) ở bit 0..2 và KS(2b+2) ở bit 4..6,
 *                      trong mỗi nhóm: bit0 = K3, bit1 = K2, bit2 = K1.
 */
#ifndef TM1638_H
#define TM1638_H

#include <stdint.h>

#define TM1638_RAM_SIZE 16u

void    tm1638_init(void);
void    tm1638_write_ram(const uint8_t ram[TM1638_RAM_SIZE], uint8_t brightness);
uint8_t tm1638_read_keys(uint8_t keys[4]);  /* 1 = OK, 0 = dữ liệu không hợp lệ */

#endif
