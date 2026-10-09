#include "buzzer.h"
#include "hwif.h"
#include "rtcfg.h"
#include "config.h"

static uint8_t flags;
static uint32_t shot_until;          /* bíp đơn đến thời điểm này */
static bool     shot_active;
static uint32_t ident_until;
static bool     alarm_on;
static bool     out_state;
static uint32_t cur_now;

void buzzer_init(uint8_t f)
{
    flags = f;
    shot_active = false;
    alarm_on = false;
    ident_until = 0;
    out_state = false;
    hw_buzzer(false);
}

static void shot(uint16_t ms)
{
    uint32_t now = hw_millis();
    uint32_t until = now + ms;
    if (!shot_active || (int32_t)(until - shot_until) > 0)
        shot_until = until;
    shot_active = true;
    buzzer_poll(now);
}

void buzzer_click(void)
{
    if (flags & RT_BF_CLICK)
        shot(BEEP_CLICK_MS);
}

void buzzer_ok(void)
{
    if (flags & RT_BF_CLICK)
        shot(BEEP_OK_MS);
}

void buzzer_error(void)
{
    if (flags & RT_BF_ERROR)
        shot(BEEP_ERR_MS);
}

void buzzer_alarm(bool on)
{
    alarm_on = on && (flags & RT_BF_ALARM);
}

void buzzer_force(uint16_t ms)
{
    shot(ms);
}

void buzzer_identify(uint16_t ms)
{
    ident_until = hw_millis() + ms;
}

bool buzzer_is_on(void)
{
    return out_state;
}

void buzzer_poll(uint32_t now)
{
    cur_now = now;
    bool want = false;
    if (shot_active) {
        if ((int32_t)(shot_until - now) > 0)
            want = true;
        else
            shot_active = false;
    }
    if (!want && alarm_on)
        want = (now % 600u) < 200u;                   /* 200 ms kêu / 400 ms im */
    if (!want && ident_until && (int32_t)(ident_until - now) > 0)
        want = (now % 200u) < 100u;
    if (want != out_state) {
        out_state = want;
        hw_buzzer(want);
    }
    (void)cur_now;
}
