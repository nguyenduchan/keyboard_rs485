#include "menu.h"
#include "ui.h"
#include "display.h"
#include "cfg.h"
#include "cfgsrv.h"
#include "buzzer.h"
#include "hwif.h"
#include "config.h"
#include <string.h>

enum {
    M_NODE, M_PLC, M_ADR, M_BAUD, M_PAR, M_STOP, M_TOUT, M_RETR, M_POLL, M_ORD, M_BRIG, M_BEEP,
    M_PUSR, M_PTEC, M_IDLE, M_OBJ, M_TEST, M_PC, M_DEF, M_VER, M_COUNT
};
enum { K_LIST, K_NUM, K_ACTION, K_INFO };
enum { L_LIST, L_EDIT, L_OBJSEL, L_OBJSUB, L_OBJNUM, L_TEST };
enum { OBJ_ADR, OBJ_MIN, OBJ_MAX, OBJ_COUNT };

typedef struct {
    uint8_t name[4];
    uint8_t kind;
} item_t;

#define D 0x80u     /* chấm */
static const item_t items[M_COUNT] = {
    [M_NODE] = { { 'n', 'o', 'd', 'E' }, K_LIST },
    [M_PLC]  = { { 'P', 'L', 'C', ' ' }, K_NUM },
    [M_ADR]  = { { 'A', 'd', 'r', ' ' }, K_NUM },
    [M_BAUD] = { { 'b', 'A', 'u', 'd' }, K_LIST },
    [M_PAR]  = { { 'P', 'A', 'r', ' ' }, K_LIST },
    [M_STOP] = { { 'S', 't', 'o', 'P' }, K_NUM },
    [M_TOUT] = { { 't', 'o', 'u', 't' }, K_NUM },
    [M_RETR] = { { 'r', 'E', 't', 'r' }, K_NUM },
    [M_POLL] = { { 'P', 'o', 'L', 'L' }, K_NUM },
    [M_ORD]  = { { 'O', 'r', 'd', ' ' }, K_LIST },
    [M_BRIG] = { { 'b', 'r', ' ', ' ' }, K_NUM },
    [M_BEEP] = { { 'b', 'E', 'E', 'P' }, K_NUM },
    [M_PUSR] = { { 'P' | D, 'U', 'S', ' ' }, K_NUM },
    [M_PTEC] = { { 'P' | D, 't', 'E', ' ' }, K_NUM },
    [M_IDLE] = { { 'i', 'd', 'L', 'E' }, K_NUM },
    [M_OBJ]  = { { 'o', 'b', 'j', ' ' }, K_ACTION },
    [M_TEST] = { { 't', 'E', 'S', 't' }, K_ACTION },
    [M_PC]   = { { 'P', 'C', ' ', ' ' }, K_ACTION },
    [M_DEF]  = { { 'd', 'E', 'F', ' ' }, K_ACTION },
    [M_VER]  = { { 'U', 'r', ' ', ' ' }, K_INFO },
};

static const uint8_t baud_txt[7][4] = {
    { '2', '4', '0', '0' }, { '4', '8', '0', '0' }, { '9', '6', '0', '0' }, { '1', '9' | D, '2', ' ' },
    { '3', '8' | D, '4', ' ' }, { '5', '7' | D, '6', ' ' }, { '1', '1', '5', ' ' },
};
static const uint8_t par_txt[3][4] = { { 'n', 'o', 'n', 'E' }, { 'E', 'v', 'E', 'n' }, { 'o', 'd', 'd', ' ' } };
static const uint8_t ord_txt[4][4] = { { 'A', 'b', 'C', 'd' }, { 'C', 'd', 'A', 'b' }, { 'b', 'A', 'd', 'C' }, { 'd', 'C', 'b', 'A' } };
static const uint8_t node_txt[2][4] = { { 'n', 'S', 't', ' ' }, { 'S', 'L', 'v', ' ' } };

