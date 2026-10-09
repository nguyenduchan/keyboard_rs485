/* Menu cài đặt trên máy (FW-70..73): vào bằng giữ ESC+ENT khi cấp nguồn. */
#ifndef MENU_H
#define MENU_H
#include <stdint.h>
#include <stdbool.h>
#include "keys.h"

typedef enum { MENU_STAY = 0, MENU_EXIT } menu_res_t;

void       menu_enter(void);
menu_res_t menu_key(const key_event_t *ev, uint32_t now);
menu_res_t menu_poll(uint32_t now);             /* nhấn giữ, hết giờ chờ xác nhận... */
void       menu_draw(uint32_t now);

#endif
