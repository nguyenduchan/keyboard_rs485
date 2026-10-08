#include "modbus_rtu.h"
#include "modbus_slave.h"
#include "main.h"
#include "config.h"
#include "stm32g0xx_ll_usart.h"
#include "stm32g0xx_ll_tim.h"

/* Khởi tạo phần cứng do CubeMX sinh trong main.c */
#define MB_USART USART2
#define MB_TIM   TIM14

enum { MB_RX, MB_FRAME_READY, MB_TX };

static uint8_t rx_buf[MB_RTU_MAX_ADU];
static uint8_t tx_buf[MB_RTU_MAX_ADU];
static volatile uint16_t rx_len;
static volatile uint8_t rx_bad;       /* lỗi khung/parity/tràn trong khung hiện tại */
static volatile uint8_t state = MB_RX;
static volatile uint16_t tx_len;
static volatile uint16_t tx_pos;

/* t3.5 theo chuẩn Modbus: 3,5 ký tự x 11 bit; cố định 1750 µs khi baud > 19200 */
static uint32_t t35_us(void)
{
    if (MODBUS_BAUDRATE > 19200u)
        return 1750u;
    return (38500000u + MODBUS_BAUDRATE - 1u) / MODBUS_BAUDRATE;  /* 9600 -> 4011 µs */
}

static void silence_timer_restart(void)
{
    LL_TIM_SetCounter(MB_TIM, 0);
    LL_TIM_EnableCounter(MB_TIM);
}

void mb_rtu_init(void)
{
    /* ---- TIM14 (MX_TIM14_Init): 1 µs/tick, chuyển sang one-pulse, ngắt khi hết t3.5 ---- */
    LL_TIM_DisableCounter(MB_TIM);
    LL_TIM_SetPrescaler(MB_TIM, HAL_RCC_GetPCLK1Freq() / 1000000u - 1u);
    LL_TIM_SetAutoReload(MB_TIM, t35_us());
    LL_TIM_SetOnePulseMode(MB_TIM, LL_TIM_ONEPULSEMODE_SINGLE);
    LL_TIM_SetUpdateSource(MB_TIM, LL_TIM_UPDATESOURCE_COUNTER);  /* UG không sinh ngắt */
    LL_TIM_GenerateEvent_UPDATE(MB_TIM);                          /* nạp PSC */
    LL_TIM_ClearFlag_UPDATE(MB_TIM);
    LL_TIM_EnableIT_UPDATE(MB_TIM);

    /* ---- USART2 (MX_USART2_UART_Init, RS485 DE phần cứng): áp baud/parity từ config.h ---- */
    LL_USART_Disable(MB_USART);
    LL_USART_SetBaudRate(MB_USART, HAL_RCC_GetPCLK1Freq(), LL_USART_PRESCALER_DIV1,
                         LL_USART_OVERSAMPLING_16, MODBUS_BAUDRATE);
#if MODBUS_PARITY == MB_PARITY_NONE
    LL_USART_SetDataWidth(MB_USART, LL_USART_DATAWIDTH_8B);
    LL_USART_SetParity(MB_USART, LL_USART_PARITY_NONE);
#else
    LL_USART_SetDataWidth(MB_USART, LL_USART_DATAWIDTH_9B);  /* 8 bit dữ liệu + parity */
    LL_USART_SetParity(MB_USART, (MODBUS_PARITY == MB_PARITY_ODD) ? LL_USART_PARITY_ODD
                                                                  : LL_USART_PARITY_EVEN);
#endif
    LL_USART_SetStopBitsLength(MB_USART, (MODBUS_STOP_BITS == 2u) ? LL_USART_STOPBITS_2
                                                                  : LL_USART_STOPBITS_1);
    LL_USART_Enable(MB_USART);

    LL_USART_ClearFlag_ORE(MB_USART);
    LL_USART_ClearFlag_FE(MB_USART);
    LL_USART_ClearFlag_NE(MB_USART);
    LL_USART_ClearFlag_PE(MB_USART);
    LL_USART_ClearFlag_TC(MB_USART);
    LL_USART_EnableIT_RXNE_RXFNE(MB_USART);
}

/* Gọi từ USART2_IRQHandler (Core/Src/stm32g0xx_it.c) */
void mb_rtu_uart_irq(void)
{
    uint32_t isr = MB_USART->ISR;
    const uint32_t errors = USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE | USART_ISR_PE;

    if (isr & (USART_ISR_RXNE_RXFNE | errors)) {
        uint8_t byte = LL_USART_ReceiveData8(MB_USART);
        if (isr & errors) {
            MB_USART->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NECF | USART_ICR_PECF;
            rx_bad = 1;
        }
        if (state == MB_RX) {
            if (rx_len < MB_RTU_MAX_ADU)
                rx_buf[rx_len++] = byte;
            else
                rx_bad = 1;
            silence_timer_restart();
        }
    }

    if (LL_USART_IsEnabledIT_TXE_TXFNF(MB_USART) && (isr & USART_ISR_TXE_TXFNF)) {
        LL_USART_TransmitData8(MB_USART, tx_buf[tx_pos++]);
        if (tx_pos >= tx_len) {
            LL_USART_DisableIT_TXE_TXFNF(MB_USART);
            LL_USART_EnableIT_TC(MB_USART);
        }
    }

    if (LL_USART_IsEnabledIT_TC(MB_USART) && (isr & USART_ISR_TC)) {
        /* Byte cuối đã ra khỏi đường dây; DE tự nhả sau DEDT */
        LL_USART_DisableIT_TC(MB_USART);
        LL_USART_ClearFlag_TC(MB_USART);
        rx_len = 0;
        rx_bad = 0;
        state = MB_RX;
    }
}

/* Gọi từ TIM14_IRQHandler: hết t3.5 không có byte mới -> khung kết thúc */
void mb_rtu_timer_irq(void)
{
    LL_TIM_ClearFlag_UPDATE(MB_TIM);
    if (state == MB_RX && rx_len > 0u)
        state = MB_FRAME_READY;
}

void mb_rtu_poll(void)
{
    if (state != MB_FRAME_READY)
        return;

    uint16_t len = 0;
    if (!rx_bad)
        len = mb_slave_process(MODBUS_SLAVE_ID, rx_buf, rx_len, tx_buf);

    if (len == 0u) {
        __disable_irq();
        rx_len = 0;
        rx_bad = 0;
        state = MB_RX;
        __enable_irq();
        return;
    }

    /* Đã im lặng >= t3.5 kể từ byte cuối của yêu cầu, có thể trả lời ngay */
    tx_len = len;
    tx_pos = 0;
    state = MB_TX;
    LL_USART_EnableIT_TXE_TXFNF(MB_USART);
}
