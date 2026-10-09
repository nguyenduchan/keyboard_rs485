/*
 * Bộ máy chạy chế độ MASTER: hỏi vòng dữ liệu đang hiển thị (+ LED + cảnh báo), bộ nhớ đệm giá trị,
 * ghi xuống PLC (kể cả đọc-sửa-ghi cho bit), phím chức năng, cảnh báo.
 */
#ifndef ENGINE_H
#define ENGINE_H
#include <stdint.h>
#include <stdbool.h>
#include "rtcfg.h"

/* mã lỗi hiển thị (E-xx) */
enum { ERR_NONE = 0, ERR_CO, ERR_AD, ERR_FN, ERR_DA, ERR_HI, ERR_LO, ERR_BU, ERR_CF, ERR_PF, ERR_CODES };
const char *err_text(uint8_t code);             /* "E-CO"... */

typedef enum { W_IDLE = 0, W_BUSY, W_OK, W_ERR } wstate_t;

void engine_init(void);                         /* sau khi cfg đã nạp và mbm_init() */
void engine_set_page(uint8_t page);             /* dựng lại danh sách hỏi vòng cho trang này */
void engine_poll(uint32_t now);

/* dữ liệu */
bool    engine_obj_value(const rt_obj_t *o, int32_t *disp);     /* false nếu chưa có dữ liệu hợp lệ */
bool    engine_comm_ok(void);
uint8_t engine_error(void);                     /* lỗi giao tiếp hiện hành (ERR_*), 0 nếu bình thường */

/* LED & cảnh báo */
uint8_t  engine_led_on(void);
uint8_t  engine_led_blink(void);
uint16_t engine_alarms_active(void);
uint16_t engine_alarms_pending(void);           /* đang báo và chưa xác nhận */
bool     engine_alarm_ack(void);                /* xác nhận mọi cảnh báo cần ack; true nếu có gì để xác nhận */

/* ghi từ giao diện (một lệnh một lúc) */
bool     engine_write_obj(const rt_obj_t *o, int32_t disp);     /* false nếu bận hoặc mã hoá lỗi (engine_write_state báo) */
wstate_t engine_write_state(uint8_t *err);
void     engine_write_done(void);

/* phím chức năng: idx 0..9; (nhấn/nhả); GOTO/NEXT/PREV do giao diện xử lý */
void     engine_fkey(uint8_t idx, bool pressed);

/* kiểm thử: số mục hỏi vòng */
uint8_t  engine_item_count(void);

#endif
