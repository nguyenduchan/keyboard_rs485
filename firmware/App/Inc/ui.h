/*
 * Giao diện người dùng: màn khởi động, trang (RUN / danh sách thông số), nhập số, mật khẩu,
 * chế độ slave, chế độ cấu hình PC, menu cài đặt và kiểm tra phần cứng.
 */
#ifndef UI_H
#define UI_H
#include <stdint.h>
#include <stdbool.h>
#include "keys.h"

typedef enum { UI_BANNER = 0, UI_PAGE, UI_EDIT, UI_PASS, UI_MENU, UI_CONFIG, UI_SLAVE } ui_state_t;

void       ui_init(void);
void       ui_poll(uint32_t now);
ui_state_t ui_state(void);
void       ui_request_menu(void);                           /* giữ ESC+ENT khi cấp nguồn */
uint8_t    ui_level(void);                                  /* cấp quyền hiện tại 0..2 */
bool       ui_access_ok(uint8_t needed);                    /* có đủ quyền / không đặt mật khẩu? */
void       ui_msg(uint8_t row, const char *txt, uint32_t ms);   /* chữ phủ lên một hàng trong thời gian ngắn */

/* dùng nội bộ giữa ui.c và menu.c */
uint32_t   ui_now(void);

#endif
