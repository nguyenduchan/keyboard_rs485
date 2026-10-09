#include "rtcfg.h"
#include "crc.h"
#include <string.h>

static const uint32_t baud_tab[7] = { 2400u, 4800u, 9600u, 19200u, 38400u, 57600u, 115200u };

uint32_t rtcfg_baud(uint8_t idx)
{
    return baud_tab[idx > 6u ? 2u : idx];
}

uint32_t rtcfg_crc(const rt_blob_t *b)
{
    return crc32_update(0, (const uint8_t *)b, sizeof(rt_blob_t) - sizeof(uint32_t));
}

void rtcfg_seal(rt_blob_t *b)
{
    b->magic = RT_MAGIC;
    b->schema = RT_SCHEMA;
    b->length = RT_BLOB_SIZE;
    b->crc32 = rtcfg_crc(b);
}

void rtcfg_defaults(rt_blob_t *b)
{
    memset(b, 0, sizeof(*b));
    b->comm.mode = RT_MODE_SLAVE;           /* thiết bị chưa cấu hình không bao giờ ghi vào PLC */
    b->comm.plc_id = 1;
    b->comm.kbd_id = 1;
    b->comm.baud_idx = 2;
    b->comm.stop = 1;
    b->comm.order = RT_ORD_ABCD;
    b->comm.timeout_ms = 200;
    b->comm.retries = 2;
    b->comm.poll_ms = 50;
    b->comm.brightness = 5;
    b->comm.beep = RT_BF_CLICK | RT_BF_ERROR | RT_BF_ALARM;
    b->comm.idle_s = 30;
    b->comm.page_count = 1;
    b->page[0].top.type = RT_OBJ_LABEL;
    memcpy(b->page[0].top.label, "PLC ", 4);
    for (uint8_t i = 0; i < RT_MAX_PAGES; i++) {
        b->page[i].top.bit = RT_BIT_NONE;
        b->page[i].bot.bit = RT_BIT_NONE;
    }
    b->led[0].mode = RT_LM_COMM;
    for (uint8_t i = 0; i < RT_MAX_LEDS; i++)
        b->led[i].bit = RT_BIT_NONE;
    for (uint8_t i = 0; i < RT_MAX_ALARMS; i++)
        b->alarm[i].bit = RT_BIT_NONE;
    rtcfg_seal(b);
}

uint8_t rt_nregs(const rt_obj_t *o)
{
    if (o->area == RT_AREA_COIL || o->area == RT_AREA_DISC || o->bit != RT_BIT_NONE)
        return 1;
    switch (o->dtype) {
    case RT_DT_U32: case RT_DT_I32: case RT_DT_BCD32: case RT_DT_F32:
        return 2;
    default:
        return 1;
    }
}

bool rt_is_writable(const rt_obj_t *o)
{
    return o->area == RT_AREA_HOLD || o->area == RT_AREA_COIL;
}

static bool obj_ok(const rt_obj_t *o)
{
    if (o->type > RT_OBJ_BIT)
        return false;
    if (o->type == RT_OBJ_NONE || o->type == RT_OBJ_LABEL)
        return true;
    if (o->area > RT_AREA_DISC || o->dtype >= RT_DT_COUNT || o->order > RT_ORD_DCBA ||
        o->decimals > 3u || o->access > 2u || (o->bit > 15u && o->bit != RT_BIT_NONE))
        return false;
    if (o->type == RT_OBJ_INPUT) {
        if (!rt_is_writable(o) || o->min > o->max)
            return false;
    }
    return true;
}

uint8_t rtcfg_validate(const rt_blob_t *b)
{
    if (b->magic != RT_MAGIC)
        return RT_E_MAGIC;
    if (b->schema != RT_SCHEMA)
        return RT_E_SCHEMA;
    if (b->length != RT_BLOB_SIZE)
        return RT_E_LENGTH;
    if (b->crc32 != rtcfg_crc(b))
        return RT_E_CRC;

    const rt_comm_t *c = &b->comm;
    if (c->mode > RT_MODE_SLAVE || c->plc_id < 1u || c->plc_id > 247u || c->kbd_id < 1u || c->kbd_id > 247u ||
        c->baud_idx > 6u || c->parity > 2u || c->stop < 1u || c->stop > 2u ||
        c->order < RT_ORD_ABCD || c->order > RT_ORD_DCBA || c->timeout_ms < 20u || c->timeout_ms > 5000u ||
        c->retries > 5u || c->poll_ms > 5000u || c->brightness > 7u || c->page_count > RT_MAX_PAGES ||
        (c->page_count != 0u && c->run_page >= c->page_count))
        return RT_E_COMM;
    if (b->sec.pwd_user > PASS_MAX || b->sec.pwd_tech > PASS_MAX)
        return RT_E_SEC;
    for (uint8_t i = 0; i < c->page_count; i++) {
        const rt_page_t *p = &b->page[i];
        if (p->access > 2u || !obj_ok(&p->top) || !obj_ok(&p->bot))
            return RT_E_PAGE;
    }
    for (uint8_t i = 0; i < RT_MAX_FKEYS; i++) {
        const rt_fkey_t *f = &b->fkey[i];
        if (f->action >= RT_FA_COUNT || f->area > RT_FAREA_HOLDWORD || f->access > 2u ||
            (f->area == RT_FAREA_HOLDBIT && f->bit > 15u))
            return RT_E_FKEY;
        if (f->action == RT_FA_GOTO && f->value >= RT_MAX_PAGES)
            return RT_E_FKEY;
        /* Giữ/đảo/đặt/xoá chỉ có nghĩa với bit (cuộn dây hoặc bit trong thanh ghi), không với cả thanh ghi */
        if (f->action >= RT_FA_MOMENTARY && f->action <= RT_FA_RESET && f->area == RT_FAREA_HOLDWORD)
            return RT_E_FKEY;
    }
    for (uint8_t i = 0; i < RT_MAX_LEDS; i++) {
        const rt_led_t *l = &b->led[i];
        if (l->mode > RT_LM_COMM || l->area > RT_AREA_DISC || (l->bit > 15u && l->bit != RT_BIT_NONE))
            return RT_E_LED;
    }
    for (uint8_t i = 0; i < RT_MAX_ALARMS; i++) {
        const rt_alarm_t *a = &b->alarm[i];
        if (a->area > RT_AREA_DISC || (a->bit > 15u && a->bit != RT_BIT_NONE) || a->led > RT_MAX_LEDS)
            return RT_E_ALARM;
    }
    return RT_OK;
}
