#include "datafmt.h"
#include <stdint.h>

static uint16_t swap16(uint16_t v)
{
    return (uint16_t)((v << 8) | (v >> 8));
}

uint32_t df_join32(uint8_t order, uint16_t a, uint16_t b)
{
    switch (order) {
    case RT_ORD_CDAB: return ((uint32_t)b << 16) | a;
    case RT_ORD_BADC: return ((uint32_t)swap16(a) << 16) | swap16(b);
    case RT_ORD_DCBA: return ((uint32_t)swap16(b) << 16) | swap16(a);
    default:          return ((uint32_t)a << 16) | b;       /* ABCD */
    }
}

void df_split32(uint8_t order, uint32_t v, uint16_t *a, uint16_t *b)
{
    uint16_t hi = (uint16_t)(v >> 16), lo = (uint16_t)v;
    switch (order) {
    case RT_ORD_CDAB: *a = lo;         *b = hi;         break;
    case RT_ORD_BADC: *a = swap16(hi); *b = swap16(lo); break;
    case RT_ORD_DCBA: *a = swap16(lo); *b = swap16(hi); break;
    default:          *a = hi;         *b = lo;         break;
    }
}

static const int32_t pow10_tab[4] = { 1, 10, 100, 1000 };

static bool bcd_decode(uint32_t v, uint8_t digits, uint32_t *out)
{
    uint32_t r = 0, mul = 1;
    for (uint8_t i = 0; i < digits; i++) {
        uint32_t n = (v >> (4u * i)) & 0xFu;
        if (n > 9u)
            return false;
        r += n * mul;
        mul *= 10u;
    }
    *out = r;
    return true;
}

static uint32_t bcd_encode(uint32_t v, uint8_t digits)
{
    uint32_t r = 0;
    for (uint8_t i = 0; i < digits; i++) {
        r |= (v % 10u) << (4u * i);
        v /= 10u;
    }
    return r;
}

static int32_t sat32(int64_t v)
{
    if (v > INT32_MAX)
        return INT32_MAX;
    if (v < INT32_MIN)
        return INT32_MIN;
    return (int32_t)v;
}

static int64_t gain_apply(int64_t v, int16_t g)
{
    if (g == 0 || g == 1000)
        return v;
    int64_t p = v * g;
    return (p >= 0 ? p + 500 : p - 500) / 1000;
}

static int64_t gain_undo(int64_t v, int16_t g)
{
    if (g == 0 || g == 1000)
        return v;
    int64_t p = v * 1000;
    return (p >= 0 ? p + g / 2 : p - g / 2) / g;
}

/*
 * float32 <-> số hiển thị chỉ dùng số nguyên (không kéo thư viện float mềm ~3,7 KB vào flash).
 * Kết quả khớp phép làm tròn "half away from zero" của (float)x * 10^dec * gain / 1000.
 */
static bool f32_decode(uint32_t bits, uint8_t decimals, int16_t gain, int32_t *out)
{
    uint32_t e = (bits >> 23) & 0xFFu, m = bits & 0x7FFFFFu;
    bool neg = (bits >> 31) != 0u;
    if (e == 0xFFu)
        return false;                                       /* NaN / vô cực */
    int64_t v = 0;
    if (e != 0u) {
        int64_t gf = (gain == 0) ? 1000 : gain;
        int64_t p = (int64_t)(m | 0x800000u) * pow10_tab[decimals & 3u] * (gf < 0 ? -gf : gf);   /* < 2^49 */
        int shift = (int)e - 150;                           /* giá trị = p * 2^shift / 1000 */
        if (shift > 13)
            v = INT64_MAX / 2;
        else if (shift >= 0)
            v = p << shift;
        else if (-shift >= 62)
            v = 0;
        else
            v = (p + (1LL << (-shift - 1))) >> -shift;
        if (v != INT64_MAX / 2)
            v = (v + 500) / 1000;
        if (gf < 0)
            neg = !neg;
    }
    if (v > INT32_MAX)
        v = INT32_MAX;
    *out = neg ? (int32_t)-v : (int32_t)v;
    return true;
}

static uint8_t msb64(uint64_t v)
{
    uint8_t n = 0;
    while (v >>= 1)
        n++;
    return n;
}

