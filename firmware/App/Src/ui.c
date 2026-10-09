#include "ui.h"
#include "menu.h"
#include "display.h"
#include "engine.h"
#include "cfg.h"
#include "cfgsrv.h"
#include "buzzer.h"
#include "hwif.h"
#include "config.h"
#include <string.h>

enum { PA_EDIT = 1, PA_PAGE, PA_MENU };

static struct {
    ui_state_t st;
    uint32_t now, t_state, last_act;
    uint8_t  level;
    uint8_t  page;
    bool     menu_req;
    /* nhập số */
    const rt_obj_t *eobj;
    uint8_t  erow;
    int32_t  eacc;
    uint8_t  edig;
    bool     eneg, ewait, ezero;
    /* mật khẩu */
    uint8_t  pass_need, pass_act, pass_arg, pass_len;
    uint16_t pass_val;
    /* phủ chữ / xác nhận */
    char     msg[5];
    uint8_t  msg_row;
    uint32_t msg_until;
    uint32_t okflash_until;
    uint8_t  okflash_row;
    /* nhấn giữ */
    uint8_t  held_sw;
    bool     long_done;
    uint32_t rep_next;
    uint8_t  rep_n;
    /* slave */
    int32_t  sacc;
    uint8_t  sdig;
    bool     sneg;
    bool     cfg_screen;
} ui;

/* ------------------------------------------------------------------ tiện ích */
ui_state_t ui_state(void)
{
    return ui.st;
}

uint8_t ui_level(void)
{
    return ui.level;
}

uint32_t ui_now(void)
{
    return ui.now;
}

static uint16_t pwd_of(uint8_t need)
{
    return need == 1u ? cfg()->sec.pwd_user : (need == 2u ? cfg()->sec.pwd_tech : 0u);
}

bool ui_access_ok(uint8_t need)
{
    if (need == 0u || pwd_of(need) == 0u)
        return true;
    return ui.level >= need;
}

void ui_msg(uint8_t row, const char *txt, uint32_t ms)
{
    strncpy(ui.msg, txt, 4);
    ui.msg[4] = 0;
    ui.msg_row = row;
    ui.msg_until = ui.now + ms;
}

static bool msg_active(void)
{
    return ui.msg_until && (int32_t)(ui.msg_until - ui.now) > 0;
}

static void touch(void)
{
    ui.last_act = ui.now;
}

static const rt_page_t *cur_page(void)
{
    const rt_blob_t *c = cfg();
    return ui.page < c->comm.page_count ? &c->page[ui.page] : 0;
}

static bool is_bit_obj(const rt_obj_t *o)
{
    return o->area == RT_AREA_COIL || o->bit != RT_BIT_NONE;
}

static void error_feedback(const char *txt, uint8_t row)
{
    ui_msg(row, txt, MSG_MS);
    buzzer_error();
}

/* ------------------------------------------------------------------ chuyển trạng thái */
static void set_state(ui_state_t s)
{
    ui.st = s;
    ui.t_state = ui.now;
    ui.held_sw = 0;
    ui.long_done = false;
}

static void go_page(uint8_t p)
{
    const rt_blob_t *c = cfg();
    if (c->comm.page_count == 0u)
        return;
    p = (uint8_t)(p % c->comm.page_count);
    if (!ui_access_ok(c->page[p].access)) {
        ui.pass_need = c->page[p].access;
        ui.pass_act = PA_PAGE;
        ui.pass_arg = p;
        ui.pass_len = 0;
        ui.pass_val = 0;
        set_state(UI_PASS);
        return;
    }
    ui.page = p;
    engine_set_page(p);
    ui.eobj = 0;
    set_state(UI_PAGE);
}

static void go_run(void)
{
    go_page(cfg()->comm.run_page);
}

static void start_pass(uint8_t need, uint8_t act, uint8_t arg)
{
    ui.pass_need = need;
    ui.pass_act = act;
    ui.pass_arg = arg;
    ui.pass_len = 0;
    ui.pass_val = 0;
    set_state(UI_PASS);
}

static void begin_edit(const rt_obj_t *o, uint8_t row)
{
    ui.eobj = o;
    ui.erow = row;
    ui.eacc = 0;
    ui.edig = 0;
    ui.eneg = false;
    ui.ewait = false;
    ui.ezero = false;
    set_state(UI_EDIT);
}

