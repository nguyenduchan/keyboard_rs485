/*
 * Lớp truyền RS485 cho Modbus RTU: USART2 + chân DE phần cứng, nhận cả khung (chốt bằng khoảng lặng t3.5).
 * Dùng chung cho master và slave. Trên PC có bản mô phỏng trong tests/fake_hw.c.
 */
#ifndef RS485_H
#define RS485_H
#include <stdint.h>
#include <stdbool.h>

void     rs485_init(uint32_t baud, uint8_t parity, uint8_t stop);   /* parity: 0 không, 1 chẵn, 2 lẻ */
uint8_t *rs485_txbuf(void);                     /* bộ đệm gửi (MB_MAX_ADU byte) */
bool     rs485_send(uint16_t len);              /* bắt đầu gửi txbuf[0..len); false nếu đang bận */
bool     rs485_tx_busy(void);
bool     rs485_rx_ready(void);                  /* đã nhận trọn một khung (im lặng >= t3.5) */
const uint8_t *rs485_rx(uint16_t *len, bool *bad);  /* bad: lỗi khung/parity/tràn */
void     rs485_rx_release(void);                /* bỏ khung hiện tại, sẵn sàng nhận khung mới */

/* ngắt (gọi từ stm32g0xx_it.c) */
void     rs485_uart_irq(void);
void     rs485_timer_irq(void);

#endif