static struct {
    uint8_t  level;
    uint8_t  item;
    int32_t  acc;
    uint8_t  dig;
    bool     neg;
    uint8_t  listv;
    uint8_t  sub;
    uint8_t  objidx;
    uint32_t sure_until;
    uint32_t t_test;
    uint8_t  test_sw;
    bool     long_done;
} m;

/* ------------------------------------------------------------------ truy cập giá trị */
static int32_t get_num(uint8_t id)
{
    const rt_comm_t *c = &cfg()->comm;
    switch (id) {
    case M_NODE: return c->mode;
    case M_PLC:  return c->plc_id;
    case M_ADR:  return c->kbd_id;
    case M_BAUD: return c->baud_idx;
    case M_PAR:  return c->parity;
    case M_STOP: return c->stop;
    case M_TOUT: return c->timeout_ms;
    case M_RETR: return c->retries;
    case M_POLL: return c->poll_ms;
    case M_ORD:  return (int32_t)c->order - 1;
    case M_BRIG: return c->brightness;
    case M_BEEP: return c->beep;
    case M_PUSR: return cfg()->sec.pwd_user;
    case M_PTEC: return cfg()->sec.pwd_tech;
    case M_IDLE: return c->idle_s;
    default:     return 0;
    }
}

static void set_num(uint8_t id, int32_t v)
{
    rt_blob_t *b = cfg_mut();
    rt_comm_t *c = &b->comm;
    switch (id) {
    case M_NODE: c->mode = (uint8_t)v; break;
    case M_PLC:  c->plc_id = (uint8_t)v; break;
    case M_ADR:  c->kbd_id = (uint8_t)v; break;
    case M_BAUD: c->baud_idx = (uint8_t)v; break;
    case M_PAR:  c->parity = (uint8_t)v; break;
    case M_STOP: c->stop = (uint8_t)v; break;
    case M_TOUT: c->timeout_ms = (uint16_t)v; break;
    case M_RETR: c->retries = (uint8_t)v; break;
    case M_POLL: c->poll_ms = (uint16_t)v; break;
    case M_ORD:  c->order = (uint8_t)(v + 1); break;
    case M_BRIG: c->brightness = (uint8_t)v; break;
    case M_BEEP: c->beep = (uint8_t)v; break;
    case M_PUSR: b->sec.pwd_user = (uint16_t)v; break;
    case M_PTEC: b->sec.pwd_tech = (uint16_t)v; break;
    case M_IDLE: c->idle_s = (uint16_t)v; break;
    default: return;
    }
    cfg_touch();
}

static void range_of(uint8_t id, int32_t *lo, int32_t *hi)
{
    *lo = 0;
    switch (id) {
    case M_PLC: case M_ADR: *lo = 1; *hi = 247; break;
    case M_STOP: *lo = 1; *hi = 2; break;
    case M_TOUT: *lo = 20; *hi = 5000; break;
    case M_RETR: *hi = 5; break;
    case M_POLL: *hi = 5000; break;
    case M_BRIG: case M_BEEP: *hi = 7; break;
    case M_PUSR: case M_PTEC: case M_IDLE: *hi = 9999; break;
    case M_NODE: *hi = 1; break;
    case M_BAUD: *hi = 6; break;
    case M_PAR: *hi = 2; break;
    case M_ORD: *hi = 3; break;
    default: *hi = 0; break;
    }
}

static uint8_t obj_count(void)
{
    return (uint8_t)(cfg()->comm.page_count * 2u);
}

static rt_obj_t *obj_at(uint8_t idx)
{
    rt_page_t *p = &cfg_mut()->page[idx / 2u];
    return (idx & 1u) ? &p->bot : &p->top;
}

static void obj_set(uint8_t field, int32_t v)
{
    rt_obj_t *o = obj_at(m.objidx);
    if (field == OBJ_ADR)
        o->addr = (uint16_t)v;
    else if (field == OBJ_MIN)
        o->min = v;
    else
        o->max = v;
    cfg_touch();
}

