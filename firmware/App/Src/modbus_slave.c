#include "modbus_slave.h"
#include "regs.h"

#define FC_READ_HOLDING   0x03u
#define FC_WRITE_SINGLE   0x06u
#define FC_WRITE_MULTIPLE 0x10u

#define MB_READ_MAX_QTY   125u
#define MB_WRITE_MAX_QTY  123u

uint16_t mb_crc16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFFu;
    while (len--) {
        crc ^= *data++;
        for (uint8_t i = 0; i < 8; i++)
            crc = (crc & 1u) ? (uint16_t)((crc >> 1) ^ 0xA001u) : (uint16_t)(crc >> 1);
    }
    return crc;
}

static uint16_t get_u16(const uint8_t *p)
{
    return (uint16_t)((p[0] << 8) | p[1]);
}

static void put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

/* Gắn CRC (byte thấp trước) và trả về tổng độ dài */
static uint16_t finish(uint8_t *resp, uint16_t len)
{
    uint16_t crc = mb_crc16(resp, len);
    resp[len++] = (uint8_t)crc;
    resp[len++] = (uint8_t)(crc >> 8);
    return len;
}

static uint16_t exception(uint8_t *resp, uint8_t fc, uint8_t code)
{
    resp[1] = (uint8_t)(fc | 0x80u);
    resp[2] = code;
    return finish(resp, 3);
}

static uint16_t read_holding(const uint8_t *req, uint16_t len, uint8_t *resp)
{
    if (len != 8)
        return 0;
    uint16_t start = get_u16(&req[2]);
    uint16_t qty = get_u16(&req[4]);

    if (qty == 0 || qty > MB_READ_MAX_QTY)
        return exception(resp, FC_READ_HOLDING, MB_EX_ILLEGAL_DATA_VALUE);
    if ((uint32_t)start + qty > REG_COUNT)
        return exception(resp, FC_READ_HOLDING, MB_EX_ILLEGAL_DATA_ADDRESS);

    resp[1] = FC_READ_HOLDING;
    resp[2] = (uint8_t)(qty * 2u);
    for (uint16_t i = 0; i < qty; i++) {
        uint16_t v;
        uint8_t ex = regs_read((uint16_t)(start + i), &v);
        if (ex != MB_EX_NONE)
            return exception(resp, FC_READ_HOLDING, ex);
        put_u16(&resp[3 + 2u * i], v);
    }
    return finish(resp, (uint16_t)(3u + qty * 2u));
}

static uint16_t write_single(const uint8_t *req, uint16_t len, uint8_t *resp)
{
    if (len != 8)
        return 0;
    uint16_t addr = get_u16(&req[2]);
    uint16_t value = get_u16(&req[4]);

    uint8_t ex = regs_check_write(addr, value);
    if (ex != MB_EX_NONE)
        return exception(resp, FC_WRITE_SINGLE, ex);
    regs_write(addr, value);

    /* Phản hồi = echo yêu cầu */
    for (uint8_t i = 1; i < 6; i++)
        resp[i] = req[i];
    return finish(resp, 6);
}

static uint16_t write_multiple(const uint8_t *req, uint16_t len, uint8_t *resp)
{
    if (len < 9)
        return 0;
    uint16_t start = get_u16(&req[2]);
    uint16_t qty = get_u16(&req[4]);
    uint8_t byte_count = req[6];

    if (len != 9u + byte_count)
        return 0;
    if (qty == 0 || qty > MB_WRITE_MAX_QTY || byte_count != qty * 2u)
        return exception(resp, FC_WRITE_MULTIPLE, MB_EX_ILLEGAL_DATA_VALUE);
    if ((uint32_t)start + qty > REG_COUNT)
        return exception(resp, FC_WRITE_MULTIPLE, MB_EX_ILLEGAL_DATA_ADDRESS);

    /* Kiểm tra toàn bộ trước rồi mới ghi: lệnh lỗi không làm đổi thanh ghi nào */
    for (uint16_t i = 0; i < qty; i++) {
        uint8_t ex = regs_check_write((uint16_t)(start + i), get_u16(&req[7 + 2u * i]));
        if (ex != MB_EX_NONE)
            return exception(resp, FC_WRITE_MULTIPLE, ex);
    }
    for (uint16_t i = 0; i < qty; i++)
        regs_write((uint16_t)(start + i), get_u16(&req[7 + 2u * i]));

    resp[1] = FC_WRITE_MULTIPLE;
    put_u16(&resp[2], start);
    put_u16(&resp[4], qty);
    return finish(resp, 6);
}

uint16_t mb_slave_process(uint8_t slave_id, const uint8_t *req, uint16_t len, uint8_t *resp)
{
    if (len < 4 || len > MB_RTU_MAX_ADU)
        return 0;
    uint8_t addr = req[0];
    if (addr != slave_id && addr != 0u)
        return 0;
    uint16_t crc = (uint16_t)(req[len - 2] | (req[len - 1] << 8));
    if (mb_crc16(req, (uint16_t)(len - 2)) != crc)
        return 0;

    len = (uint16_t)(len - 2);  /* bỏ CRC, các hàm con kiểm tra độ dài PDU không CRC */
    uint8_t fc = req[1];
    resp[0] = slave_id;

    uint16_t out;
    switch (fc) {
    case FC_READ_HOLDING:   out = read_holding(req, len + 2u, resp); break;
    case FC_WRITE_SINGLE:   out = write_single(req, len + 2u, resp); break;
    case FC_WRITE_MULTIPLE: out = write_multiple(req, len + 2u, resp); break;
    default:                out = exception(resp, fc, MB_EX_ILLEGAL_FUNCTION); break;
    }

    /* Broadcast: thực hiện lệnh ghi nhưng không bao giờ trả lời */
    return (addr == 0u) ? 0u : out;
}
