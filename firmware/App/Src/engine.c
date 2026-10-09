#include "engine.h"
#include "mb_master.h"
#include "datafmt.h"
#include "cfg.h"
#include "buzzer.h"
#include "hwif.h"
#include "config.h"
#include <string.h>
#include <stdint.h>

#define MAX_ITEMS   32u
#define MAX_JOBS    4u

/* ------------------------------------------------------------------ mục hỏi vòng */
typedef struct {
    uint8_t  area;
    uint8_t  n;             /* số thanh ghi (1/2) hoặc 1 bit */
    uint8_t  valid;
    uint16_t addr;
    uint16_t w[2];
} item_t;

static item_t items[MAX_ITEMS];
static uint8_t n_items;
static uint8_t page_cur;
static uint8_t cursor;
static bool    cycle_wait;
static uint32_t next_cycle;
static uint32_t cur_now;
static bool     inflight;       /* có một lệnh hỏi vòng đang chạy */
static uint8_t  gen;            /* tăng mỗi lần dựng lại danh sách: bỏ kết quả của nhóm đã lỗi thời */
static uint8_t  cycle_err;
static bool     cycle_co;       /* trong chu kỳ này có giao dịch mất liên lạc */

/* nhóm đang bay */
static struct { uint8_t first, last; uint8_t area; uint16_t base, qty; } grp;

static bool is_bit_area(uint8_t a)
{
    return a == RT_AREA_COIL || a == RT_AREA_DISC;
}

static int8_t item_find(uint8_t area, uint16_t addr)
{
    for (uint8_t i = 0; i < n_items; i++)
        if (items[i].area == area && items[i].addr == addr)
            return (int8_t)i;
    return -1;
}

static void item_add(item_t *tmp, uint8_t *cnt, uint8_t area, uint16_t addr, uint8_t n)
{
    for (uint8_t i = 0; i < *cnt; i++)
        if (tmp[i].area == area && tmp[i].addr == addr) {
            if (n > tmp[i].n)
                tmp[i].n = n;
            return;
        }
    if (*cnt >= MAX_ITEMS)
        return;
    tmp[*cnt] = (item_t){ area, n, 0, addr, { 0, 0 } };
    (*cnt)++;
}

static void add_obj(item_t *tmp, uint8_t *cnt, const rt_obj_t *o)
{
    if (o->type != RT_OBJ_SHOW && o->type != RT_OBJ_INPUT && o->type != RT_OBJ_BIT)
        return;
    item_add(tmp, cnt, o->area, o->addr, rt_nregs(o));
}

static void items_build(void)
{
    const rt_blob_t *c = cfg();
    item_t tmp[MAX_ITEMS];
    uint8_t cnt = 0;

    if (page_cur < c->comm.page_count) {
        add_obj(tmp, &cnt, &c->page[page_cur].top);
        if (!(c->page[page_cur].top.flags & RT_OF_WIDE))
            add_obj(tmp, &cnt, &c->page[page_cur].bot);
    }
    for (uint8_t i = 0; i < RT_MAX_LEDS; i++)
        if (c->led[i].mode == RT_LM_STEADY || c->led[i].mode == RT_LM_BLINK)
            item_add(tmp, &cnt, c->led[i].area, c->led[i].addr, 1);
    for (uint8_t i = 0; i < RT_MAX_ALARMS; i++)
        if (c->alarm[i].bit != RT_BIT_NONE || c->alarm[i].addr != 0u || c->alarm[i].flags != 0u)
            item_add(tmp, &cnt, c->alarm[i].area, c->alarm[i].addr, 1);

    /* giữ lại giá trị cũ nếu cùng địa chỉ */
    for (uint8_t i = 0; i < cnt; i++) {
        int8_t j = item_find(tmp[i].area, tmp[i].addr);
        if (j >= 0 && items[j].n >= tmp[i].n) {
            tmp[i].valid = items[j].valid;
            tmp[i].w[0] = items[j].w[0];
            tmp[i].w[1] = items[j].w[1];
        }
    }
    /* sắp xếp theo (vùng, địa chỉ) */
    for (uint8_t i = 1; i < cnt; i++) {
        item_t k = tmp[i];
        int8_t j = (int8_t)(i - 1u);
        while (j >= 0 && (tmp[j].area > k.area || (tmp[j].area == k.area && tmp[j].addr > k.addr))) {
            tmp[j + 1] = tmp[j];
            j--;
        }
        tmp[j + 1] = k;
    }
    memcpy(items, tmp, sizeof(item_t) * cnt);
    n_items = cnt;
    cursor = 0;
    gen++;
}

