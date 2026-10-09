/*
 * RS485 trên USART2 (DE phần cứng, cấu hình bởi MX_USART2_UART_Init) + TIM14 đo khoảng lặng t3.5.
 * Nhận từng byte trong ngắt; TIM14 (one-pulse) hết hạn mà không có byte mới => khung hoàn chỉnh.
 */
#include "rs485.h"
#include "mb_slave.h"       /* MB_MAX_ADU */
#include "main.h"
#include "stm32g0xx_ll_usart.h"
#include "stm32g0xx_ll_tim.h"

#define RS_USART USART2
#define RS_TIM   TIM14

enum { R_RX = 0, R_READY, R_TX };

static uint8_t rx_buf[MB_MAX_ADU];
static uint8_t tx_buf[MB_MAX_ADU];
static volatile uint16_t rx_len;
static volatile uint8_t  rx_bad;
static volatile uint8_t  state = R_RX;
static volatile uint16_t tx_len;
static volatile uint16_t tx_pos;
static uint32_t t35_us_val = 4011u;

/* t3.5 = 3,5 ký tự x 11 bit; cố định 1750 µs khi baud > 19200 (khuyến nghị của chuẩn Modbus) */
static uint32_t t35_us(uint32_t baud)
{
    if (baud > 19200u)
        return 1750u;
    return (38500000u + baud - 1u) / baud;
}

void rs485_init(uint32_t baud, uint8_t parity, uint8_t stop)
{
    t35_us_val = t35_us(baud);
    uint32_t pclk = HAL_RCC_GetPCLK1Freq();

    LL_TIM_DisableCounter(RS_TIM);
    LL_TIM_SetPrescaler(RS_TIM, pclk / 1000000u - 1u);      /* 1 µs / tick */
    LL_TIM_SetAutoReload(RS_TIM, t35_us_val);
    LL_TIM_SetOnePulseMode(RS_TIM, LL_TIM_ONEPULSEMODE_SINGLE);
    LL_TIM_SetUpdateSource(RS_TIM, LL_TIM_UPDATESOURCE_COUNTER);
    LL_TIM_GenerateEvent_UPDATE(RS_TIM);
    LL_TIM_ClearFlag_UPDATE(RS_TIM);
    LL_TIM_EnableIT_UPDATE(RS_TIM);

    LL_USART_Disable(RS_USART);
    LL_USART_SetBaudRate(RS_USART, pclk, LL_USART_PRESCALER_DIV1, LL_USART_OVERSAMPLING_16, baud);
    if (parity == 0u) {
        LL_USART_SetDataWidth(RS_USART, LL_USART_DATAWIDTH_8B);
        LL_USART_SetParity(RS_USART, LL_USART_PARITY_NONE);
    } else {
        LL_USART_SetDataWidth(RS_USART, LL_USART_DATAWIDTH_9B);     /* 8 bit dữ liệu + parity */
        LL_USART_SetParity(RS_USART, parity == 2u ? LL_USART_PARITY_ODD : LL_USART_PARITY_EVEN);
    }
    LL_USART_SetStopBitsLength(RS_USART, stop == 2u ? LL_USART_STOPBITS_2 : LL_USART_STOPBITS_1);
    LL_USART_Enable(RS_USART);
    RS_USART->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NECF | USART_ICR_PECF | USART_ICR_TCCF;
    LL_USART_EnableIT_RXNE_RXFNE(RS_USART);

    rx_len = 0;
    rx_bad = 0;
    state = R_RX;
}

uint8_t *rs485_txbuf(void)
{
    return tx_buf;
}

bool rs485_send(uint16_t len)
{
    if (state == R_TX || len == 0u || len > MB_MAX_ADU)
        return false;
    __disable_irq();
    tx_len = len;
    tx_pos = 0;
    state = R_TX;
    __enable_irq();
    LL_USART_EnableIT_TXE_TXFNF(RS_USART);
    return true;
}

bool rs485_tx_busy(void)
{
    return state == R_TX;
}

bool rs485_rx_ready(void)
{
    return state == R_READY;
}

const uint8_t *rs485_rx(uint16_t *len, bool *bad)
{
    *len = rx_len;
    *bad = rx_bad != 0u;
    return rx_buf;
}

void rs485_rx_release(void)
{
    __disable_irq();
    if (state != R_TX) {
        rx_len = 0;
        rx_bad = 0;
        state = R_RX;
    }
    __enable_irq();
}

void rs485_uart_irq(void)
{
    uint32_t isr = RS_USART->ISR;
    const uint32_t errors = USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE | USART_ISR_PE;

    if (isr & (USART_ISR_RXNE_RXFNE | errors)) {
        uint8_t byte = LL_USART_ReceiveData8(RS_USART);
        if (isr & errors) {
            RS_USART->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NECF | USART_ICR_PECF;
            rx_bad = 1;
        }
        if (state == R_RX) {
            if (rx_len < MB_MAX_ADU)
                rx_buf[rx_len++] = byte;
            else
                rx_bad = 1;
            LL_TIM_SetCounter(RS_TIM, 0);
            LL_TIM_EnableCounter(RS_TIM);
        }
    }
    if (LL_USART_IsEnabledIT_TXE_TXFNF(RS_USART) && (isr & USART_ISR_TXE_TXFNF)) {
        LL_USART_TransmitData8(RS_USART, tx_buf[tx_pos++]);
        if (tx_pos >= tx_len) {
            LL_USART_DisableIT_TXE_TXFNF(RS_USART);
            LL_USART_EnableIT_TC(RS_USART);
        }
    }
    if (LL_USART_IsEnabledIT_TC(RS_USART) && (isr & USART_ISR_TC)) {
        LL_USART_DisableIT_TC(RS_USART);
        LL_USART_ClearFlag_TC(RS_USART);
        rx_len = 0;
        rx_bad = 0;
        state = R_RX;                   /* byte cuối đã ra khỏi dây; DE tự nhả sau DEDT */
    }
}

void rs485_timer_irq(void)
{
    LL_TIM_ClearFlag_UPDATE(RS_TIM);
    if (state == R_RX && rx_len > 0u)
        state = R_READY;
}
