/*
 * Phím: đọc TM1638, chống dội, chỉ nhận MỘT phím một lúc (FW-44), phát sự kiện nhấn / nhả.
 * Ma trận không có diode nên khi >= 2 phím cùng bị nhấn thì không xác định được phím nào thật:
 * phím mới chỉ được nhận khi tại thời điểm đó chỉ có đúng một phím; phím đang giữ vẫn giữ cho đến khi nhả.
 */
#ifndef KEYS_H
#define KEYS_H
#include <stdint.h>
#include <stdbool.h>

/* mã phím logic (thanh ghi 0 ở chế độ slave) */
enum {
    KC_NONE = 0,
    KC_1, KC_2, KC_3, KC_4, KC_5, KC_6, KC_7, KC_8, KC_9, KC_0,
    KC_ESC, KC_ENT, KC_UP, KC_DOWN,
    KC_F1, KC_F2, KC_F3, KC_F4, KC_F5, KC_F6, KC_F7, KC_F8, KC_F9, KC_F10
};
#define KC_COUNT 25u

enum { KEV_PRESS = 1, KEV_RELEASE = 2 };
typedef struct {
    uint8_t  type;      /* KEV_* */
    uint8_t  sw;        /* số công tắc 1..24 (SW1..SW24) */
    uint32_t t;
} key_event_t;

void     keys_init(bool ext_enabled);
void     keys_poll(uint32_t now);               /* gọi thường xuyên; tự giới hạn chu kỳ KEY_SCAN_PERIOD_MS */
bool     keys_pop(key_event_t *ev);
uint8_t  keys_active(void);                     /* SW đang giữ (0 = không) */
uint32_t keys_held_ms(uint32_t now);
uint32_t keys_raw(void);                        /* bitmap thô 24 bit lần đọc gần nhất (bit n-1 = SWn) */
uint16_t keys_count(void);                      /* số lần nhấn từ khi khởi động */

uint32_t keys_decode(const uint8_t k[4], bool ext);   /* 4 byte TM1638 -> bitmap */
uint8_t  keys_logical(uint8_t sw);                    /* SW -> mã phím logic theo bố cục mặc định */
int8_t   kc_digit(uint8_t kc);                        /* 0..9 hoặc -1 */

#endif