/* ------------------------------------------------------------------ lỗi */
static uint8_t err_now;

const char *err_text(uint8_t code)
{
    static const char *const t[ERR_CODES] = { "    ", "E-CO", "E-Ad", "E-Fn", "E-dA", "E-Hi", "E-Lo", "E-bU", "E-cF", "E-PF" };
    return code < ERR_CODES ? t[code] : "E-??";
}

static uint8_t map_err(mbm_status_t st, uint8_t exc)
{
    if (st == MBM_EXC)
        return exc == 2u ? ERR_AD : (exc == 3u ? ERR_DA : ERR_FN);
    return ERR_CO;
}

/* ------------------------------------------------------------------ hỏi vòng */
static uint8_t fn_for(uint8_t area)
{
    switch (area) {
    case RT_AREA_HOLD:  return 3;
    case RT_AREA_INPUT: return 4;
    case RT_AREA_COIL:  return 1;
    default:            return 2;
    }
}

static uint16_t be16(const uint8_t *p)
{
    return (uint16_t)((p[0] << 8) | p[1]);
}

static bool comm_flip;          /* đổi trạng thái mỗi giao dịch có phản hồi (LED COMM) */

/* lỗi mất liên lạc hiện ngay; lỗi exception chỉ chốt khi hết một chu kỳ hỏi (tránh nhấp nháy giữa các nhóm) */
static void note_result(mbm_status_t st, uint8_t exc, bool poll)
{
    if (st == MBM_OK || st == MBM_EXC)
        comm_flip = !comm_flip;
    if (st == MBM_OK)
        return;
    uint8_t e = map_err(st, exc);
    if (e == ERR_CO) {
        err_now = ERR_CO;
        cycle_co = true;
    } else if (poll) {
        cycle_err = e;
    }
}

static void poll_cb(void *ctx, mbm_status_t st, uint8_t exc, const uint8_t *data, uint8_t dlen)
{
    inflight = false;
    if ((uint8_t)(uintptr_t)ctx != gen)
        return;                         /* danh sách đã đổi trong lúc chờ */
    note_result(st, exc, true);
    if (st == MBM_OK) {
        for (uint8_t i = grp.first; i < grp.last; i++) {
            item_t *it = &items[i];
            uint16_t off = (uint16_t)(it->addr - grp.base);
            if (is_bit_area(grp.area)) {
                if ((uint16_t)(off / 8u) < dlen) {
                    it->w[0] = (data[off / 8u] >> (off % 8u)) & 1u;
                    it->valid = 1;
                }
            } else if ((uint16_t)((off + it->n) * 2u) <= dlen) {
                it->w[0] = be16(&data[off * 2u]);
                it->w[1] = it->n > 1u ? be16(&data[off * 2u + 2u]) : 0u;
                it->valid = 1;
            }
        }
    }
    /* thành công hay lỗi đều sang nhóm kế tiếp để không kẹt ở một nhóm hỏng */
    cursor = grp.last;
    if (cursor >= n_items) {
        cursor = 0;
        err_now = cycle_co ? ERR_CO : cycle_err;
        cycle_err = ERR_NONE;
        cycle_co = false;
        cycle_wait = true;
        next_cycle = cur_now + (err_now == ERR_CO ? POLL_ERR_MS : cfg()->comm.poll_ms);
    }
}

static void schedule_poll(void)
{
    if (n_items == 0u || inflight || !mbm_bg_free() || cycle_wait)
        return;
    uint8_t first = cursor;
    uint8_t area = items[first].area;
    uint16_t base = items[first].addr;
    uint16_t end = (uint16_t)(base + items[first].n);
    uint16_t limit = is_bit_area(area) ? POLL_MAX_BITS : POLL_MAX_REGS;
    uint8_t last = (uint8_t)(first + 1u);
    while (last < n_items && items[last].area == area) {
        uint16_t e2 = (uint16_t)(items[last].addr + items[last].n);
        if (items[last].addr > (uint16_t)(end + POLL_MAX_GAP))
            break;
        if ((uint16_t)((e2 > end ? e2 : end) - base) > limit)
            break;
        if (e2 > end)
            end = e2;
        last++;
    }
    grp.first = first;
    grp.last = last;
    grp.area = area;
    grp.base = base;
    grp.qty = (uint16_t)(end - base);
    mbm_req_t r = { 0 };
    r.fn = fn_for(area);
    r.addr = base;
    r.qty = grp.qty;
    r.cb = poll_cb;
    r.ctx = (void *)(uintptr_t)gen;
    inflight = mbm_submit_bg(&r);
}