/* ------------------------------------------------------------------ vào / ra */
void menu_enter(void)
{
    memset(&m, 0, sizeof m);
    m.level = L_LIST;
    m.item = 0;
    buzzer_ok();
}

static menu_res_t leave(void)
{
    if (cfg_dirty()) {
        uint8_t r = cfg_save();
        if (r != 0u) {
            ui_msg(0, "E-cF", MSG_MS);
            buzzer_error();
            return MENU_STAY;
        }
        buzzer_ok();
        hw_reset();                         /* áp dụng thông số nối tiếp / chế độ mới */
    }
    return MENU_EXIT;
}

/* ------------------------------------------------------------------ nhập số */
static uint8_t digits_of(int32_t v)
{
    uint8_t n = 1;
    while (v >= 10) {
        v /= 10;
        n++;
    }
    return n;
}

static void start_edit(bool list, int32_t cur)
{
    m.dig = 0;
    m.neg = false;
    m.acc = 0;
    m.listv = (uint8_t)cur;
    m.long_done = false;
    (void)list;
}

static void type_digit(uint8_t d, uint8_t maxd)
{
    if (m.acc != 0 && digits_of(m.acc) >= maxd)
        return;
    m.acc = m.acc * 10 + d;
    m.dig++;
}

/* ------------------------------------------------------------------ phím */
static menu_res_t key_list(const key_event_t *ev)
{
    uint8_t kc = keys_logical(ev->sw);
    if (ev->type != KEV_PRESS)
        return MENU_STAY;
    if (kc == KC_UP) {
        m.item = (uint8_t)(m.item ? m.item - 1u : M_COUNT - 1u);
        m.sure_until = 0;
    } else if (kc == KC_DOWN) {
        m.item = (uint8_t)((m.item + 1u) % M_COUNT);
        m.sure_until = 0;
    } else if (kc == KC_ESC) {
        return leave();
    } else if (kc == KC_ENT) {
        const item_t *it = &items[m.item];
        if (it->kind == K_LIST || it->kind == K_NUM) {
            m.level = L_EDIT;
            start_edit(it->kind == K_LIST, get_num(m.item));
        } else if (m.item == M_OBJ) {
            if (obj_count() == 0u) {
                ui_msg(0, "----", MSG_MS);
                buzzer_error();
            } else {
                m.level = L_OBJSEL;
                m.objidx = 0;
            }
        } else if (m.item == M_TEST) {
            m.level = L_TEST;
            m.t_test = ui_now();
            m.test_sw = 0;
        } else if (m.item == M_PC) {
            cfgsrv_enter_config();
            return MENU_EXIT;
        } else if (m.item == M_DEF) {
            if (m.sure_until && (int32_t)(m.sure_until - ui_now()) > 0) {
                cfg_factory();
                m.sure_until = 0;
                buzzer_ok();
                ui_msg(1, "donE", MSG_MS);
            } else {
                m.sure_until = ui_now() + 3000u;
            }
        }
    }
    return MENU_STAY;
}

static menu_res_t key_edit(const key_event_t *ev)
{
    const item_t *it = &items[m.item];
    uint8_t kc = keys_logical(ev->sw);
    int32_t lo, hi;
    range_of(m.item, &lo, &hi);

    if (it->kind == K_LIST) {
        if (ev->type != KEV_PRESS)
            return MENU_STAY;
        if (kc == KC_UP)
            m.listv = (uint8_t)(m.listv ? m.listv - 1u : (uint8_t)hi);
        else if (kc == KC_DOWN)
            m.listv = (uint8_t)(m.listv >= (uint8_t)hi ? 0u : m.listv + 1u);
        else if (kc == KC_ENT) {
            set_num(m.item, m.listv);
            m.level = L_LIST;
            buzzer_ok();
        } else if (kc == KC_ESC)
            m.level = L_LIST;
        return MENU_STAY;
    }
    /* số */
    if (ev->type == KEV_RELEASE) {
        if (kc == KC_ESC && !m.long_done) {
            if (m.dig == 0u) {
                m.level = L_LIST;
            } else {
                m.acc /= 10;
                m.dig--;
            }
        }
        m.long_done = false;
        return MENU_STAY;
    }
    int8_t d = kc_digit(kc);
    if (d >= 0) {
        type_digit((uint8_t)d, 4);
    } else if (kc == KC_ENT) {
        if (m.dig == 0u) {
            m.level = L_LIST;
            return MENU_STAY;
        }
        if (m.acc < lo || m.acc > hi) {
            ui_msg(1, m.acc < lo ? "E-Lo" : "E-Hi", MSG_MS);
            buzzer_error();
            return MENU_STAY;
        }
        set_num(m.item, m.acc);
        m.level = L_LIST;
        buzzer_ok();
    }
    return MENU_STAY;
}

