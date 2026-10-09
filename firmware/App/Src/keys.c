#include "keys.h"
#include "hwif.h"
#include "config.h"

/*
 * Bố cục mặc định nhìn từ mặt trước (SW13..SW16 hàng trên, SW1..SW4 hàng dưới):
 *     7   8   9  ESC      SW13 SW14 SW15 SW16
 *     4   5   6   ▲       SW9  SW10 SW11 SW12
 *     1   2   3   ▼       SW5  SW6  SW7  SW8
 *     F1  0   F2  ENT     SW1  SW2  SW3  SW4
 * Phím K3 (SW17..SW24, mở rộng) = F3..F10.
 */
static const uint8_t keymap[24] = {
    KC_F1, KC_0, KC_F2, KC_ENT, KC_1, KC_2, KC_3, KC_DOWN,
    KC_4, KC_5, KC_6, KC_UP, KC_7, KC_8, KC_9, KC_ESC,
    KC_F3, KC_F4, KC_F5, KC_F6, KC_F7, KC_F8, KC_F9, KC_F10,
};

#define RING 8u
static key_event_t ring[RING];
static uint8_t r_head, r_count;

static bool ext_keys;
static uint32_t last_scan;
static uint32_t raw_now;
static uint8_t active_sw;
static uint32_t active_since;
static uint8_t cand_sw, cand_cnt, rel_cnt;
static uint16_t press_count;

uint8_t keys_logical(uint8_t sw)
{
    return (sw >= 1u && sw <= 24u) ? keymap[sw - 1u] : (uint8_t)KC_NONE;
}

int8_t kc_digit(uint8_t kc)
{
    if (kc >= KC_1 && kc <= KC_9)
        return (int8_t)kc;
    if (kc == KC_0)
        return 0;
    return -1;
}

/*
 * Dữ liệu phím TM1638 (datasheet / thư viện TM16xx): byte b chứa KS(2b+1) ở nửa thấp và KS(2b+2) ở nửa cao;
 * trong mỗi nửa: bit2 = K1, bit1 = K2, bit0 = K3.
 */
uint32_t keys_decode(const uint8_t k[4], bool ext)
{
    uint32_t raw = 0;
    for (uint8_t ks = 0; ks < 8u; ks++) {
        uint8_t nib = (uint8_t)((k[ks / 2u] >> ((ks % 2u) * 4u)) & 0x0Fu);
        if (nib & 0x04u)
            raw |= 1ul << ks;                   /* K1 -> SW1..8 */
        if (nib & 0x02u)
            raw |= 1ul << (8u + ks);            /* K2 -> SW9..16 */
        if (ext && (nib & 0x01u))
            raw |= 1ul << (16u + ks);           /* K3 -> SW17..24 */
    }
    return raw;
}

static void push(uint8_t type, uint8_t sw, uint32_t t)
{
    if (r_count >= RING)
        return;
    ring[(r_head + r_count) % RING] = (key_event_t){ type, sw, t };
    r_count++;
}

bool keys_pop(key_event_t *ev)
{
    if (!r_count)
        return false;
    *ev = ring[r_head];
    r_head = (uint8_t)((r_head + 1u) % RING);
    r_count--;
    return true;
}

void keys_init(bool ext)
{
    ext_keys = ext;
    r_head = r_count = 0;
    active_sw = cand_sw = 0;
    cand_cnt = rel_cnt = 0;
    raw_now = 0;
    last_scan = 0;
}

static uint8_t popcount24(uint32_t v, uint8_t *idx)
{
    uint8_t n = 0;
    for (uint8_t i = 0; i < 24u; i++)
        if (v & (1ul << i)) {
            n++;
            *idx = (uint8_t)(i + 1u);
        }
    return n;
}

void keys_poll(uint32_t now)
{
    if ((uint32_t)(now - last_scan) < KEY_SCAN_PERIOD_MS)
        return;
    last_scan = now;

    uint8_t k[4];
    if (!hw_tm_read_keys(k))
        return;                                 /* đọc lỗi: giữ nguyên trạng thái */
    raw_now = keys_decode(k, ext_keys);

    if (active_sw) {
        if (raw_now & (1ul << (active_sw - 1u))) {
            rel_cnt = 0;
        } else if (++rel_cnt >= KEY_DEBOUNCE_SAMPLES) {
            push(KEV_RELEASE, active_sw, now);
            active_sw = 0;
            rel_cnt = 0;
            cand_sw = 0;
            cand_cnt = 0;
        }
        return;
    }
    uint8_t idx = 0;
    if (popcount24(raw_now, &idx) == 1u) {
        if (idx == cand_sw) {
            if (++cand_cnt >= KEY_DEBOUNCE_SAMPLES) {
                active_sw = idx;
                active_since = now;
                press_count++;
                push(KEV_PRESS, idx, now);
                rel_cnt = 0;
            }
        } else {
            cand_sw = idx;
            cand_cnt = 1;
        }
    } else {
        cand_sw = 0;
        cand_cnt = 0;
    }
}

uint8_t keys_active(void)
{
    return active_sw;
}

uint32_t keys_held_ms(uint32_t now)
{
    return active_sw ? (uint32_t)(now - active_since) : 0u;
}

uint32_t keys_raw(void)
{
    return raw_now;
}

uint16_t keys_count(void)
{
    return press_count;
}