/* ------------------------------------------------------------------ ghi */
enum { OP_WRITE, OP_SET, OP_CLR, OP_TOGGLE };
enum { PH_WRITE, PH_READ, PH_VERIFY };
enum { KIND_UI, KIND_FKEY };

typedef struct {
    uint8_t  used, kind, phase, area, op, bit, nregs, verify, fk, result, err;
    uint16_t addr;
    uint16_t w[2];
} wjob_t;

static wjob_t jobs[MAX_JOBS];
static uint8_t ui_job = 0xFF;
static uint16_t fk_press_pending, fk_release_pending;     /* mặt nạ theo phím */
static uint16_t fk_held;                                  /* phím giữ đã gửi lệnh nhấn: bắt buộc phải gửi lệnh nhả */

static void job_finish(wjob_t *j, uint8_t ok, uint8_t err)
{
    j->result = ok ? 1u : 2u;
    j->err = err;
    if (j->kind == KIND_FKEY) {
        j->used = 0;
        if (!ok)
            buzzer_error();             /* phím F ghi PLC thất bại: báo cho người dùng biết */
    } else if (ok)
        buzzer_ok();
    else
        buzzer_error();
}

static void cache_store(const wjob_t *j)
{
    int8_t k = item_find(j->area, j->addr);
    if (k < 0)
        return;
    items[k].w[0] = j->w[0];
    if (items[k].n > 1u)
        items[k].w[1] = j->w[1];
    items[k].valid = 1;
}

static void job_cb(void *ctx, mbm_status_t st, uint8_t exc, const uint8_t *data, uint8_t dlen);

static bool job_submit_read(wjob_t *j)
{
    mbm_req_t r = { 0 };
    r.fn = (j->area == RT_AREA_COIL) ? 1u : 3u;
    r.addr = j->addr;
    r.qty = (j->area == RT_AREA_COIL) ? 1u : j->nregs;
    r.cb = job_cb;
    r.ctx = j;
    return mbm_submit_urgent(&r);
}

static bool job_submit_write(wjob_t *j)
{
    mbm_req_t r = { 0 };
    r.addr = j->addr;
    r.cb = job_cb;
    r.ctx = j;
    if (j->area == RT_AREA_COIL) {
        r.fn = 5;
        r.qty = j->w[0] ? 1u : 0u;
    } else if (j->nregs >= 2u) {
        r.fn = 16;
        r.qty = 2;
        r.data[0] = j->w[0];
        r.data[1] = j->w[1];
    } else {
        r.fn = 6;
        r.data[0] = j->w[0];
    }
    return mbm_submit_urgent(&r);
}

static void job_start(wjob_t *j)
{
    bool need_read = (j->op == OP_TOGGLE) || (j->area == RT_AREA_HOLD && j->bit != RT_BIT_NONE && j->op != OP_WRITE);
    bool ok;
    if (need_read) {
        j->phase = PH_READ;
        ok = job_submit_read(j);
    } else {
        if (j->area == RT_AREA_COIL && j->op != OP_WRITE)
            j->w[0] = (j->op == OP_SET) ? 1u : 0u;
        j->phase = PH_WRITE;
        ok = job_submit_write(j);
    }
    if (!ok)
        job_finish(j, 0, ERR_BU);
}

