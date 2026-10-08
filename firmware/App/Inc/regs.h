/*
 * Bản đồ Holding Register (4xxxx) của bàn phím.
 *
 *  PLC     Offset  Quyền  Nội dung
 *  40001   0x0000  R      Trạng thái 16 phím tức thời, bit n = phím S(n+1), 1 = nhấn
 *  40002   0x0001  R      Bộ đếm sự kiện nhấn phím (tăng 1 mỗi phím mới nhấn, tràn về 0)
 *  40003   0x0002  R/W    Hàng 7 đoạn 1 (DS1, hàng trên): 0..9999
 *  40004   0x0003  R/W    Hàng 7 đoạn 2 (DS2, hàng dưới): 0..9999
 *  40005   0x0004  R/W    8 LED đơn, bit 0..7 = LED 1..8 (D10..D17), byte cao bỏ qua
 */
#ifndef REGS_H
#define REGS_H

#include <stdint.h>

#define REG_KEY_STATE     0x0000u
#define REG_KEY_COUNTER   0x0001u
#define REG_DISPLAY1      0x0002u
#define REG_DISPLAY2      0x0003u
#define REG_LEDS          0x0004u
#define REG_COUNT         5u

#define DISPLAY_MAX_VALUE 9999u

/* Mã exception Modbus */
#define MB_EX_NONE                 0u
#define MB_EX_ILLEGAL_FUNCTION     1u
#define MB_EX_ILLEGAL_DATA_ADDRESS 2u
#define MB_EX_ILLEGAL_DATA_VALUE   3u

void     regs_init(void);

/* Phía Modbus */
uint8_t  regs_read(uint16_t addr, uint16_t *value);
uint8_t  regs_check_write(uint16_t addr, uint16_t value);  /* kiểm tra, chưa ghi */
void     regs_write(uint16_t addr, uint16_t value);        /* chỉ gọi sau khi check OK */

/* Phía ứng dụng */
uint16_t regs_get(uint16_t addr);
void     regs_set_key_state(uint16_t state);
void     regs_add_key_events(uint16_t count);
uint8_t  regs_take_output_changed(void);  /* 1 nếu PLC vừa đổi 7 đoạn / LED, đồng thời xoá cờ */

#endif