static menu_res_t key_objsel(const key_event_t *ev)
{
    uint8_t kc = keys_logical(ev->sw);
    if (ev->type != KEV_PRESS)
        return MENU_STAY;
    uint8_t n = obj_count();
    if (kc == KC_UP)
        m.objidx = (uint8_t)(m.objidx ? m.objidx - 1u : n - 1u);
    else if (kc == KC_DOWN)
        m.objidx = (uint8_t)((m.objidx + 1u) % n);
    else if (kc == KC_ESC)
        m.level = L_LIST;
    else if (kc == KC_ENT) {
        m.level = L_OBJSUB;
        m.sub = 0;
    }
    return MENU_STAY;
}

static menu_res_t key_objsub(const key_event_t *ev)
{
    uint8_t kc = keys_logical(ev->sw);
    if (ev->type != KEV_PRESS)
        return MENU_STAY;
    if (kc == KC_UP)
        m.sub = (uint8_t)(m.sub ? m.sub - 1u : OBJ_COUNT - 1u);
    else if (kc == KC_DOWN)
        m.sub = (uint8_t)((m.sub + 1u) % OBJ_COUNT);
    else if (kc == KC_ESC)
        m.level = L_OBJSEL;
    else if (kc == KC_ENT) {
        m.level = L_OBJNUM;
        m.acc = 0;
        m.dig = 0;
        m.neg = false;
        m.long_done = false;
    }
    return MENU_STAY;
}

static menu_res_t key_objnum(const key_event_t *ev)
{
    uint8_t kc = keys_logical(ev->sw);
    if (ev->type == KEV_RELEASE) {
        if (kc == KC_ESC && !m.long_done) {
            if (m.dig == 0u)
                m.level = L_OBJSUB;
            else {
                m.acc /= 10;
                m.dig--;
                if (!m.dig)
                    m.neg = false;
            }
        }
        m.long_done = false;
        return MENU_STAY;
    }
    int8_t d = kc_digit(kc);
    if (d >= 0) {
        type_digit((uint8_t)d, 8);
    } else if (kc == KC_ENT) {
        if (m.dig == 0u) {
            m.level = L_OBJSUB;
            return MENU_STAY;
        }
        int32_t v = m.neg ? -m.acc : m.acc;
        if (m.sub == OBJ_ADR && (v < 0 || v > 65535)) {
            ui_msg(1, "E-Hi", MSG_MS);
            buzzer_error();
            return MENU_STAY;
        }
        obj_set(m.sub, v);
        m.level = L_OBJSUB;
        buzzer_ok();
    }
    return MENU_STAY;
}

static menu_res_t key_test(const key_event_t *ev)
{
    if (ev->type == KEV_PRESS) {
        m.test_sw = ev->sw;
        buzzer_click();
    } else {
        m.long_done = false;
    }
    return MENU_STAY;
}

menu_res_t menu_key(const key_event_t *ev, uint32_t now)
{
    (void)now;
    switch (m.level) {
    case L_LIST:   return key_list(ev);
    case L_EDIT:   return key_edit(ev);
    case L_OBJSEL: return key_objsel(ev);
    case L_OBJSUB: return key_objsub(ev);
    case L_OBJNUM: return key_objnum(ev);
    default:       return key_test(ev);
    }
}

