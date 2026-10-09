#include "app.h"
#include "buzzer.h"
#include "cfg.h"
#include "cfgsrv.h"
#include "config.h"
#include "display.h"
#include "engine.h"
#include "hwif.h"
#include "keys.h"
#include "mb_master.h"
#include "mb_slave.h"
#include "rs485.h"
#include "ui.h"

typedef enum { ROLE_WINDOW, ROLE_MASTER, ROLE_SLAVE, ROLE_CONFIG } role_t;

static role_t   role;
static uint32_t t_boot;
static uint8_t  config_station;

/* Giữ ESC + ENT lúc cấp nguồn (>= ~100 ms) -> menu cài đặt */
static bool boot_menu_held(void)
{
    const uint32_t both = (1ul << 15) | (1ul << 3);        /* SW16 = ESC, SW4 = ENT */
    uint32_t start = hw_millis();
    uint8_t hits = 0, samples = 0;
    while ((uint32_t)(hw_millis() - start) < 120u) {
        uint8_t k[4];
        if (hw_tm_read_keys(k)) {
            samples++;
            if ((keys_decode(k, false) & both) == both)
                hits++;
        }
        hw_wdt_kick();
        uint32_t t = hw_millis();
        while (hw_millis() == t) {
        }
        /* nhịp ~1 ms giữa các lần đọc */
    }
    return samples >= 8u && hits >= (uint8_t)(samples - 2u);
}

static void bus_start(void)
{
    const rt_comm_t *c = &cfg()->comm;
    rs485_init(rtcfg_baud(c->baud_idx), c->parity, c->stop);
    if (c->mode == RT_MODE_MASTER) {
        mbm_init(c->plc_id, c->timeout_ms, c->retries);
        role = ROLE_MASTER;
    } else {
        role = ROLE_SLAVE;
    }
}

static void to_config(bool reinit, uint8_t station)
{
    if (reinit)
        rs485_init(CONFIG_BAUD, 0, 1);
    config_station = station;
    cfgsrv_enter_config();
    role = ROLE_CONFIG;
}

/* phục vụ một khung với vai trò slave; trả về true nếu đã trả lời */
static bool serve(uint8_t id, uint32_t now)
{
    if (!rs485_rx_ready())
        return false;
    uint16_t len;
    bool bad;
    const uint8_t *f = rs485_rx(&len, &bad);
    uint16_t rl = 0;
    if (!bad)
        rl = mbs_process(id, f, len, rs485_txbuf(), cfgsrv_ops());
    if (rl) {
        cfgsrv_note_request(now);
        rs485_send(rl);
        return true;
    }
    rs485_rx_release();
    return false;
}

void app_init(void)
{
    hw_tm_init();
    cfg_load();
    const rt_comm_t *c = &cfg()->comm;
    buzzer_init(c->beep);
    display_init(c->brightness);
    keys_init((c->flags & RT_CF_EXTKEYS) != 0u);
    cfgsrv_init();
    bool menu = boot_menu_held();

    ui_init();
    if (menu)
        ui_request_menu();
    engine_init();

    rs485_init(CONFIG_BAUD, 0, 1);                  /* cửa sổ cấu hình 3 s đầu: slave 247, 9600 8N1 */
    config_station = CONFIG_STATION;
    role = ROLE_WINDOW;
    t_boot = hw_millis();
    hw_wdt_start();
}

void app_loop(void)
{
    uint32_t now = hw_millis();
    hw_wdt_kick();

    switch (role) {
    case ROLE_WINDOW:
        if (serve(CONFIG_STATION, now)) {
            to_config(false, CONFIG_STATION);       /* PC đã hỏi: ở lại chế độ cấu hình (9600 8N1, trạm 247) */
        } else if ((uint32_t)(now - t_boot) >= CONFIG_WINDOW_MS) {
            if (cfgsrv_config_mode())
                role = ROLE_CONFIG;
            else
                bus_start();
        }
        break;
    case ROLE_MASTER:
        mbm_poll(now);
        engine_poll(now);
        if (cfgsrv_config_mode())
            to_config(true, CONFIG_STATION);        /* vào từ menu "PC" */
        break;
    case ROLE_SLAVE:
        serve(cfg()->comm.kbd_id, now);
        if (cfgsrv_config_mode())
            to_config(false, cfg()->comm.kbd_id);   /* lệnh ENTER_CONFIG của PC: giữ nguyên thông số */
        break;
    case ROLE_CONFIG:
        serve(config_station, now);
        if ((uint32_t)(now - cfgsrv_last_request()) >= CONFIG_IDLE_MS)
            hw_reset();
        break;
    }
    if (cfgsrv_reset_due(now) && !rs485_tx_busy())
        hw_reset();

    keys_poll(now);
    ui_poll(now);
    buzzer_poll(now);
    display_flush(now);
}
