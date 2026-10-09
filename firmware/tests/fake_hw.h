/* Phần cứng giả để chạy firmware trên PC: thời gian, TM1638 + ma trận phím, flash cấu hình, RS485, PLC giả. */
#ifndef FAKE_HW_H
#define FAKE_HW_H
#include <stdint.h>
#include <stdbool.h>
#include "hwif.h"

/* ---- thời gian & hệ thống ---- */
extern uint32_t fake_now;
extern bool     fake_tick_on_read;              /* hw_millis() tự tăng 1 mỗi lần gọi */
extern int      fake_reset_count;
extern int      fake_wdt_kicks;
extern bool     fake_wdt_started;
extern bool     fake_power_ok;
void fake_hw_reset_all(void);                   /* khôi phục trạng thái ban đầu (flash trống) */
void fake_hw_keep_flash_reset(void);            /* như cấp nguồn lại: giữ nội dung flash */

/* ---- còi ---- */
extern bool     fake_buzzer;
extern int      fake_buzzer_on_count;
extern uint32_t fake_buzzer_on_ms;              /* tổng thời gian bật */

/* ---- TM1638 ---- */
extern uint8_t  fake_tm_ram[16];
extern uint8_t  fake_tm_bright;
extern int      fake_tm_writes;
void fake_key_press(uint8_t sw);                /* 1..24 */
void fake_key_release(uint8_t sw);
void fake_key_release_all(void);
void fake_tm_dead(bool dead);                   /* TM1638 không trả lời (đọc ra 0xFF) */
uint8_t fake_seg(uint8_t digit);                /* đoạn đang hiển thị của digit 0..7 (đọc lại từ RAM TM1638) */
bool    fake_led(uint8_t led);                  /* LED 0..7 */
void    fake_row_text(uint8_t row, char out[9]);/* quy ngược 4 số về chữ (chỉ để so sánh trong test; dp = '.' phía sau) */

/* ---- flash ---- */
extern int fake_flash_erases, fake_flash_writes;
void fake_flash_power_cut_after(int nwrites);   /* sau N lệnh ghi thì mọi thao tác thất bại (-1 = tắt) */
void fake_flash_corrupt(uint32_t addr, uint8_t xor_mask);

/* ---- RS485 giả + PLC ---- */
void fake_bus_reset(void);
void fake_rs485_inject(const uint8_t *frame, uint16_t len, bool bad);   /* khung đến từ ngoài (test slave) */
extern uint8_t  fake_tx_log[16][256];
extern uint16_t fake_tx_len[16];
extern int      fake_tx_count;
extern uint32_t fake_baud;
extern uint8_t  fake_parity, fake_stop;
extern int      fake_rs485_inits;

/* PLC giả (trạm fake_plc_id): bật bằng fake_plc_enable(true). Bộ nhớ: 2048 thanh ghi + 2048 cuộn dây. */
extern uint16_t fake_plc_hold[2048];
extern uint8_t  fake_plc_coil[2048];
extern uint8_t  fake_plc_id;
extern bool     fake_plc_online;
extern int      fake_plc_requests;
extern int      fake_plc_bad_addr_from;         /* >= giá trị này: exception 02 (mặc định -1: tắt) */
extern int      fake_plc_drop_next;             /* bỏ qua N yêu cầu kế tiếp (không trả lời) */
extern int      fake_plc_corrupt_next;          /* làm hỏng CRC N phản hồi kế tiếp */
extern uint8_t  fake_plc_last_req[256];
extern uint16_t fake_plc_last_len;
void fake_plc_enable(bool on);
void fake_plc_clear(void);

#endif
