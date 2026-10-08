#include "regs.h"

static uint16_t holding[REG_COUNT];
static uint8_t output_changed;

void regs_init(void)
{
    for (uint16_t i = 0; i < REG_COUNT; i++)
        holding[i] = 0;
    output_changed = 1;
}

uint8_t regs_read(uint16_t addr, uint16_t *value)
{
    if (addr >= REG_COUNT)
        return MB_EX_ILLEGAL_DATA_ADDRESS;
    *value = holding[addr];
    return MB_EX_NONE;
}

uint8_t regs_check_write(uint16_t addr, uint16_t value)
{
    switch (addr) {
    case REG_DISPLAY1:
    case REG_DISPLAY2:
        return (value <= DISPLAY_MAX_VALUE) ? MB_EX_NONE : MB_EX_ILLEGAL_DATA_VALUE;
    case REG_LEDS:
        return MB_EX_NONE;
    default:
        /* 40001/40002 là read-only, ngoài bản đồ là không tồn tại */
        return MB_EX_ILLEGAL_DATA_ADDRESS;
    }
}

void regs_write(uint16_t addr, uint16_t value)
{
    if (addr == REG_LEDS)
        value &= 0x00FFu;
    if (holding[addr] != value) {
        holding[addr] = value;
        output_changed = 1;
    }
}

uint16_t regs_get(uint16_t addr)
{
    return (addr < REG_COUNT) ? holding[addr] : 0u;
}

void regs_set_key_state(uint16_t state)
{
    holding[REG_KEY_STATE] = state;
}

void regs_add_key_events(uint16_t count)
{
    holding[REG_KEY_COUNTER] = (uint16_t)(holding[REG_KEY_COUNTER] + count);
}

uint8_t regs_take_output_changed(void)
{
    uint8_t changed = output_changed;
    output_changed = 0;
    return changed;
}
