/*
 * Lớp truyền Modbus RTU trên USART2 + SIT3485.
 *  - Nhận bằng ngắt RXNE, TIM14 đo khoảng lặng t3.5 để chốt khung
 *    (USART2 của G030 không có Receiver Timeout).
 *  - Phát bằng ngắt TXE/TC, chân DE (PA1) do phần cứng USART tự bật/tắt.
 */
#ifndef MODBUS_RTU_H
#define MODBUS_RTU_H

#include <stdint.h>

void mb_rtu_init(void);       /* sau MX_USART2_UART_Init() và MX_TIM14_Init() */
void mb_rtu_poll(void);       /* gọi liên tục trong vòng lặp chính */
void mb_rtu_uart_irq(void);   /* từ USART2_IRQHandler */
void mb_rtu_timer_irq(void);  /* từ TIM14_IRQHandler */

#endif
