/*
 * Quét phím (debounce, đếm sự kiện, còi) và cập nhật hiển thị theo thanh ghi Modbus.
 * Chạy trong vòng lặp chính, độc lập với truyền thông RS485.
 */
#ifndef PANEL_H
#define PANEL_H

#include <stdint.h>

void panel_init(void);
void panel_poll(uint32_t now_ms);

#endif
