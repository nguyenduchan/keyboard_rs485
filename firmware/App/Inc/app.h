/*
 * Ứng dụng bàn phím Modbus — gọi từ Core/Src/main.c (khối USER CODE).
 * Bản đồ thanh ghi: regs.h. Cấu hình: config.h.
 */
#ifndef APP_H
#define APP_H

void app_init(void);  /* sau các hàm MX_xxx_Init() */
void app_loop(void);  /* gọi liên tục trong while(1) */

#endif