menu_res_t menu_poll(uint32_t now)
{
    uint8_t sw = keys_active();
    if (!sw)
        return MENU_STAY;
    uint8_t kc = keys_logical(sw);
    uint32_t held = keys_held_ms(now);
    if (m.level == L_TEST && kc == KC_ESC && held >= KEY_LONG_MS && !m.long_done) {
        m.long_done = true;
        m.level = L_LIST;
    } else if ((m.level == L_EDIT || m.level == L_OBJNUM) && held >= KEY_LONG_MS && kc == KC_ESC && !m.long_done) {
        m.long_done = true;
        m.level = (m.level == L_EDIT) ? L_LIST : L_OBJSUB;
    } else if (m.level == L_OBJNUM && kc == KC_0 && held >= KEY_ZERO_LONG_MS && !m.long_done && m.sub != OBJ_ADR) {
        m.long_done = true;
        m.neg = !m.neg;
        if (!m.dig)
            m.dig = 1;
    }
    return MENU_STAY;
}

/* ------------------------------------------------------------------ vẽ */
static void show_value(uint8_t id, int32_t v)
{
    switch (id) {
    case M_NODE: display_text(1, node_txt[v & 1]); break;
    case M_BAUD: display_text(1, baud_txt[v > 6 ? 2 : v]); break;
    case M_PAR:  display_text(1, par_txt[v > 2 ? 0 : v]); break;
    case M_ORD:  display_text(1, ord_txt[v > 3 ? 0 : v]); break;
    default:     display_number(1, v, 0, 0, 4); break;
    }
}

void menu_draw(uint32_t now)
{
    display_clear();
    display_leds(0, 0);
    switch (m.level) {
    case L_LIST: {
        const item_t *it = &items[m.item];
        display_text(0, it->name);
        if (it->kind == K_LIST || it->kind == K_NUM)
            show_value(m.item, get_num(m.item));
        else if (m.item == M_VER)
            display_number(1, (int32_t)(FW_VER_MAJOR * 100u + FW_VER_MINOR * 10u + FW_VER_PATCH), 2, 0, 4);
        else if (m.item == M_DEF && m.sure_until && (int32_t)(m.sure_until - now) > 0)
            display_str(1, "SurE");
        else
            display_str(1, "    ");
        break;
    }
    case L_EDIT: {
        const item_t *it = &items[m.item];
        display_text(0, it->name);
        if (it->kind == K_LIST)
            show_value(m.item, m.listv);
        else
            display_number(1, m.acc, 0, 0, 4);
        display_row_blink(1, true);
        break;
    }
    case L_OBJSEL:
        display_str(0, "obj ");
        display_number(1, (int32_t)(m.objidx + 1u), 0, 0, 4);
        break;
    case L_OBJSUB: {
        static const uint8_t lbl[OBJ_COUNT][4] = { { 'A', 'd', 'r', ' ' }, { 'L', 'o', ' ', ' ' }, { 'H', 'i', ' ', ' ' } };
        display_text(0, lbl[m.sub]);
        display_number(1, (int32_t)(m.objidx + 1u), 0, 0, 4);
        break;
    }
    case L_OBJNUM:
        display_number(0, m.neg ? -m.acc : m.acc, 0, 0, 8);
        display_row_blink(0, true);
        display_row_blink(1, true);
        break;
    case L_TEST: {
        uint32_t t = now - m.t_test;
        if (m.test_sw) {
            display_number(0, m.test_sw, 0, DISP_FLAG_ZEROS, 4);            /* số công tắc SWn */
            display_number(1, keys_logical(m.test_sw), 0, DISP_FLAG_ZEROS, 4);  /* mã phím logic */
        } else {
            for (uint8_t d = 0; d < 8u; d++)
                display_raw(d, 0xFFu);
        }
        display_leds((uint8_t)(1u << ((t / 200u) % 8u)), 0);
        break;
    }
    }
}
