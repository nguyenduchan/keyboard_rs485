#include "mb_slave.h"
#include "crc.h"

static uint16_t be16(const uint8_t *p)
{
    return (uint16_t)((p[0] << 8) | p[1]);
}

static uint16_t finish(uint8_t *resp, uint16_t n)
{
    uint16_t crc = crc16_modbus(resp, n);
    resp[n++] = (uint8_t)crc;
    resp[n++] = (uint8_t)(crc >> 8);
    return n;
}

static uint16_t exception(uint8_t id, uint8_t fn, uint8_t code, uint8_t *resp)
{
    resp[0] = id;
    resp[1] = (uint8_t)(fn | 0x80u);
    resp[2] = code;
    return finish(resp, 3);
}

uint16_t mbs_process(uint8_t my_id, const uint8_t *req, uint16_t len, uint8_t *resp, const mbs_ops_t *ops)
{
    if (len < 4u || len > MB_MAX_ADU)
        return 0;
    uint16_t rx_crc = (uint16_t)(req[len - 2u] | (req[len - 1u] << 8));
    if (crc16_modbus(req, (size_t)len - 2u) != rx_crc)
        return 0;
    uint8_t id = req[0], fn = req[1];
    bool bcast = (id == 0u);
    if (!bcast && id != my_id)
        return 0;

    bool is_write = (fn == 5u || fn == 6u || fn == 15u || fn == 16u);
    if (bcast && !is_write)
        return 0;

    uint16_t out = 0;
    uint8_t ex = 0;
    uint16_t body = (uint16_t)(len - 2u);   /* độ dài không CRC */

    switch (fn) {
    case 1: case 2: {                               /* đọc cuộn dây / đầu vào rời */
        if (body != 6u) { ex = MB_EX_VALUE; break; }
        uint16_t addr = be16(&req[2]), qty = be16(&req[4]);
        if (qty < 1u || qty > 2000u) { ex = MB_EX_VALUE; break; }
        if ((uint32_t)addr + qty > 0x10000u) { ex = MB_EX_ADDRESS; break; }
        uint8_t nbytes = (uint8_t)((qty + 7u) / 8u);
        resp[0] = id; resp[1] = fn; resp[2] = nbytes;
        for (uint8_t i = 0; i < nbytes; i++)
            resp[3 + i] = 0;
        for (uint16_t i = 0; i < qty && !ex; i++) {
            uint8_t v = 0;
            ex = ops->rd_bit((uint16_t)(addr + i), &v, fn == 2u);
            if (v)
                resp[3 + i / 8u] |= (uint8_t)(1u << (i % 8u));
        }
        if (!ex)
            out = finish(resp, (uint16_t)(3u + nbytes));
        break;
    }
    case 3: case 4: {                               /* đọc thanh ghi */
        if (body != 6u) { ex = MB_EX_VALUE; break; }
        uint16_t addr = be16(&req[2]), qty = be16(&req[4]);
        if (qty < 1u || qty > 125u) { ex = MB_EX_VALUE; break; }
        if ((uint32_t)addr + qty > 0x10000u) { ex = MB_EX_ADDRESS; break; }
        resp[0] = id; resp[1] = fn; resp[2] = (uint8_t)(qty * 2u);
        for (uint16_t i = 0; i < qty && !ex; i++) {
            uint16_t v = 0;
            ex = ops->rd_reg((uint16_t)(addr + i), &v, fn == 4u);
            resp[3 + 2 * i] = (uint8_t)(v >> 8);
            resp[4 + 2 * i] = (uint8_t)v;
        }
        if (!ex)
            out = finish(resp, (uint16_t)(3u + qty * 2u));
        break;
    }
    case 5: {                                       /* ghi một cuộn dây */
        if (body != 6u) { ex = MB_EX_VALUE; break; }
        uint16_t addr = be16(&req[2]), val = be16(&req[4]);
        if (val != 0x0000u && val != 0xFF00u) { ex = MB_EX_VALUE; break; }
        ex = ops->wr_bit(addr, val ? 1u : 0u);
        if (!ex) {
            for (uint8_t i = 0; i < 6u; i++)
                resp[i] = req[i];
            out = finish(resp, 6);
        }
        break;
    }
    case 6: {                                       /* ghi một thanh ghi */
        if (body != 6u) { ex = MB_EX_VALUE; break; }
        uint16_t addr = be16(&req[2]), val = be16(&req[4]);
        ex = ops->chk_wr_reg(addr, val);
        if (!ex)
            ex = ops->wr_reg(addr, val);
        if (!ex) {
            for (uint8_t i = 0; i < 6u; i++)
                resp[i] = req[i];
            out = finish(resp, 6);
        }
        break;
    }
    case 15: {                                      /* ghi nhiều cuộn dây */
        if (body < 7u) { ex = MB_EX_VALUE; break; }
        uint16_t addr = be16(&req[2]), qty = be16(&req[4]);
        uint8_t nbytes = req[6];
        if (qty < 1u || qty > 1968u || nbytes != (uint8_t)((qty + 7u) / 8u) || body != (uint16_t)(7u + nbytes)) {
            ex = MB_EX_VALUE; break;
        }
        if ((uint32_t)addr + qty > 0x10000u) { ex = MB_EX_ADDRESS; break; }
        for (uint16_t i = 0; i < qty && !ex; i++)
            ex = ops->wr_bit((uint16_t)(addr + i), (uint8_t)((req[7 + i / 8u] >> (i % 8u)) & 1u));
        if (!ex) {
            for (uint8_t i = 0; i < 6u; i++)
                resp[i] = req[i];
            out = finish(resp, 6);
        }
        break;
    }
    case 16: {                                      /* ghi nhiều thanh ghi: kiểm tra TẤT CẢ trước rồi mới ghi */
        if (body < 7u) { ex = MB_EX_VALUE; break; }
        uint16_t addr = be16(&req[2]), qty = be16(&req[4]);
        uint8_t nbytes = req[6];
        if (qty < 1u || qty > 123u || nbytes != qty * 2u || body != (uint16_t)(7u + nbytes)) {
            ex = MB_EX_VALUE; break;
        }
        if ((uint32_t)addr + qty > 0x10000u) { ex = MB_EX_ADDRESS; break; }
        for (uint16_t i = 0; i < qty && !ex; i++)
            ex = ops->chk_wr_reg((uint16_t)(addr + i), be16(&req[7 + 2 * i]));
        for (uint16_t i = 0; i < qty && !ex; i++)
            ex = ops->wr_reg((uint16_t)(addr + i), be16(&req[7 + 2 * i]));
        if (!ex) {
            for (uint8_t i = 0; i < 6u; i++)
                resp[i] = req[i];
            out = finish(resp, 6);
        }
        break;
    }
    default:
        ex = MB_EX_FUNCTION;
        break;
    }

    if (bcast)
        return 0;
    if (ex)
        return exception(id, fn, ex, resp);
    return out;
}