static void enter_slave_or_page(void)
{
    if (cfgsrv_config_mode()) {
        ui.cfg_screen = true;
        cfgsrv_state()->fmt |= FMT_RAW;
        memset(cfgsrv_state()->seg, 0, 8);
        cfgsrv_state()->seg[0] = display_char_seg('P');
        cfgsrv_state()->seg[1] = display_char_seg('C');
        set_state(UI_CONFIG);
    } else if (cfg()->comm.mode == RT_MODE_SLAVE) {
        set_state(UI_SLAVE);
    } else {
        go_run();
        if (ui.st == UI_BANNER)
            set_state(UI_PAGE);
    }
}

void ui_init(void)
{
    memset(&ui, 0, sizeof ui);
    ui.st = UI_BANNER;
    ui.now = hw_millis();
    ui.t_state = ui.now;
    ui.last_act = ui.now;
}

void ui_request_menu(void)
{
    ui.menu_req = true;
}

/* ------------------------------------------------------------------ vẽ */
static uint8_t disp_flags(const rt_obj_t *o)
{
    return (o->flags & RT_OF_ZEROS) ? DISP_FLAG_ZEROS : 0u;
}

static void draw_obj(uint8_t row, const rt_obj_t *o)
{
    int32_t v;
    switch (o->type) {
    case RT_OBJ_LABEL:
        display_text(row, o->label);
        break;
    case RT_OBJ_SHOW:
    case RT_OBJ_INPUT:
        if (engine_obj_value(o, &v))
            display_number(row, v, o->decimals, disp_flags(o), (o->flags & RT_OF_WIDE) ? 8u : 4u);
        else
            display_row_dashes(row);
        break;
    case RT_OBJ_BIT:
        if (engine_obj_value(o, &v)) {
            if (v) {
                display_text(row, o->label);
            } else {
                uint8_t off[4];
                memcpy(off, &o->min, 4);
                display_text(row, off);
            }
        } else {
            display_row_dashes(row);
        }
        break;
    default:
        break;
    }
}

static void draw_alarm_code(uint8_t idx)
{
    uint8_t t[4] = { 'A', '-', (uint8_t)('0' + (idx + 1u) / 10u), (uint8_t)('0' + (idx + 1u) % 10u) };
    display_text(0, t);
}

static void draw_overlays(void)
{
    uint16_t pend = engine_alarms_pending();
    uint8_t err = engine_error();
    bool phase_a = (ui.now % 1400u) < 700u;

    if (pend && phase_a) {
        uint8_t cnt = 0;
        for (uint8_t i = 0; i < RT_MAX_ALARMS; i++)
            if (pend & (1u << i))
                cnt++;
        uint8_t k = (uint8_t)((ui.now / 1400u) % cnt);      /* các cảnh báo hiện lần lượt */
        for (uint8_t i = 0; i < RT_MAX_ALARMS; i++)
            if ((pend & (1u << i)) && k-- == 0u) {
                draw_alarm_code(i);
                break;
            }
    } else if (err && !phase_a) {
        display_str(0, err_text(err));
    }
}

static void draw_edit(void)
{
    const rt_obj_t *o = ui.eobj;
    bool wide = (o->flags & RT_OF_WIDE) != 0u;
    int32_t v = ui.eneg ? -ui.eacc : ui.eacc;
    if (wide)
        display_clear();
    display_number(wide ? 0u : ui.erow, v, o->decimals, 0u, wide ? 8u : 4u);
    if (ui.eneg && ui.eacc == 0 && o->decimals == 0u)
        display_raw(wide ? 6u : (uint8_t)(ui.erow * 4u + 2u), 0x40);      /* "-0": dấu trừ trước chữ số hàng đơn vị */
    display_row_blink(wide ? 0u : ui.erow, true);
    if (wide)
        display_row_blink(1, true);
}

static void draw_pass(void)
{
    display_str(0, "PASS");
    char t[5] = "____";
    for (uint8_t i = 0; i < ui.pass_len && i < 4u; i++)
        t[i] = '-';
    display_str(1, t);
}

static void draw_page(void)
{
    const rt_page_t *p = cur_page();
    display_clear();
    if (p) {
        bool wide = (p->top.flags & RT_OF_WIDE) && (p->top.type == RT_OBJ_SHOW || p->top.type == RT_OBJ_INPUT);
        draw_obj(0, &p->top);
        if (!wide)
            draw_obj(1, &p->bot);
    }
    display_leds(engine_led_on(), engine_led_blink());
}

