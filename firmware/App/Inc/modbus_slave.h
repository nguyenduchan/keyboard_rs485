/*
 * Xử lý khung Modbus RTU (không phụ thuộc phần cứng, test được trên PC).
 * Hỗ trợ FC03 Read Holding Registers, FC06 Write Single Register,
 * FC16 Write Multiple Registers. Địa chỉ 0 = broadcast (chỉ ghi, không trả lời).
 */
#ifndef MODBUS_SLAVE_H
#define MODBUS_SLAVE_H

#include <stdint.h>

#define MB_RTU_MAX_ADU 256u

uint16_t mb_crc16(const uint8_t *data, uint16_t len);

/*
 * req/len: khung nhận được (kể cả CRC). resp: bộ đệm >= MB_RTU_MAX_ADU.
 * Trả về số byte cần gửi (đã kèm CRC); 0 = không trả lời
 * (sai CRC, không phải địa chỉ của mình, broadcast, khung hỏng).
 */
uint16_t mb_slave_process(uint8_t slave_id, const uint8_t *req, uint16_t len, uint8_t *resp);

#endif
