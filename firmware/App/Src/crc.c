#include "crc.h"

uint16_t crc16_modbus(const uint8_t *p, size_t n)
{
    uint16_t crc = 0xFFFFu;
    while (n--) {
        crc ^= *p++;
        for (uint8_t i = 0; i < 8u; i++)
            crc = (crc & 1u) ? (uint16_t)((crc >> 1) ^ 0xA001u) : (uint16_t)(crc >> 1);
    }
    return crc;
}

/* CRC-32 chuẩn IEEE 802.3 (đa thức 0xEDB88320, giá trị đầu/ cuối 0xFFFFFFFF), bảng 16 mục để tiết kiệm flash */
static const uint32_t crc32_tab[16] = {
    0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu, 0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
    0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu, 0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu,
};

uint32_t crc32_update(uint32_t crc, const uint8_t *p, size_t n)
{
    crc = ~crc;
    while (n--) {
        crc ^= *p++;
        crc = (crc >> 4) ^ crc32_tab[crc & 0x0Fu];
        crc = (crc >> 4) ^ crc32_tab[crc & 0x0Fu];
    }
    return ~crc;
}
