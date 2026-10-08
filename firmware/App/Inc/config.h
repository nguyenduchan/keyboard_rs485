/*
 * Cấu hình firmware bàn phím Modbus RTU slave.
 * Sửa ở đây (hoặc truyền -DNAME=value khi build) rồi nạp lại.
 */
#ifndef CONFIG_H
#define CONFIG_H

/* ---------- Modbus RTU ---------- */
#ifndef MODBUS_SLAVE_ID
#define MODBUS_SLAVE_ID     1u          /* 1..247 */
#endif
#ifndef MODBUS_BAUDRATE
#define MODBUS_BAUDRATE     9600u       /* 9600 / 19200 / 38400 / 57600 / 115200 */
#endif

#define MB_PARITY_NONE      0u
#define MB_PARITY_EVEN      1u
#define MB_PARITY_ODD       2u
#ifndef MODBUS_PARITY
#define MODBUS_PARITY       MB_PARITY_NONE
#endif
#ifndef MODBUS_STOP_BITS
#define MODBUS_STOP_BITS    1u          /* 1 hoặc 2 */
#endif

/* ---------- Bàn phím ---------- */
#define KEY_SCAN_PERIOD_MS  5u          /* chu kỳ đọc phím từ TM1638 */
#define KEY_DEBOUNCE_SAMPLES 4u         /* 4 x 5 ms = 20 ms ổn định mới nhận */

/* ---------- Còi ---------- */
#define BUZZER_ENABLE       1u
#define BUZZER_BEEP_MS      40u         /* tiếng bíp mỗi lần nhận phím */

/* ---------- Hiển thị ---------- */
#define DISPLAY_BRIGHTNESS  5u          /* 0..7 (TM1638 pulse width 1/16 .. 14/16) */
#define DISPLAY_LEADING_ZERO_BLANK 1u   /* 1: 12 hiển thị "  12"; 0: "0012" */
#define DISPLAY_REFRESH_MS  250u        /* ghi lại RAM TM1638 định kỳ (chống nhiễu ESD) */
#define LAMP_TEST_MS        600u        /* sáng hết khi khởi động; 0 = bỏ */

/* ---------- Watchdog ---------- */
#define IWDG_ENABLE         1u
#define IWDG_TIMEOUT_MS     500u        /* LSI/32 ~ 1 tick/ms, tối đa 4095 */

#endif