static void job_cb(void *ctx, mbm_status_t st, uint8_t exc, const uint8_t *data, uint8_t dlen)
{
    wjob_t *j = (wjob_t *)ctx;
    note_result(st, exc, false);
    if (st != MBM_OK) {
        job_finish(j, 0, map_err(st, exc));
        return;
    }
    switch (j->phase) {
    case PH_READ: {
        uint8_t nb = (j->area == RT_AREA_COIL) ? 1u : (uint8_t)(j->nregs * 2u);
        if (dlen < nb) {
            job_finish(j, 0, ERR_DA);
            return;
        }
        if (j->area == RT_AREA_COIL) {
            uint8_t old = data[0] & 1u;
            j->w[0] = (j->op == OP_SET) ? 1u : (j->op == OP_CLR ? 0u : (uint16_t)!old);
        } else {
            uint16_t old = be16(data);
            uint16_t m = (uint16_t)(1u << (j->bit & 15u));
            uint16_t nw = (j->op == OP_SET) ? (uint16_t)(old | m)
                        : (j->op == OP_CLR) ? (uint16_t)(old & ~m)
                                            : (uint16_t)(old ^ m);
            j->w[0] = nw;
            j->nregs = 1;
        }
        j->phase = PH_WRITE;
        if (!job_submit_write(j))
            job_finish(j, 0, ERR_BU);
        return;
    }
    case PH_WRITE:
        cache_store(j);
        if (j->verify) {
            j->phase = PH_VERIFY;
            if (!job_submit_read(j))
                job_finish(j, 0, ERR_BU);
            return;
        }
        job_finish(j, 1, ERR_NONE);
        return;
    case PH_VERIFY:
    default: {
        bool same;
        if (j->area == RT_AREA_COIL)
            same = dlen >= 1u && ((data[0] & 1u) == (j->w[0] ? 1u : 0u));
        else if (j->bit != RT_BIT_NONE) {
            uint16_t m = (uint16_t)(1u << (j->bit & 15u));      /* PLC có thể đổi các bit khác: chỉ so bit đã ghi */
            same = dlen >= 2u && ((be16(data) & m) == (j->w[0] & m));
        } else {
            same = dlen >= (uint8_t)(j->nregs * 2u) && be16(data) == j->w[0] &&
                   (j->nregs < 2u || be16(data + 2) == j->w[1]);
        }
        job_finish(j, same, same ? ERR_NONE : ERR_DA);
        return;
    }
    }
}

static wjob_t *job_alloc(uint8_t kind)
{
    for (uint8_t i = 0; i < MAX_JOBS; i++)
        if (!jobs[i].used) {
            memset(&jobs[i], 0, sizeof jobs[i]);
            jobs[i].used = 1;
            jobs[i].kind = kind;
            return &jobs[i];
        }
    return 0;
}

bool engine_write_obj(const rt_obj_t *o, int32_t disp)
{
    if (ui_job != 0xFFu)
        return false;
    wjob_t *j = job_alloc(KIND_UI);
    if (!j)
        return false;
    ui_job = (uint8_t)(j - jobs);
    j->area = o->area;
    j->addr = o->addr;
    j->bit = o->bit;
    j->nregs = rt_nregs(o);
    j->verify = ((o->flags & RT_OF_VERIFY) || (cfg()->comm.flags & RT_CF_VERIFY)) ? 1u : 0u;
    j->op = OP_WRITE;
    if (!hw_power_ok()) {
        job_finish(j, 0, ERR_PF);
        return true;
    }
    if (o->area == RT_AREA_COIL) {
        j->w[0] = disp ? 1u : 0u;
    } else if (o->bit != RT_BIT_NONE) {
        j->op = disp ? OP_SET : OP_CLR;
    } else if (!df_encode(o, cfg()->comm.order, disp, j->w)) {
        job_finish(j, 0, ERR_DA);
        return true;
    }
    job_start(j);
    return true;
}

wstate_t engine_write_state(uint8_t *err)
{
    if (ui_job == 0xFFu)
        return W_IDLE;
    wjob_t *j = &jobs[ui_job];
    if (j->result == 0u)
        return W_BUSY;
    if (err)
        *err = j->err;
    return j->result == 1u ? W_OK : W_ERR;
}

void engine_write_done(void)
{
    if (ui_job != 0xFFu) {
        jobs[ui_job].used = 0;
        ui_job = 0xFF;
    }
}

