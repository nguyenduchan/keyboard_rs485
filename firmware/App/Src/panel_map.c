#include "panel_map.h"
#include "config.h"

/* Vị trí bit của K1/K2 trong mỗi nhóm 4 bit dữ liệu phím TM1638 (bit0 = K3) */
#ifndef TM1638_K1_BIT
#define TM1638_K1_BIT 2u
#endif
#ifndef TM1638_K2_BIT
#define TM1638_K2_BIT 1u
#endif

/* Nét: bit0 = a ... bit6 = g, bit7 = dp */
static const uint8_t digit_font[10] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F,
};

uint32_t panel_decode_keys(const uint8_t tm_keys[4])
{
    uint8_t k1 = 0, k2 = 0;  /* bit (ks-1) = phím trên đường KS đó */

    for (uint8_t ks = 0; ks < 8u; ks++) {
        uint8_t nibble = (uint8_t)(tm_keys[ks / 2u] >> ((ks % 2u) * 4u));
        if (nibble & (1u << TM1638_K1_BIT))
            k1 |= (uint8_t)(1u << ks);
        if (nibble & (1u << TM1638_K2_BIT))
            k2 |= (uint8_t)(1u << ks);
    }

    /*
     * Ma trận không có diode: 3 phím ở 3 góc hình chữ nhật làm góc thứ 4 cũng báo nhấn.
     * Với 2 đường K, điều đó xảy ra khi có >= 2 đường KS cùng thấy cả K1 lẫn K2.
     * Khi đó không biết phím nào thật -> báo KEY_GHOST, giữ nguyên trạng thái cũ.
     */
    uint8_t both = k1 & k2;
    if (both & (uint8_t)(both - 1u))
        return KEY_GHOST;

    return (uint32_t)k1 | ((uint32_t)k2 << 8);
}

static void encode_row(uint16_t value, uint8_t seg[4])
{
    if (value > 9999u) {
        /* Không xảy ra vì Modbus đã chặn, nhưng hiển thị "----" cho chắc */
        for (uint8_t i = 0; i < 4u; i++)
            seg[i] = 0x40u;
        return;
    }
    uint8_t blanking = DISPLAY_LEADING_ZERO_BLANK;
    uint16_t div = 1000u;
    for (uint8_t i = 0; i < 4u; i++) {
        uint8_t d = (uint8_t)((value / div) % 10u);
        if (d != 0u || i == 3u)
            blanking = 0;
        seg[i] = blanking ? 0x00u : digit_font[d];
        div /= 10u;
    }
}

void panel_build_ram(uint16_t row1, uint16_t row2, uint8_t leds, uint8_t ram[16])
{
    uint8_t seg[8];
    encode_row(row1, &seg[0]);
    encode_row(row2, &seg[4]);
    for (uint8_t g = 0; g < 8u; g++) {
        ram[2u * g] = seg[g];
        ram[2u * g + 1u] = (uint8_t)((leds >> g) & 1u);
    }
}

uint8_t panel_popcount16(uint16_t v)
{
    uint8_t n = 0;
    while (v) {
        v &= (uint16_t)(v - 1u);
        n++;
    }
    return n;
}
