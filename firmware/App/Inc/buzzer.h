/* Còi chủ động (PA7): bíp không chặn, theo cờ trong cấu hình (click / lỗi / cảnh báo). */
#ifndef BUZZER_H
#define BUZZER_H
#include <stdint.h>
#include <stdbool.h>

void buzzer_init(uint8_t beep_flags);        /* RT_BF_* */
void buzzer_click(void);                     /* nhấn phím */
void buzzer_ok(void);                        /* ghi thành công */
void buzzer_error(void);                     /* lỗi */
void buzzer_alarm(bool on);                  /* tiếng cảnh báo lặp (đến khi tắt) */
void buzzer_force(uint16_t ms);              /* bíp bắt buộc (PC / kiểm tra), bỏ qua cờ */
void buzzer_identify(uint16_t ms);           /* nhịp nhận diện thiết bị */
void buzzer_poll(uint32_t now);
bool buzzer_is_on(void);

#endif