static uint32_t f32_encode(int32_t disp, uint8_t decimals, int16_t gain)
{
    if (disp == 0)
        return 0;
    bool neg = disp < 0;
    int64_t gf = (gain == 0) ? 1000 : gain;
    if (gf < 0) {
        gf = -gf;
        neg = !neg;
    }
    uint64_t n = (uint64_t)((disp < 0) ? -(int64_t)disp : (int64_t)disp) * 1000u;   /* < 2^41 */
    uint64_t d = (uint64_t)gf * (uint64_t)pow10_tab[decimals & 3u];                /* < 2^25 */
    uint8_t top = msb64(n);
    uint8_t k = (uint8_t)(62u - top);
    uint64_t q = (n << k) / d;
    uint8_t p = msb64(q);
    int e = (int)p - (int)k + 127;
    uint8_t sh = (uint8_t)(p - 23u);
    uint64_t mant = (q + (1ULL << (sh - 1u))) >> sh;
    if (mant >= (1ULL << 24)) {
        mant >>= 1;
        e++;
    }
    uint32_t bits;
    if (e <= 0)
        bits = 0;
    else if (e >= 255)
        bits = 0x7F800000u;
    else
        bits = ((uint32_t)e << 23) | ((uint32_t)mant & 0x7FFFFFu);
    return bits | (neg ? 0x80000000u : 0u);
}

bool df_decode(const rt_obj_t *o, uint8_t def_order, const uint16_t w[2], int32_t *disp)
{
    uint8_t ord = o->order ? o->order : def_order;

    if (o->area == RT_AREA_COIL || o->area == RT_AREA_DISC) {
        *disp = (w[0] & 1u) ? 1 : 0;
        return true;
    }
    if (o->bit != RT_BIT_NONE) {
        *disp = (w[0] >> (o->bit & 15u)) & 1u;
        return true;
    }

    int64_t raw;
    switch (o->dtype) {
    case RT_DT_U16:
        raw = w[0];
        break;
    case RT_DT_I16:
        raw = (int16_t)w[0];
        break;
    case RT_DT_BCD16: {
        uint32_t r;
        if (!bcd_decode(w[0], 4, &r))
            return false;
        raw = r;
        break;
    }
    case RT_DT_U32:
        raw = df_join32(ord, w[0], w[1]);
        break;
    case RT_DT_I32:
        raw = (int32_t)df_join32(ord, w[0], w[1]);
        break;
    case RT_DT_BCD32: {
        uint32_t r;
        if (!bcd_decode(df_join32(ord, w[0], w[1]), 8, &r))
            return false;
        raw = r;
        break;
    }
    case RT_DT_F32:
        return f32_decode(df_join32(ord, w[0], w[1]), o->decimals, o->gain, disp);
    default:
        return false;
    }
    *disp = sat32(gain_apply(raw, o->gain));
    return true;
}

bool df_encode(const rt_obj_t *o, uint8_t def_order, int32_t disp, uint16_t w[2])
{
    uint8_t ord = o->order ? o->order : def_order;

    if (o->dtype == RT_DT_F32) {
        df_split32(ord, f32_encode(disp, o->decimals, o->gain), &w[0], &w[1]);
        return true;
    }

    int64_t raw = gain_undo(disp, o->gain);
    switch (o->dtype) {
    case RT_DT_U16:
        if (raw < 0 || raw > 65535)
            return false;
        w[0] = (uint16_t)raw;
        return true;
    case RT_DT_I16:
        if (raw < -32768 || raw > 32767)
            return false;
        w[0] = (uint16_t)(int16_t)raw;
        return true;
    case RT_DT_BCD16:
        if (raw < 0 || raw > 9999)
            return false;
        w[0] = (uint16_t)bcd_encode((uint32_t)raw, 4);
        return true;
    case RT_DT_U32:
        if (raw < 0 || raw > 0xFFFFFFFFLL)
            return false;
        df_split32(ord, (uint32_t)raw, &w[0], &w[1]);
        return true;
    case RT_DT_I32:
        if (raw < INT32_MIN || raw > INT32_MAX)
            return false;
        df_split32(ord, (uint32_t)(int32_t)raw, &w[0], &w[1]);
        return true;
    case RT_DT_BCD32:
        if (raw < 0 || raw > 99999999LL)
            return false;
        df_split32(ord, bcd_encode((uint32_t)raw, 8), &w[0], &w[1]);
        return true;
    default:
        return false;
    }
}
