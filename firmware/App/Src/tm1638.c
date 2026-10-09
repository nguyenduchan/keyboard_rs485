/*
 * Driver TM1638 (3 dây STB/CLK/DIO, bit-bang, đường dây open-drain kéo lên +5V_LED 4,7k).
 * RAM hiển thị 16 byte; dữ liệu phím 4 byte (xem keys.c). Chỉ biên dịch cho chip.
 */
#include "hwif.h"
#include "main.h"
#include "stm32g0xx_ll_gpio.h"

#define CMD_DATA_WRITE_AUTO  0x40u
#define CMD_DATA_READ_KEYS   0x42u
#define CMD_ADDRESS          0xC0u
#define CMD_DISPLAY_ON       0x88u

/* nửa chu kỳ CLK ~2-3 µs ở 16 MHz (~200 kHz), TM1638 cho phép tới 1 MHz */
static void tm_delay(void)
{
    for (volatile uint8_t i = 0; i < 6u; i++) {
    }
}

#define HIGH(pin) LL_GPIO_SetOutputPin(TM_STB_GPIO_Port, (pin))
#define LOW(pin)  LL_GPIO_ResetOutputPin(TM_STB_GPIO_Port, (pin))
#define DIO_IN()  (LL_GPIO_IsInputPinSet(TM_DIO_GPIO_Port, TM_DIO_Pin) ? 1u : 0u)

static void tm_start(void)
{
    LOW(TM_STB_Pin);
    tm_delay();
}

static void tm_stop(void)
{
    HIGH(TM_CLK_Pin);
    HIGH(TM_DIO_Pin);
    tm_delay();
    HIGH(TM_STB_Pin);
    tm_delay();
}

static void tm_write_byte(uint8_t b)               /* LSB trước, chốt ở sườn lên CLK */
{
    for (uint8_t i = 0; i < 8u; i++) {
        LOW(TM_CLK_Pin);
        if (b & 1u)
            HIGH(TM_DIO_Pin);
        else
            LOW(TM_DIO_Pin);
        tm_delay();
        HIGH(TM_CLK_Pin);
        tm_delay();
        b >>= 1;
    }
}

static uint8_t tm_read_byte(void)                  /* TM1638 đẩy bit ở sườn xuống CLK */
{
    uint8_t b = 0;
    HIGH(TM_DIO_Pin);                               /* thả DIO */
    for (uint8_t i = 0; i < 8u; i++) {
        LOW(TM_CLK_Pin);
        tm_delay();
        b >>= 1;
        if (DIO_IN())
            b |= 0x80u;
        HIGH(TM_CLK_Pin);
        tm_delay();
    }
    return b;
}

static void tm_command(uint8_t cmd)
{
    tm_start();
    tm_write_byte(cmd);
    tm_stop();
}

void hw_tm_init(void)
{
    static const uint8_t blank[TM_RAM_SIZE] = { 0 };
    HIGH(TM_STB_Pin);
    HIGH(TM_CLK_Pin);
    HIGH(TM_DIO_Pin);
    tm_delay();
    hw_tm_write(blank, 0);
}

void hw_tm_write(const uint8_t ram[TM_RAM_SIZE], uint8_t brightness)
{
    tm_command(CMD_DATA_WRITE_AUTO);
    tm_start();
    tm_write_byte(CMD_ADDRESS);
    for (uint8_t i = 0; i < TM_RAM_SIZE; i++)
        tm_write_byte(ram[i]);
    tm_stop();
    tm_command((uint8_t)(CMD_DISPLAY_ON | (brightness & 0x07u)));
}

bool hw_tm_read_keys(uint8_t keys[4])
{
    tm_start();
    tm_write_byte(CMD_DATA_READ_KEYS);
    HIGH(TM_DIO_Pin);
    tm_delay();                                     /* Twait >= 1 µs trước khi đọc */
    bool all_ones = true;
    for (uint8_t i = 0; i < 4u; i++) {
        keys[i] = tm_read_byte();
        if (keys[i] != 0xFFu)
            all_ones = false;
    }
    tm_stop();
    return !all_ones;                               /* DIO bị kéo lên cố định = TM1638 không trả lời */
}
