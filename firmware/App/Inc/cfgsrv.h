/*
 * Bản đồ thanh ghi Modbus của bàn phím khi làm SLAVE (FW-16, 1.8) và giao diện cấu hình cho PC app (FW-60..65).
 * Địa chỉ là địa chỉ thô bắt đầu từ 0 (xem docs/CONFIG_PROTOCOL.md).
 *
 *   0 mã phím vừa nhấn | 1 bộ đếm lần nhấn | 2-3 giá trị vừa nhập (hi,lo) | 4 cờ | 5-6 bitmap phím
 *   10-11 giá trị hàng trên | 12-13 giá trị hàng dưới | 14 LED | 15 điều khiển | 16 định dạng | 20-27 đoạn thô
 *   100-121 thông tin + lệnh | 130-137 chẩn đoán | 200-1159 cửa sổ khối cấu hình (1920 byte)
 */
#ifndef CFGSRV_H
#define CFGSRV_H
#include <stdint.h>
#include <stdbool.h>
#include "mb_slave.h"

/* thanh ghi */
#define REG_KEY_CODE     0
#define REG_KEY_COUNT    1
#define REG_VAL_HI       2
#define REG_VAL_LO       3
#define REG_FLAGS        4
#define REG_KEYS_LO      5
#define REG_KEYS_EXT     6
#define REG_TOP_HI       10
#define REG_TOP_LO       11
#define REG_BOT_HI       12
#define REG_BOT_LO       13
#define REG_LED          14
#define REG_CTRL         15
#define REG_FMT          16
#define REG_RAW0         20
#define REG_INFO         100
#define REG_CMD          120
#define REG_CMD_RESULT   121
#define REG_DIAG         130
#define REG_WINDOW       200
#define WINDOW_REGS      960u

/* REG_FLAGS */
#define FLG_NEW_VALUE    0x0001u
/* REG_CTRL */
#define CTRL_BEEP_SHORT  0x0100u
#define CTRL_BEEP_LONG   0x0200u
#define CTRL_KEYLOCK     0x0400u
/* REG_FMT */
#define FMT_WIDE         0x0100u
#define FMT_ZEROS        0x0200u
#define FMT_BLINK_TOP    0x0400u
#define FMT_BLINK_BOT    0x0800u
#define FMT_RAW          0x1000u

/* lệnh ghi vào REG_CMD */
enum { CMD_NONE = 0, CMD_APPLY = 1, CMD_RELOAD = 2, CMD_FACTORY = 3, CMD_IDENTIFY = 4, CMD_EXIT = 5, CMD_ENTER_CONFIG = 6, CMD_CLEAR_DIAG = 7 };
/* kết quả */
enum { CR_OK = 0, CR_NOT_CONFIG = 0x20, CR_UNKNOWN = 0x21 };      /* và RT_E_* / 0x10+ST_* từ cfg_save */

typedef struct {
    uint16_t key_code, key_count, flags, val_hi, val_lo, keys_lo, keys_ext;
    int32_t  top, bot;
    uint16_t led, ctrl, fmt;
    uint8_t  seg[8];
} slave_state_t;

void  cfgsrv_init(void);
const mbs_ops_t *cfgsrv_ops(void);
slave_state_t *cfgsrv_state(void);

bool  cfgsrv_config_mode(void);
void  cfgsrv_enter_config(void);                 /* vào chế độ cấu hình (giữ thông số nối tiếp hiện tại) */
bool  cfgsrv_identify_active(uint32_t now);
bool  cfgsrv_reset_due(uint32_t now);            /* lệnh EXIT đã nhận: app khởi động lại khi true */
void  cfgsrv_on_key(uint8_t kc, uint8_t sw);     /* cập nhật thanh ghi 0,1 */
void  cfgsrv_set_keys(uint32_t raw);             /* thanh ghi 5,6 */
void  cfgsrv_set_entered(int32_t value, uint8_t obj);   /* thanh ghi 2,3,4 + cờ giá trị mới */
void  cfgsrv_note_request(uint32_t now);         /* có yêu cầu hợp lệ gửi tới (cho bộ đếm nhàn rỗi chế độ cấu hình) */
uint32_t cfgsrv_last_request(void);

#endif