static void slave_render(void)
{
    slave_state_t *s = cfgsrv_state();
    display_clear();
    if (s->fmt & FMT_RAW) {
        for (uint8_t d = 0; d < 8u; d++)
            display_raw(d, s->seg[d]);
    } else {
        uint8_t fl = (s->fmt & FMT_ZEROS) ? DISP_FLAG_ZEROS : 0u;
        uint8_t dt = (uint8_t)(s->fmt & 3u), db = (uint8_t)((s->fmt >> 4) & 3u);
        if (s->fmt & FMT_WIDE) {
            display_number(0, s->top, dt, fl, 8);
        } else {
            display_number(0, s->top, dt, fl, 4);
            if (ui.sdig)
                display_number(1, ui.sneg ? -ui.sacc : ui.sacc, db, 0u, 4);
            else
                display_number(1, s->bot, db, fl, 4);
        }
        display_row_blink(0, (s->fmt & FMT_BLINK_TOP) != 0u);
        display_row_blink(1, ((s->fmt & FMT_BLINK_BOT) != 0u) || ui.sdig);
    }
    display_leds((uint8_t)s->led, (uint8_t)(s->led >> 8));
    display_set_brightness((uint8_t)(s->ctrl & 7u));
}

static void draw_banner(void)
{
    display_clear();
    uint32_t t = ui.now - ui.t_state;
    if (t < BANNER_MS / 2u) {
        display_number(0, (int32_t)(FW_VER_MAJOR * 100u + FW_VER_MINOR * 10u + FW_VER_PATCH), 2, 0u, 4);
        display_str(1, cfg()->comm.mode == RT_MODE_MASTER ? "nSt " : "SLv ");
    } else {
        display_str(0, " Id ");
        display_number(1, cfg()->comm.mode == RT_MODE_MASTER ? cfg()->comm.plc_id : cfg()->comm.kbd_id, 0, 0u, 4);
    }
    display_leds(0xFF, 0);                     /* thử LED khi khởi động */
}

/* ------------------------------------------------------------------ xử lý phím theo trạng thái */
static void do_fkey(uint8_t idx, bool pressed)
{
    const rt_fkey_t *f = &cfg()->fkey[idx];
    if (f->action == RT_FA_NONE)
        return;
    if (pressed && !ui_access_ok(f->access)) {
        error_feedback("LoC ", 0);
        return;
    }
    switch (f->action) {
    case RT_FA_GOTO:
        if (pressed)
            go_page((uint8_t)f->value);
        break;
    case RT_FA_NEXT:
        if (pressed)
            go_page((uint8_t)(ui.page + 1u));
        break;
    case RT_FA_PREV:
        if (pressed)
            go_page((uint8_t)(ui.page ? ui.page - 1u : cfg()->comm.page_count - 1u));
        break;
    default:
        if (pressed && ui.level < f->access && pwd_of(f->access))
            return;
        engine_fkey(idx, pressed);       /* nhả luôn được chuyển xuống: engine chỉ nhả khi đã nhấn */
        break;
    }
}

static const rt_obj_t *edit_target(const rt_page_t *p, uint8_t *row)
{
    if (p->bot.type == RT_OBJ_INPUT && !((p->top.flags & RT_OF_WIDE) && (p->top.type == RT_OBJ_SHOW || p->top.type == RT_OBJ_INPUT))) {
        *row = 1;
        return &p->bot;
    }
    if (p->top.type == RT_OBJ_INPUT) {
        *row = 0;
        return &p->top;
    }
    return 0;
}

static uint8_t digits_of(int32_t v)
{
    uint8_t n = 1;
    while (v >= 10) {
        v /= 10;
        n++;
    }
    return n;
}

static void edit_digit(uint8_t d)
{
    bool wide = (ui.eobj->flags & RT_OF_WIDE) != 0u;
    uint8_t maxd = (uint8_t)((wide ? 8u : 4u) - (ui.eneg ? 1u : 0u));
    if (is_bit_obj(ui.eobj))
        maxd = 1;
    if (ui.eacc != 0 && digits_of(ui.eacc) >= maxd)
        return;
    if (ui.eacc == 0 && d == 0u && ui.edig >= maxd)
        return;
    ui.eacc = ui.eacc * 10 + d;
    ui.edig++;
    ui.ezero = (d == 0u);
}