/* ------------------------------------------------------------------ phím chức năng */
void engine_fkey(uint8_t idx, bool pressed)
{
    if (idx >= RT_MAX_FKEYS)
        return;
    const rt_fkey_t *f = &cfg()->fkey[idx];
    if (f->action == RT_FA_NONE || f->action >= RT_FA_GOTO)
        return;
    uint16_t m = (uint16_t)(1u << idx);
    if (pressed) {
        fk_press_pending |= m;
        if (f->action == RT_FA_MOMENTARY)
            fk_held |= m;
    } else if (f->action == RT_FA_MOMENTARY && (fk_held & m)) {
        /* chỉ nhả khi đã nhấn; không phụ thuộc quyền truy cập hay trang hiện tại */
        fk_held &= (uint16_t)~m;
        fk_release_pending |= m;
    }
}

static void fkey_run(uint8_t idx, bool pressed)
{
    const rt_fkey_t *f = &cfg()->fkey[idx];
    wjob_t *j = job_alloc(KIND_FKEY);
    if (!j)
        return;
    j->fk = idx;
    j->area = (f->area == RT_FAREA_COIL) ? RT_AREA_COIL : RT_AREA_HOLD;
    j->addr = f->addr;
    j->bit = (f->area == RT_FAREA_HOLDBIT) ? f->bit : RT_BIT_NONE;
    j->nregs = 1;
    j->verify = (cfg()->comm.flags & RT_CF_VERIFY) ? 1u : 0u;
    switch (f->action) {
    case RT_FA_MOMENTARY: j->op = pressed ? OP_SET : OP_CLR; break;
    case RT_FA_TOGGLE:    j->op = OP_TOGGLE; break;
    case RT_FA_SET:       j->op = OP_SET; break;
    case RT_FA_RESET:     j->op = OP_CLR; break;
    default:              /* RT_FA_WRITE */
        if (f->area == RT_FAREA_HOLDWORD) {
            j->op = OP_WRITE;
            j->w[0] = f->value;
        } else {
            j->op = f->value ? OP_SET : OP_CLR;
        }
        break;
    }
    job_start(j);
}

static bool fkey_job_busy(void)
{
    for (uint8_t k = 0; k < MAX_JOBS; k++)
        if (jobs[k].used && jobs[k].kind == KIND_FKEY)
            return true;
    return false;
}

/* Mỗi lần chỉ một lệnh phím F chạy (đọc-sửa-ghi không chồng nhau, nhấn rồi nhả đúng thứ tự). */
static void fkeys_service(void)
{
    if (!hw_power_ok()) {
        fk_press_pending = fk_release_pending = 0;
        return;
    }
    if (fkey_job_busy())
        return;
    for (uint8_t i = 0; i < RT_MAX_FKEYS; i++) {
        uint16_t m = (uint16_t)(1u << i);
        if ((fk_release_pending & m) && !(fk_press_pending & m)) {
            fk_release_pending &= (uint16_t)~m;
            fkey_run(i, false);
            return;
        }
        if (fk_press_pending & m) {
            fk_press_pending &= (uint16_t)~m;
            fkey_run(i, true);
            return;
        }
    }
}

/* ------------------------------------------------------------------ LED & cảnh báo */
static uint8_t  led_on_mask, led_blink_mask;
static uint16_t al_active, al_acked, al_prev;

static bool item_bit(uint8_t area, uint16_t addr, uint8_t bit, bool *val)
{
    int8_t k = item_find(area, addr);
    if (k < 0 || !items[k].valid)
        return false;
    if (is_bit_area(area))
        *val = items[k].w[0] & 1u;
    else if (bit != RT_BIT_NONE)
        *val = (items[k].w[0] >> (bit & 15u)) & 1u;
    else
        *val = (items[k].w[0] | items[k].w[1]) != 0u;
    return true;
}

