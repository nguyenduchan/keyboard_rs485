#include "storage.h"
#include "crc.h"
#include "hwif.h"
#include <string.h>

#define SLOT_MAGIC  0x4C534B50u     /* byte 'P','K','S','L' */
#define HDR_SIZE    16u
#define BLOB_PAD    ((RT_BLOB_SIZE + 7u) & ~7u)

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;
    uint32_t seq;
    uint16_t len;
    uint16_t rsvd;
    uint32_t crc;           /* CRC-32 của blob */
} slot_hdr_t;
#pragma pack(pop)
_Static_assert(sizeof(slot_hdr_t) == HDR_SIZE, "slot_hdr_t");
_Static_assert(HDR_SIZE + BLOB_PAD <= STORAGE_BANK_SIZE, "storage bank too small");

static uint32_t active_bank;    /* 0 = chưa có */
static uint32_t active_seq;

static bool slot_ok(uint32_t bank, uint32_t *seq)
{
    const slot_hdr_t *h = (const slot_hdr_t *)hw_flash_ptr(bank);
    if (h->magic != SLOT_MAGIC || h->len != RT_BLOB_SIZE)
        return false;
    const rt_blob_t *b = (const rt_blob_t *)hw_flash_ptr(bank + HDR_SIZE);
    if (crc32_update(0, (const uint8_t *)b, RT_BLOB_SIZE) != h->crc)
        return false;
    if (rtcfg_validate(b) != RT_OK)
        return false;
    *seq = h->seq;
    return true;
}

st_result_t storage_load(rt_blob_t *out)
{
    uint32_t sa = 0, sb = 0;
    bool a = slot_ok(STORAGE_BANK_A, &sa);
    bool b = slot_ok(STORAGE_BANK_B, &sb);
    active_bank = 0;
    active_seq = 0;
    if (!a && !b)
        return ST_NONE;
    if (a && b)
        a = ((int32_t)(sa - sb) > 0);       /* chịu được tràn số thứ tự */
    active_bank = a ? STORAGE_BANK_A : STORAGE_BANK_B;
    active_seq = a ? sa : sb;
    memcpy(out, hw_flash_ptr(active_bank + HDR_SIZE), sizeof(*out));
    return ST_OK;
}

uint32_t storage_seq(void)
{
    return active_seq;
}

static bool bank_blank(uint32_t bank)
{
    const uint8_t *p = hw_flash_ptr(bank);
    for (uint8_t i = 0; i < HDR_SIZE; i++)
        if (p[i] != 0xFFu)
            return false;
    return true;
}

bool storage_blank(void)
{
    return bank_blank(STORAGE_BANK_A) && bank_blank(STORAGE_BANK_B);
}

st_result_t storage_save(const rt_blob_t *blob)
{
    uint32_t target = (active_bank == STORAGE_BANK_A) ? STORAGE_BANK_B : STORAGE_BANK_A;
    uint32_t seq = active_seq + 1u;
    if (seq == 0u)
        seq = 1u;

    hw_wdt_kick();
    if (!hw_flash_erase_page(target))
        return ST_ERASE;
    hw_wdt_kick();

    /* dữ liệu trước (phần cuối đệm 0xFF cho đủ bội số 8) */
    uint32_t full = RT_BLOB_SIZE & ~7u;
    if (!hw_flash_write(target + HDR_SIZE, blob, full))
        return ST_WRITE;
    if (full < RT_BLOB_SIZE) {
        uint8_t tail[8];
        memset(tail, 0xFF, sizeof tail);
        memcpy(tail, (const uint8_t *)blob + full, RT_BLOB_SIZE - full);
        if (!hw_flash_write(target + HDR_SIZE + full, tail, 8u))
            return ST_WRITE;
    }
    hw_wdt_kick();

    /* tiêu đề cuối cùng: nửa sau (len, crc) rồi nửa đầu (magic, seq) = "chốt" */
    slot_hdr_t h;
    h.magic = SLOT_MAGIC;
    h.seq = seq;
    h.len = RT_BLOB_SIZE;
    h.rsvd = 0xFFFFu;
    h.crc = crc32_update(0, (const uint8_t *)blob, RT_BLOB_SIZE);
    if (!hw_flash_write(target + 8u, (const uint8_t *)&h + 8u, 8u))
        return ST_WRITE;
    if (!hw_flash_write(target, &h, 8u))
        return ST_WRITE;

    uint32_t chk;
    if (!slot_ok(target, &chk) || chk != seq ||
        memcmp(hw_flash_ptr(target + HDR_SIZE), blob, RT_BLOB_SIZE) != 0)
        return ST_VERIFY;
    active_bank = target;
    active_seq = seq;
    return ST_OK;
}