static void edit_commit(void)
{
    const rt_obj_t *o = ui.eobj;
    int32_t v = ui.eneg ? -ui.eacc : ui.eacc;
    int32_t lo = o->min, hi = o->max;
    if (is_bit_obj(o)) {
        lo = 0;
        hi = 1;
    }
    if (ui.edig == 0u) {
        go_page(ui.page);                       /* không gõ gì: huỷ */
        return;
    }
    if (v < lo) {
        error_feedback("E-Lo", ui.erow);
        return;
    }
    if (v > hi) {
        error_feedback("E-Hi", ui.erow);
        return;
    }
    if (!engine_write_obj(o, v)) {
        error_feedback("E-bU", ui.erow);
        return;
    }
    ui.ewait = true;
}

static void edit_key(const key_event_t *ev)
{
    uint8_t kc = keys_logical(ev->sw);
    if (ui.ewait)
        return;
    if (ev->type == KEV_PRESS) {
        int8_t d = kc_digit(kc);
        if (d >= 0) {
            edit_digit((uint8_t)d);
        } else if (kc == KC_ENT) {
            edit_commit();
        }
    } else if (ev->type == KEV_RELEASE) {
        if (kc == KC_ESC && !ui.long_done) {
            if (ui.edig == 0u) {
                go_page(ui.page);
            } else {
                ui.eacc /= 10;
                ui.edig--;
                if (ui.edig == 0u)
                    ui.eneg = false;
            }
        }
        ui.long_done = false;
    }
}

static void edit_poll(void)
{
    if (ui.ewait) {
        uint8_t err = 0;
        wstate_t w = engine_write_state(&err);
        if (w == W_OK) {
            ui.okflash_until = ui.now + 600u;
            ui.okflash_row = ui.erow;
            engine_write_done();
            go_page(ui.page);
        } else if (w == W_ERR) {
            engine_write_done();
            ui_msg(ui.erow, err_text(err), MSG_MS);
            go_page(ui.page);
        }
        return;
    }
    uint8_t sw = keys_active();
    if (!sw)
        return;
    uint8_t kc = keys_logical(sw);
    uint32_t held = keys_held_ms(ui.now);
    if (kc == KC_ESC && held >= KEY_LONG_MS && !ui.long_done) {
        ui.long_done = true;
        go_page(ui.page);                       /* giữ ESC: huỷ */
    } else if (kc == KC_0 && held >= KEY_ZERO_LONG_MS && !ui.long_done) {
        ui.long_done = true;                    /* giữ 0: đổi dấu */
        if (ui.eobj->min < 0 && !is_bit_obj(ui.eobj)) {
            if (ui.ezero && ui.edig > 0u) {
                ui.edig--;
                ui.eacc /= 10;
            }
            ui.eneg = !ui.eneg;
            if (ui.edig == 0u)
                ui.edig = 1;
        }
    }
}

static void pass_key(const key_event_t *ev)
{
    if (ev->type != KEV_PRESS)
        return;
    uint8_t kc = keys_logical(ev->sw);
    int8_t d = kc_digit(kc);
    if (d >= 0) {
        if (ui.pass_len < 4u) {
            ui.pass_val = (uint16_t)(ui.pass_val * 10u + (uint16_t)d);
            ui.pass_len++;
        }
    } else if (kc == KC_ESC) {
        if (ui.pass_len) {
            ui.pass_len = 0;
            ui.pass_val = 0;
        } else {
            go_run();
            if (ui.st == UI_PASS)
                set_state(UI_PAGE);
        }
    } else if (kc == KC_ENT) {
        uint16_t want = pwd_of(ui.pass_need);
        if (ui.pass_len == 0u || ui.pass_val != want) {
            error_feedback("E-Pd", 1);
            ui.pass_len = 0;
            ui.pass_val = 0;
            return;
        }
        if (ui.level < ui.pass_need)
            ui.level = ui.pass_need;
        switch (ui.pass_act) {
        case PA_PAGE:
            go_page(ui.pass_arg);
            break;
        case PA_MENU:
            menu_enter();
            set_state(UI_MENU);
            break;
        default: {
            const rt_page_t *p = cur_page();
            uint8_t row = 0;
            const rt_obj_t *o = p ? edit_target(p, &row) : 0;
            set_state(UI_PAGE);
            if (o)
                begin_edit(o, row);
            break;
        }
        }
    }
}

