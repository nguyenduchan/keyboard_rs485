#include "display.h"
#include "hwif.h"
#include "config.h"
#include <string.h>

static uint8_t seg[8];
static uint8_t led_on, led_blink;
static uint8_t row_blink;           /* bit0 hàng trên, bit1 hàng dưới */
static uint8_t brightness;
static uint8_t last_ram[TM_RAM_SIZE];
static uint8_t last_bright = 0xFFu;
static uint32_t last_write;
static bool force = true;

/* a=bit0 b=bit1 c=bit2 d=bit3 e=bit4 f=bit5 g=bit6 */
static const uint8_t font_digit[10] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F };
/* A..Z (chữ hoa); 0 = không vẽ được */
static const uint8_t font_upper[26] = {
    0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71, 0x3D, 0x76, 0x06, 0x1E, 0x76, 0x38, 0x00,   /* A B C D E F G H I J K L M */
    0x54, 0x3F, 0x73, 0x67, 0x50, 0x6D, 0x78, 0x3E, 0x00, 0x00, 0x00, 0x6E, 0x5B,   /* N O P Q R S T U V W X Y Z */
};
/* chữ thường có hình riêng */
static const uint8_t font_lower[26] = {
    0x5F, 0x7C, 0x58, 0x5E, 0x7B, 0x71, 0x6F, 0x74, 0x04, 0x0E, 0x75, 0x30, 0x00,   /* a b c d e f g h i j k l m */
    0x54, 0x5C, 0x73, 0x67, 0x50, 0x6D, 0x78, 0x1C, 0x1C, 0x00, 0x00, 0x6E, 0x5B,   /* n o p q r s t u v w x y z */
};

uint8_t display_char_seg(uint8_t c)
{
    c &= 0x7Fu;
    if (c >= '0' && c <= '9')
        return font_digit[c - '0'];
    if (c >= 'A' && c <= 'Z')
        return font_upper[c - 'A'] ? font_upper[c - 'A'] : 0x40u;
    if (c >= 'a' && c <= 'z')
        return font_lower[c - 'a'] ? font_lower[c - 'a'] : 0x40u;
    switch (c) {
    case ' ': return 0x00;
    case '-': return 0x40;
    case '_': return 0x08;
    case '=': return 0x48;
    case '"': return 0x22;
    case '\'': return 0x20;
    case '[': return 0x39;
    case ']': return 0x0F;
    case '*': return 0x63;
    case '^': return 0x23;
    default:  return 0x40;
    }
}

bool display_char_ok(uint8_t c)
{
    c &= 0x7Fu;
    if (c >= '0' && c <= '9')
        return true;
    if (c >= 'A' && c <= 'Z')
        return font_upper[c - 'A'] != 0u;
    if (c >= 'a' && c <= 'z')
        return font_lower[c - 'a'] != 0u;
    return c == ' ' || c == '-' || c == '_' || c == '=' || c == '"' || c == '\'' || c == '[' || c == ']' ||
           c == '*' || c == '^';
}

void display_init(uint8_t b)
{
    brightness = b & 7u;
    display_clear();
    force = true;
}

void display_set_brightness(uint8_t b)
{
    brightness = b & 7u;
}

void display_clear(void)
{
    memset(seg, 0, sizeof seg);
    led_on = led_blink = row_blink = 0;
}

void display_row_clear(uint8_t row)
{
    memset(&seg[(row & 1u) * 4u], 0, 4);
}

void display_text(uint8_t row, const uint8_t text[4])
{
    uint8_t *d = &seg[(row & 1u) * 4u];
    for (uint8_t i = 0; i < 4u; i++)
        d[i] = (uint8_t)(display_char_seg(text[i]) | ((text[i] & 0x80u) ? 0x80u : 0u));
}

void display_str(uint8_t row, const char *s)
{
    uint8_t t[4] = { ' ', ' ', ' ', ' ' };
    for (uint8_t i = 0; i < 4u && s[i]; i++)
        t[i] = (uint8_t)s[i];
    display_text(row, t);
}

void display_row_dashes(uint8_t row)
{
    display_str(row, "----");
}

void display_raw(uint8_t digit, uint8_t segs)
{
    seg[digit & 7u] = segs;
}

void display_number(uint8_t row, int32_t v, uint8_t decimals, uint8_t flags, uint8_t width)
{
    uint8_t n = (width >= 8u) ? 8u : 4u;
    uint8_t *d = &seg[(width >= 8u) ? 0u : (row & 1u) * 4u];
    uint8_t digits[10];
    uint8_t cnt = 0;
    bool neg = v < 0;
    uint32_t u = neg ? (uint32_t)(-(int64_t)v) : (uint32_t)v;

    do {
        digits[cnt++] = (uint8_t)(u % 10u);
        u /= 10u;
    } while (u && cnt < 10u);
    while (cnt < (uint8_t)(decimals + 1u))
        digits[cnt++] = 0;
    if ((flags & DISP_FLAG_ZEROS) && cnt < n)
        while (cnt < (uint8_t)(n - (neg ? 1u : 0u)))
            digits[cnt++] = 0;
    uint8_t need = (uint8_t)(cnt + (neg ? 1u : 0u));
    if (need > n) {
        for (uint8_t i = 0; i < n; i++)
            d[i] = 0x40;
        return;
    }
    memset(d, 0, n);
    uint8_t pos = (uint8_t)(n - 1u);
    for (uint8_t i = 0; i < cnt; i++, pos--)
        d[pos] = font_digit[digits[i]];
    if (neg)
        d[pos] = 0x40;
    if (decimals)
        d[n - 1u - decimals] |= 0x80u;          /* chấm thập phân gắn vào chữ số đứng ngay trước phần lẻ */
}

void display_leds(uint8_t on_mask, uint8_t blink_mask)
{
    led_on = on_mask;
    led_blink = blink_mask;
}

void display_row_blink(uint8_t row, bool blink)
{
    if (blink)
        row_blink |= (uint8_t)(1u << (row & 1u));
    else
        row_blink &= (uint8_t)~(1u << (row & 1u));
}

void display_force(void)
{
    force = true;
}

uint8_t display_peek_seg(uint8_t digit)
{
    return seg[digit & 7u];
}

void display_flush(uint32_t now)
{
    bool phase = ((now / BLINK_HALF_MS) & 1u) == 0u;        /* true: pha sáng */
    uint8_t ram[TM_RAM_SIZE];
    uint8_t leds = led_on;

    if (!phase)
        leds = (uint8_t)(leds & ~led_blink);
    else
        leds = (uint8_t)(leds | led_blink);
    for (uint8_t g = 0; g < 8u; g++) {
        uint8_t s = seg[g];
        if (!phase && (row_blink & (1u << (g / 4u))))
            s = 0;
        ram[2u * g] = s;
        ram[2u * g + 1u] = (uint8_t)((leds >> g) & 1u);
    }
    if (force || brightness != last_bright || memcmp(ram, last_ram, sizeof ram) != 0 ||
        (uint32_t)(now - last_write) >= DISPLAY_REFRESH_MS) {
        hw_tm_write(ram, brightness);
        memcpy(last_ram, ram, sizeof ram);
        last_bright = brightness;
        last_write = now;
        force = false;
    }
}