static void leds_alarms_update(void)
{
    const rt_blob_t *c = cfg();
    uint8_t on = 0, blink = 0;

    for (uint8_t i = 0; i < RT_MAX_LEDS; i++) {
        const rt_led_t *l = &c->led[i];
        bool v;
        if (l->mode == RT_LM_COMM) {
            if (err_now != ERR_CO && comm_flip)
                on |= (uint8_t)(1u << i);
        } else if (l->mode != RT_LM_OFF && item_bit(l->area, l->addr, l->bit, &v)) {
            if (l->flags & RT_LF_INVERT)
                v = !v;
            if (v) {
                if (l->mode == RT_LM_BLINK)
                    blink |= (uint8_t)(1u << i);
                else
                    on |= (uint8_t)(1u << i);
            }
        }
    }

    uint16_t act = 0;
    for (uint8_t i = 0; i < RT_MAX_ALARMS; i++) {
        const rt_alarm_t *a = &c->alarm[i];
        bool v;
        if (a->bit == RT_BIT_NONE && a->addr == 0u && a->flags == 0u)
            continue;
        if (item_bit(a->area, a->addr, a->bit, &v) && v)
            act |= (uint16_t)(1u << i);
    }
    al_acked &= act;                    /* hết báo thì quên xác nhận */
    al_prev = act;
    al_active = act;

    bool beep = false;
    for (uint8_t i = 0; i < RT_MAX_ALARMS; i++) {
        if (!(act & (1u << i)))
            continue;
        const rt_alarm_t *a = &c->alarm[i];
        bool acked = (a->flags & RT_AF_ACK) && (al_acked & (1u << i));
        if (a->led && a->led <= RT_MAX_LEDS && !acked) {
            if (a->flags & RT_AF_LEDBLINK)
                blink |= (uint8_t)(1u << (a->led - 1u));
            else
                on |= (uint8_t)(1u << (a->led - 1u));
        }
        if ((a->flags & RT_AF_BEEP) && !acked)
            beep = true;
    }
    buzzer_alarm(beep);
    led_on_mask = on;
    led_blink_mask = blink;
}

uint16_t engine_alarms_active(void)
{
    return al_active;
}

uint16_t engine_alarms_pending(void)
{
    uint16_t p = 0;
    const rt_blob_t *c = cfg();
    for (uint8_t i = 0; i < RT_MAX_ALARMS; i++) {
        uint16_t m = (uint16_t)(1u << i);
        if (!(al_active & m))
            continue;
        if ((c->alarm[i].flags & RT_AF_ACK) ? !(al_acked & m) : 1)
            p |= m;
    }
    return p;
}

bool engine_alarm_ack(void)
{
    const rt_blob_t *c = cfg();
    bool any = false;
    for (uint8_t i = 0; i < RT_MAX_ALARMS; i++) {
        uint16_t m = (uint16_t)(1u << i);
        const rt_alarm_t *a = &c->alarm[i];
        if (!(al_active & m) || !(a->flags & RT_AF_ACK) || (al_acked & m))
            continue;
        wjob_t *j = job_alloc(KIND_FKEY);
        if (!j)
            break;
        j->area = RT_AREA_COIL;
        j->addr = a->ack_addr;
        j->bit = RT_BIT_NONE;
        j->nregs = 1;
        j->op = OP_SET;
        job_start(j);
        al_acked |= m;
        any = true;
    }
    return any;
}

uint8_t engine_led_on(void)
{
    return led_on_mask;
}

uint8_t engine_led_blink(void)
{
    return led_blink_mask;
}

/* ------------------------------------------------------------------ dữ liệu / điều khiển chung */
bool engine_obj_value(const rt_obj_t *o, int32_t *disp)
{
    int8_t k = item_find(o->area, o->addr);
    if (k < 0 || !items[k].valid)
        return false;
    return df_decode(o, cfg()->comm.order, items[k].w, disp);
}

bool engine_comm_ok(void)
{
    return err_now != ERR_CO;
}

uint8_t engine_error(void)
{
    return err_now;
}

uint8_t engine_item_count(void)
{
    return n_items;
}

void engine_init(void)
{
    memset(items, 0, sizeof items);
    memset(jobs, 0, sizeof jobs);
    n_items = 0;
    cursor = 0;
    cycle_wait = false;
    inflight = false;
    err_now = ERR_NONE;
    cycle_err = ERR_NONE;
    cycle_co = false;
    ui_job = 0xFF;
    fk_press_pending = fk_release_pending = fk_held = 0;
    al_active = al_acked = al_prev = 0;
    led_on_mask = led_blink_mask = 0;
    comm_flip = false;
    page_cur = cfg()->comm.run_page;
    items_build();
}

void engine_set_page(uint8_t page)
{
    page_cur = page;
    items_build();
    cycle_wait = false;
}

void engine_poll(uint32_t now)
{
    cur_now = now;
    if (cycle_wait && (int32_t)(now - next_cycle) >= 0)
        cycle_wait = false;
    if (hw_power_ok())
        schedule_poll();
    fkeys_service();
    leds_alarms_update();
}