static void page_key(const key_event_t *ev)
{
    uint8_t kc = keys_logical(ev->sw);
    const rt_page_t *p = cur_page();
    bool press = ev->type == KEV_PRESS;

    if (kc >= KC_F1 && kc <= KC_F10) {
        do_fkey((uint8_t)(kc - KC_F1), press);
        return;
    }
    if (!press)
        return;
    int8_t d = kc_digit(kc);
    if (d >= 0) {
        uint8_t row = 0;
        const rt_obj_t *o = p ? edit_target(p, &row) : 0;
        if (!o)
            return;
        if (!ui_access_ok(o->access)) {
            start_pass(o->access, PA_EDIT, 0);
            return;
        }
        begin_edit(o, row);
        edit_digit((uint8_t)d);
    } else if (kc == KC_UP) {
        go_page((uint8_t)(ui.page ? ui.page - 1u : (cfg()->comm.page_count ? cfg()->comm.page_count - 1u : 0u)));
        ui.rep_next = ui.now + KEY_REPEAT_FIRST_MS;
        ui.rep_n = 0;
    } else if (kc == KC_DOWN) {
        go_page((uint8_t)(ui.page + 1u));
        ui.rep_next = ui.now + KEY_REPEAT_FIRST_MS;
        ui.rep_n = 0;
    } else if (kc == KC_ESC) {
        if (!engine_alarm_ack() && ui.page != cfg()->comm.run_page)
            go_run();
    }
}

static void page_poll(void)
{
    uint8_t sw = keys_active();
    if (!sw)
        return;
    uint8_t kc = keys_logical(sw);
    if ((kc == KC_UP || kc == KC_DOWN) && keys_held_ms(ui.now) >= KEY_REPEAT_FIRST_MS &&
        (int32_t)(ui.now - ui.rep_next) >= 0) {
        if (kc == KC_UP)
            go_page((uint8_t)(ui.page ? ui.page - 1u : (cfg()->comm.page_count ? cfg()->comm.page_count - 1u : 0u)));
        else
            go_page((uint8_t)(ui.page + 1u));
        ui.rep_n++;
        ui.rep_next = ui.now + (ui.rep_n >= KEY_REPEAT_ACCEL ? KEY_REPEAT_FAST_MS : KEY_REPEAT_MS);
    }
}

/* ------------------------------------------------------------------ slave / cấu hình */
static void slave_key(const key_event_t *ev)
{
    uint8_t kc = keys_logical(ev->sw);
    slave_state_t *s = cfgsrv_state();
    if (ev->type == KEV_PRESS) {
        cfgsrv_on_key(kc, ev->sw);
        if (!(s->ctrl & CTRL_KEYLOCK))
            buzzer_click();
    }
    cfgsrv_set_keys(keys_raw());
    if (ui.st == UI_CONFIG || (s->ctrl & CTRL_KEYLOCK))
        return;
    if (ev->type == KEV_PRESS) {
        int8_t d = kc_digit(kc);
        bool wide = (s->fmt & FMT_WIDE) != 0u;
        uint8_t maxd = (uint8_t)((wide ? 8u : 4u) - (ui.sneg ? 1u : 0u));
        if (d >= 0) {
            if (ui.sacc == 0 || digits_of(ui.sacc) < maxd) {
                ui.sacc = ui.sacc * 10 + d;
                ui.sdig++;
            }
        } else if (kc == KC_ENT && ui.sdig) {
            cfgsrv_set_entered(ui.sneg ? -ui.sacc : ui.sacc, 0);
            ui.sacc = 0;
            ui.sdig = 0;
            ui.sneg = false;
        }
    } else if (ev->type == KEV_RELEASE && kc == KC_ESC && !ui.long_done) {
        if (ui.sdig) {
            ui.sacc /= 10;
            ui.sdig--;
            if (!ui.sdig)
                ui.sneg = false;
        }
        ui.long_done = false;
    } else if (ev->type == KEV_RELEASE) {
        ui.long_done = false;
    }
}

static void slave_poll(void)
{
    cfgsrv_set_keys(keys_raw());
    uint8_t sw = keys_active();
    if (!sw || ui.st == UI_CONFIG)
        return;
    uint8_t kc = keys_logical(sw);
    uint32_t held = keys_held_ms(ui.now);
    if (kc == KC_ESC && held >= KEY_LONG_MS && !ui.long_done) {
        ui.long_done = true;
        ui.sacc = 0;
        ui.sdig = 0;
        ui.sneg = false;
    } else if (kc == KC_0 && held >= KEY_ZERO_LONG_MS && !ui.long_done) {
        ui.long_done = true;
        if (ui.sdig) {
            ui.sacc /= 10;
            ui.sdig--;
        }
        ui.sneg = !ui.sneg;
        if (!ui.sdig)
            ui.sdig = 1;
    }
}

