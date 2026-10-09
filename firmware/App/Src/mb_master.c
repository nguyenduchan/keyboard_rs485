#include "mb_master.h"
#include "mb_slave.h"       /* MB_MAX_ADU */
#include "rs485.h"
#include "crc.h"
#include "config.h"
#include <string.h>

typedef enum { S_IDLE, S_TX, S_WAIT } mstate_t;

static uint8_t  station;
static uint16_t timeout_ms;
static uint8_t  max_retries;

static mbm_req_t urgent[MB_URGENT_DEPTH];
static uint8_t   u_head, u_count;
static mbm_req_t bg;
static bool      bg_used;

static mbm_req_t cur;
static mstate_t  state;
static uint8_t   tries;
static uint32_t  t_tx;
static bool      comm_ok;
static mbm_stats_t stats;

void mbm_init(uint8_t st, uint16_t tmo, uint8_t retries)
{
    station = st;
    timeout_ms = tmo;
    max_retries = retries;
    u_head = u_count = 0;
    bg_used = false;
    state = S_IDLE;
    comm_ok = true;
    memset(&stats, 0, sizeof stats);
    rs485_rx_release();
}

bool mbm_submit_urgent(const mbm_req_t *r)
{
    if (u_count >= MB_URGENT_DEPTH)
        return false;
    urgent[(u_head + u_count) % MB_URGENT_DEPTH] = *r;
    u_count++;
    return true;
}

bool mbm_submit_bg(const mbm_req_t *r)
{
    if (bg_used)
        return false;
    bg = *r;
    bg_used = true;
    return true;
}

bool mbm_bg_free(void)
{
    return !bg_used;
}

bool mbm_idle(void)
{
    return state == S_IDLE && u_count == 0 && !bg_used;
}

bool mbm_comm_ok(void)
{
    return comm_ok;
}

const mbm_stats_t *mbm_stats(void)
{
    return &stats;
}

static void put16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

static void start_tx(void)
{
    uint8_t *b = rs485_txbuf();
    uint16_t n = 0;
    b[n++] = station;
    b[n++] = cur.fn;
    put16(&b[n], cur.addr);
    n += 2;
    switch (cur.fn) {
    case 5:
        put16(&b[n], cur.qty ? 0xFF00u : 0x0000u);
        n += 2;
        break;
    case 6:
        put16(&b[n], cur.data[0]);
        n += 2;
        break;
    case 16: {
        uint8_t q = (uint8_t)(cur.qty > 2u ? 2u : cur.qty);
        put16(&b[n], q);
        n += 2;
        b[n++] = (uint8_t)(q * 2u);
        for (uint8_t i = 0; i < q; i++) {
            put16(&b[n], cur.data[i]);
            n += 2;
        }
        break;
    }
    default:
        put16(&b[n], cur.qty);
        n += 2;
        break;
    }
    uint16_t crc = crc16_modbus(b, n);
    b[n++] = (uint8_t)crc;
    b[n++] = (uint8_t)(crc >> 8);
    rs485_rx_release();
    rs485_send(n);
    state = S_TX;
}

static void finish(mbm_status_t st, uint8_t exc, const uint8_t *data, uint8_t dlen)
{
    mbm_req_t done = cur;
    state = S_IDLE;
    if (st == MBM_OK) {
        stats.ok++;
        comm_ok = true;
    } else if (st == MBM_EXC) {
        stats.exceptions++;
        stats.last_err = st;
        stats.last_exc = exc;
        comm_ok = true;                 /* thiết bị có trả lời */
    } else {
        if (st == MBM_TIMEOUT)
            stats.timeouts++;
        else
            stats.crc_err++;
        stats.last_err = st;
        comm_ok = false;
    }
    if (done.cb)
        done.cb(done.ctx, st, exc, data, dlen);
}

static void retry_or_fail(mbm_status_t st)
{
    if (tries < max_retries) {
        tries++;
        start_tx();
    } else {
        finish(st, 0, 0, 0);
    }
}

static void on_frame(void)
{
    uint16_t len;
    bool bad;
    const uint8_t *f = rs485_rx(&len, &bad);
    if (bad || len < 5u) {
        rs485_rx_release();
        retry_or_fail(MBM_FRAME);
        return;
    }
    uint16_t crc = (uint16_t)(f[len - 2u] | (f[len - 1u] << 8));
    if (crc16_modbus(f, (size_t)len - 2u) != crc) {
        rs485_rx_release();
        retry_or_fail(MBM_CRC);
        return;
    }
    if (f[0] != station) {                          /* khung của trạm khác: coi như nhiễu */
        rs485_rx_release();
        retry_or_fail(MBM_FRAME);
        return;
    }
    if (f[1] == (uint8_t)(cur.fn | 0x80u) && len == 5u) {
        uint8_t exc = f[2];
        rs485_rx_release();
        finish(MBM_EXC, exc, 0, 0);
        return;
    }
    if (f[1] != cur.fn) {
        rs485_rx_release();
        retry_or_fail(MBM_FRAME);
        return;
    }
    switch (cur.fn) {
    case 1: case 2: case 3: case 4: {
        uint8_t n = f[2];
        uint16_t expect = (cur.fn <= 2u) ? (uint16_t)((cur.qty + 7u) / 8u) : (uint16_t)(cur.qty * 2u);
        if (n != expect || (uint16_t)(3u + n + 2u) != len) {
            rs485_rx_release();
            retry_or_fail(MBM_FRAME);
            return;
        }
        /* dữ liệu nằm trong bộ đệm nhận: callback phải dùng ngay, trước khi nhả bộ đệm */
        finish(MBM_OK, 0, &f[3], n);
        rs485_rx_release();
        return;
    }
    default:                                        /* ghi: phản hồi = echo 6 byte + CRC */
        if (len != 8u) {
            rs485_rx_release();
            retry_or_fail(MBM_FRAME);
            return;
        }
        rs485_rx_release();
        finish(MBM_OK, 0, 0, 0);
        return;
    }
}

void mbm_poll(uint32_t now)
{
    switch (state) {
    case S_IDLE:
        if (u_count) {
            cur = urgent[u_head];
            u_head = (uint8_t)((u_head + 1u) % MB_URGENT_DEPTH);
            u_count--;
        } else if (bg_used) {
            cur = bg;
            bg_used = false;
        } else {
            return;
        }
        tries = 0;
        start_tx();
        break;
    case S_TX:
        if (!rs485_tx_busy()) {
            t_tx = now;
            state = S_WAIT;
        }
        break;
    case S_WAIT:
        if (rs485_rx_ready())
            on_frame();
        else if ((uint32_t)(now - t_tx) >= timeout_ms)
            retry_or_fail(MBM_TIMEOUT);
        break;
    }
}
