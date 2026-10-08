#include "tm1638.h"
#include "main.h"
#include "stm32g0xx_ll_gpio.h"

#define CMD_DATA_WRITE_AUTO  0x40u
#define CMD_DATA_READ_KEYS   0x42u
#define CMD_ADDRESS          0xC0u
#define CMD_DISPLAY_ON       0x88u

/*
 * Đường dây open-drain kéo lên 4,7k: thời gian lên ~0,2–0,5 µs.
 * Mỗi nửa chu kỳ CLK ~2–3 µs (≈ 200 kHz) để dư biên, TM1638 cho phép tới 1 MHz.
 */
static void tm_delay(void)
{
    for (volatile uint8_t i = 0; i < 6u; i++) {}
}

/* STB/CLK/DIO cùng cổng GPIOA (main.h), open-drain: "high" = thả cho điện trở kéo lên */
#define PIN_STB TM_STB_Pin
#define PIN_CLK TM_CLK_Pin
#define PIN_DIO TM_DIO_Pin
#define LINE_HIGH(pin) LL_GPIO_SetOutputPin(TM_STB_GPIO_Port, (pin))
#define LINE_LOW(pin)  LL_GPIO_ResetOutputPin(TM_STB_GPIO_Port, (pin))
#define DIO_READ()     (LL_GPIO_IsInputPinSet(TM_DIO_GPIO_Port, TM_DIO_Pin) ? 1u : 0u)

static void tm_start(void)
{
    LINE_LOW(PIN_STB);
    tm_delay();
}

static void tm_stop(void)
{
    LINE_HIGH(PIN_CLK);
    LINE_HIGH(PIN_DIO);
    tm_delay();
    LINE_HIGH(PIN_STB);
    tm_delay();
}

/* LSB trước, TM1638 chốt DIO ở sườn lên CLK */
static void tm_write_byte(uint8_t b)
{
    for (uint8_t i = 0; i < 8u; i++) {
        LINE_LOW(PIN_CLK);
        if (b & 1u)
            LINE_HIGH(PIN_DIO);
        else
            LINE_LOW(PIN_DIO);
        tm_delay();
        LINE_HIGH(PIN_CLK);
        tm_delay();
        b >>= 1;
    }
}

/* TM1638 đẩy bit ra ở sườn xuống CLK, đọc khi CLK thấp rồi kéo CLK lên */
static uint8_t tm_read_byte(void)
{
    uint8_t b = 0;
    LINE_HIGH(PIN_DIO);  /* thả DIO (open-drain) để TM1638 điều khiển */
    for (uint8_t i = 0; i < 8u; i++) {
        LINE_LOW(PIN_CLK);
        tm_delay();
        b >>= 1;
        if (DIO_READ())
            b |= 0x80u;
        LINE_HIGH(PIN_CLK);
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

void tm1638_init(void)
{
    static const uint8_t blank[TM1638_RAM_SIZE] = {0};
    LINE_HIGH(PIN_STB);
    LINE_HIGH(PIN_CLK);
    LINE_HIGH(PIN_DIO);
    tm_delay();
    tm1638_write_ram(blank, 0);
}

void tm1638_write_ram(const uint8_t ram[TM1638_RAM_SIZE], uint8_t brightness)
{
    tm_command(CMD_DATA_WRITE_AUTO);

    tm_start();
    tm_write_byte(CMD_ADDRESS | 0x00u);
    for (uint8_t i = 0; i < TM1638_RAM_SIZE; i++)
        tm_write_byte(ram[i]);
    tm_stop();

    tm_command((uint8_t)(CMD_DISPLAY_ON | (brightness & 0x07u)));
}

uint8_t tm1638_read_keys(uint8_t keys[4])
{
    tm_start();
    tm_write_byte(CMD_DATA_READ_KEYS);
    LINE_HIGH(PIN_DIO);
    tm_delay();  /* Twait >= 1 µs trước khi đọc */
    uint8_t all_ones = 1;
    for (uint8_t i = 0; i < 4u; i++) {
        keys[i] = tm_read_byte();
        if (keys[i] != 0xFFu)
            all_ones = 0;
    }
    tm_stop();

    /* DIO bị kéo lên cố định (TM1638 không trả lời / đứt dây) -> bỏ mẫu này */
    return all_ones ? 0u : 1u;
}