/* ------------------------------------------------------------------ vòng chính */
void ui_poll(uint32_t now)
{
    ui.now = now;

    /* PC đã đưa thiết bị vào chế độ cấu hình */
    if (cfgsrv_config_mode() && ui.st != UI_CONFIG && ui.st != UI_BANNER) {
        enter_slave_or_page();
    }

    key_event_t ev;
    while (keys_pop(&ev)) {
        touch();
        if (ev.type == KEV_PRESS && ui.st != UI_SLAVE && ui.st != UI_CONFIG && ui.st != UI_MENU)
            buzzer_click();
        if (ev.type == KEV_RELEASE && ui.st != UI_PAGE && ui.st != UI_SLAVE && ui.st != UI_CONFIG && ui.st != UI_MENU) {
            uint8_t rk = keys_logical(ev.sw);       /* phím giữ đã nhấn rồi đổi trạng thái: vẫn phải nhả */
            if (rk >= KC_F1 && rk <= KC_F10)
                engine_fkey((uint8_t)(rk - KC_F1), false);
        }
        switch (ui.st) {
        case UI_BANNER:
            break;
        case UI_PAGE:
            page_key(&ev);
            break;
        case UI_EDIT:
            edit_key(&ev);
            break;
        case UI_PASS:
            pass_key(&ev);
            break;
        case UI_MENU:
            if (menu_key(&ev, now) == MENU_EXIT)
                enter_slave_or_page();
            break;
        case UI_SLAVE:
        case UI_CONFIG:
            slave_key(&ev);
            break;
        }
    }

    switch (ui.st) {
    case UI_BANNER:
        if ((uint32_t)(now - ui.t_state) >= BANNER_MS) {
            if (ui.menu_req) {
                ui.menu_req = false;
                if (pwd_of(2) != 0u)
                    start_pass(2, PA_MENU, 0);
                else {
                    menu_enter();
                    set_state(UI_MENU);
                }
            } else {
                enter_slave_or_page();
                if (cfg_load_status() == 2u)
                    error_feedback("E-cF", 0);
            }
        }
        break;
    case UI_PAGE:
        page_poll();
        break;
    case UI_EDIT:
        edit_poll();
        break;
    case UI_MENU:
        if (menu_poll(now) == MENU_EXIT)
            enter_slave_or_page();
        break;
    case UI_SLAVE:
    case UI_CONFIG:
        slave_poll();
        break;
    default:
        break;
    }

    /* hết thời gian chờ: đăng xuất + về trang RUN */
    uint16_t idle = cfg()->comm.idle_s;
    if (idle && (ui.st == UI_PAGE || ui.st == UI_EDIT || ui.st == UI_PASS) &&
        (uint32_t)(now - ui.last_act) >= (uint32_t)idle * 1000u) {
        if (ui.st == UI_EDIT && ui.ewait)
            return;
        ui.level = 0;
        go_run();
        if (ui.st == UI_PASS)
            set_state(UI_PAGE);
        touch();
    }

    /* vẽ */
    switch (ui.st) {
    case UI_BANNER:
        draw_banner();
        break;
    case UI_PAGE:
        draw_page();
        draw_overlays();
        if (ui.okflash_until && (int32_t)(ui.okflash_until - now) > 0)
            display_row_blink(ui.okflash_row, true);
        break;
    case UI_EDIT:
        draw_page();
        draw_edit();
        break;
    case UI_PASS:
        draw_pass();
        display_leds(0, 0);
        break;
    case UI_MENU:
        menu_draw(now);
        break;
    case UI_SLAVE:
    case UI_CONFIG:
        slave_render();
        break;
    }
    if (msg_active())
        display_str(ui.msg_row, ui.msg);
    if (ui.st == UI_BANNER || ui.st == UI_PASS || ui.st == UI_MENU) {
        /* LED giữ theo từng màn */
    }
    if (cfgsrv_identify_active(now)) {
        display_clear();
        display_leds(((now / 150u) & 1u) ? 0xFFu : 0x00u, 0);
        display_str(0, "----");
        display_str(1, "----");
    }
    display_set_brightness(ui.st == UI_SLAVE || ui.st == UI_CONFIG ? (uint8_t)(cfgsrv_state()->ctrl & 7u)
                                                                    : cfg()->comm.brightness);
}
