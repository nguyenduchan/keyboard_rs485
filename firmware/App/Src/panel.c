#include "panel.h"
#include "panel_map.h"
#include "tm1638.h"
#include "regs.h"
#include "main.h"
#include "config.h"

static uint32_t last_scan_ms;
static uint32_t last_refresh_ms;
static uint32_t beep_until_ms;
static uint8_t beeping;

static uint16_t candidate;      /* mẫu thô đang chờ ổn định */
static uint8_t stable_count;
static uint16_t debounced;      /* trạng thái đã lọc = thanh ghi 40001 */

static void buzzer_set(uint8_t on)
{
    HAL_GPIO_WritePin(BZ_ON_GPIO_Port, BZ_ON_Pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void refresh_display(void)
{
    uint8_t ram[TM1638_RAM_SIZE];
    panel_build_ram(regs_get(REG_DISPLAY1), regs_get(REG_DISPLAY2),
                    (uint8_t)regs_get(REG_LEDS), ram);
    tm1638_write_ram(ram, DISPLAY_BRIGHTNESS);
}

static void beep_start(uint32_t now_ms)
{
#if BUZZER_ENABLE
    buzzer_set(1);
    beeping = 1;
    beep_until_ms = now_ms + BUZZER_BEEP_MS;
#else
    (void)now_ms;
#endif
}

static void scan_keys(uint32_t now_ms)
{
    uint8_t raw[4];
    if (!tm1638_read_keys(raw))
        return;
    uint32_t decoded = panel_decode_keys(raw);
    if (decoded == KEY_GHOST)
        return;  /* tổ hợp mơ hồ: bỏ mẫu, giữ trạng thái cũ */

    uint16_t sample = (uint16_t)decoded;
    if (sample != candidate) {
        candidate = sample;
        stable_count = 1;
        return;
    }
    if (stable_count < KEY_DEBOUNCE_SAMPLES)
        stable_count++;
    if (stable_count < KEY_DEBOUNCE_SAMPLES || candidate == debounced)
        return;

    uint16_t pressed = (uint16_t)(candidate & ~debounced);
    debounced = candidate;
    regs_set_key_state(debounced);
    if (pressed) {
        regs_add_key_events(panel_popcount16(pressed));
        beep_start(now_ms);
    }
}

void panel_init(void)
{
    tm1638_init();
#if LAMP_TEST_MS > 0
    uint8_t ram[TM1638_RAM_SIZE];
    for (uint8_t i = 0; i < TM1638_RAM_SIZE; i++)
        ram[i] = (i & 1u) ? 0x01u : 0xFFu;
    tm1638_write_ram(ram, DISPLAY_BRIGHTNESS);
    HAL_Delay(LAMP_TEST_MS);  /* chạy trước khi bật IWDG */
#endif
    refresh_display();
    last_refresh_ms = HAL_GetTick();
    last_scan_ms = last_refresh_ms;
}

void panel_poll(uint32_t now_ms)
{
    if ((uint32_t)(now_ms - last_scan_ms) >= KEY_SCAN_PERIOD_MS) {
        last_scan_ms = now_ms;
        scan_keys(now_ms);
    }

    if (beeping && (int32_t)(now_ms - beep_until_ms) >= 0) {
        buzzer_set(0);
        beeping = 0;
    }

    /* Ghi ngay khi PLC đổi giá trị, và ghi lại định kỳ để tự phục hồi nếu TM1638 bị nhiễu */
    if (regs_take_output_changed() || (uint32_t)(now_ms - last_refresh_ms) >= DISPLAY_REFRESH_MS) {
        last_refresh_ms = now_ms;
        refresh_display();
    }
}
