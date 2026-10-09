/*
 * Giao diện phần cứng mà phần logic của firmware sử dụng.
 * Trên chip: hw_*.c, tm1638.c (STM32 HAL/LL). Trên PC: tests/fake_hw.c (mô phỏng để chạy unit test).
 */
#ifndef HWIF_H
#define HWIF_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

uint32_t hw_millis(void);
void     hw_buzzer(bool on);
void     hw_wdt_start(void);            /* bật IWDG (không tắt được) */
void     hw_wdt_kick(void);
void     hw_reset(void);                /* khởi động lại MCU (trên PC: đặt cờ) */
uint32_t hw_uid32(void);                /* số serial 32 bit rút gọn từ UID */
bool     hw_power_ok(void);             /* false khi VDD thấp hơn ngưỡng PVD */

/* ---- TM1638 ---- */
#define TM_RAM_SIZE 16u
void     hw_tm_init(void);
void     hw_tm_write(const uint8_t ram[TM_RAM_SIZE], uint8_t brightness);
/* keys[4] = 4 byte dữ liệu phím; false nếu đường dữ liệu không hợp lệ (TM1638 không trả lời) */
bool     hw_tm_read_keys(uint8_t keys[4]);

/* ---- flash (khu vực cấu hình) ---- */
bool     hw_flash_erase_page(uint32_t addr);
bool     hw_flash_write(uint32_t addr, const void *data, uint32_t len);  /* len bội số của 8 */
const uint8_t *hw_flash_ptr(uint32_t addr);

#endif
