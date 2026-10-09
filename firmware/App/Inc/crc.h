#ifndef CRC_H
#define CRC_H
#include <stdint.h>
#include <stddef.h>

uint16_t crc16_modbus(const uint8_t *p, size_t n);
uint32_t crc32_update(uint32_t crc, const uint8_t *p, size_t n);  /* crc ban đầu 0 */

#endif
