/*
 * Màn hình: 2 hàng x 4 số 7 đoạn + 8 LED trên TM1638 (RAM: byte 2g = đoạn a..g,dp của GRID(g+1);
 * byte 2g+1 bit0 = SEG9 = LED g+1). Hàng trên = digit 0..3, hàng dưới = digit 4..7.
 * Mọi hàm vẽ chỉ ghi vào bộ đệm; display_flush() mới đẩy xuống TM1638 khi nội dung đổi hoặc đến kỳ làm tươi.
 */
#ifndef DISPLAY_H
#define DISPLAY_H
#include <stdint.h>
#include <stdbool.h>

#define DISP_FLAG_ZEROS 0x01u   /* hiện số 0 đứng đầu */

void display_init(uint8_t brightness);
void display_set_brightness(uint8_t b);
void display_clear(void);
void display_row_clear(uint8_t row);

/* Chữ 4 ký tự (ASCII; bit7 = bật chấm của ký tự đó). Ký tự không vẽ được -> '-' */
void display_text(uint8_t row, const uint8_t text[4]);
void display_str(uint8_t row, const char *s);               /* s dài tối đa 4, ngắn hơn thì đệm trắng bên phải */
/* Số nguyên có dấu với `decimals` số lẻ, căn phải trong 4 số; tràn -> "----". width = 4 (một hàng) hoặc 8 (ghép) */
void display_number(uint8_t row, int32_t v, uint8_t decimals, uint8_t flags, uint8_t width);
void display_row_dashes(uint8_t row);
void display_raw(uint8_t digit, uint8_t segs);              /* digit 0..7 */

void display_leds(uint8_t on_mask, uint8_t blink_mask);
void display_row_blink(uint8_t row, bool blink);            /* nháy cả hàng */

uint8_t display_char_seg(uint8_t c);                        /* mã đoạn của ký tự (không gồm dp) */
bool    display_char_ok(uint8_t c);                         /* ký tự có vẽ được không */

void display_flush(uint32_t now);
void display_force(void);                                   /* buộc ghi lần tới */
uint8_t display_peek_seg(uint8_t digit);                    /* đọc lại (kiểm thử) */

#endif
