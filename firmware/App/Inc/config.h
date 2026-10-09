/*
 * Hằng số biên dịch của firmware Panel Keyboard (STM32G030F6P6, 32 KB flash / 8 KB RAM).
 * Cấu hình người dùng (trang, phím, LED...) nằm trong khối runtime (rtcfg.h), không ở đây.
 */
#ifndef CONFIG_H
#define CONFIG_H

#define FW_VER_MAJOR        1u
#define FW_VER_MINOR        0u
#define FW_VER_PATCH        0u
#define FW_MODEL_ID         0x0001u     /* PK-4x4-G030 */

/* ---------- Bản đồ flash: 28 KB ứng dụng + 2 trang cấu hình A/B (2 KB mỗi trang) ---------- */
#define STORAGE_BANK_A      0x08007000u
#define STORAGE_BANK_B      0x08007800u
#define STORAGE_BANK_SIZE   2048u

/* ---------- Cửa sổ cấu hình sau khi cấp nguồn (FW-60) ---------- */
#define CONFIG_WINDOW_MS    3000u
#define CONFIG_STATION      247u
#define CONFIG_BAUD         9600u
#define CONFIG_IDLE_MS      120000u     /* chế độ cấu hình không có yêu cầu nào quá lâu -> khởi động lại */

/* ---------- Giao diện ---------- */
#define BANNER_MS           1000u       /* màn khởi động (FW-03) */
#define KEY_SCAN_PERIOD_MS  5u
#define KEY_DEBOUNCE_SAMPLES 5u         /* 5 x 5 ms = 25 ms (FW-44: 20..30 ms) */
#define KEY_LONG_MS         1000u       /* giữ ESC: huỷ nhập */
#define KEY_ZERO_LONG_MS    800u        /* giữ 0: đổi dấu */
#define KEY_REPEAT_FIRST_MS 500u        /* giữ ▲▼: lặp sau 0,5 s */
#define KEY_REPEAT_MS       220u
#define KEY_REPEAT_FAST_MS  90u
#define KEY_REPEAT_ACCEL    6u          /* sau 6 lần lặp thì lặp nhanh */
#define DISPLAY_REFRESH_MS  250u        /* ghi lại RAM TM1638 định kỳ (chống ESD) */
#define BLINK_HALF_MS       250u        /* nháy 2 Hz */
#define MSG_MS              1500u       /* thông báo lỗi / xác nhận */
#define PASS_MAX            9999u

/* ---------- Còi ---------- */
#define BEEP_CLICK_MS       18u
#define BEEP_OK_MS          70u
#define BEEP_ERR_MS         400u

/* ---------- Modbus master ---------- */
#define POLL_MAX_GAP        2u          /* gộp địa chỉ cách nhau tối đa 2 thanh ghi */
#define POLL_MAX_REGS       100u        /* tối đa thanh ghi / lệnh đọc gộp */
#define POLL_MAX_BITS       400u
#define POLL_ERR_MS         500u        /* chu kỳ hỏi khi mất liên lạc */
#define MB_URGENT_DEPTH     6u

/* ---------- Watchdog / nguồn ---------- */
#define IWDG_TIMEOUT_MS     500u

/* ---------- Giới hạn khối cấu hình (khớp RUNTIME_FORMAT.md) ---------- */
#define RT_MAX_PAGES        32u
#define RT_MAX_FKEYS        10u
#define RT_MAX_LEDS         8u
#define RT_MAX_ALARMS       16u

#endif
