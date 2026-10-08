/* Unit test chạy trên PC: xử lý khung Modbus, bản đồ thanh ghi, giải mã phím / hiển thị. */
#include <stdio.h>
#include <string.h>
#include "modbus_slave.h"
#include "regs.h"
#include "panel_map.h"
#include "config.h"

static int failures;

#define CHECK(cond) do { if (!(cond)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static uint8_t resp[MB_RTU_MAX_ADU];

/* Gắn CRC vào khung rồi gửi cho slave id 1 */
static uint16_t send(uint8_t *frame, uint16_t len)
{
    uint16_t crc = mb_crc16(frame, len);
    frame[len] = (uint8_t)crc;
    frame[len + 1] = (uint8_t)(crc >> 8);
    return mb_slave_process(1, frame, (uint16_t)(len + 2), resp);
}

static int crc_ok(uint16_t len)
{
    return len >= 4 && mb_crc16(resp, (uint16_t)(len - 2)) == (uint16_t)(resp[len - 2] | (resp[len - 1] << 8));
}

static void test_crc(void)
{
    /* Ví dụ chuẩn: 01 03 00 00 00 0A -> CRC C5 CD */
    const uint8_t f[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
    CHECK(mb_crc16(f, 6) == 0xCDC5);
}

static void test_read_keys(void)
{
    regs_init();
    regs_set_key_state(0x8001);
    regs_add_key_events(3);
    uint8_t f[16] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x02};
    uint16_t n = send(f, 6);
    CHECK(n == 9);
    CHECK(crc_ok(n));
    CHECK(resp[0] == 1 && resp[1] == 3 && resp[2] == 4);
    CHECK(resp[3] == 0x80 && resp[4] == 0x01);
    CHECK(resp[5] == 0x00 && resp[6] == 0x03);
}

static void test_read_all_and_out_of_range(void)
{
    regs_init();
    uint8_t f[16] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x05};
    CHECK(send(f, 6) == 15);

    uint8_t g[16] = {0x01, 0x03, 0x00, 0x04, 0x00, 0x02};  /* 40005..40006 */
    uint16_t n = send(g, 6);
    CHECK(n == 5 && resp[1] == 0x83 && resp[2] == MB_EX_ILLEGAL_DATA_ADDRESS);

    uint8_t h[16] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x00};  /* qty = 0 */
    n = send(h, 6);
    CHECK(n == 5 && resp[1] == 0x83 && resp[2] == MB_EX_ILLEGAL_DATA_VALUE);
}

static void test_write_single(void)
{
    regs_init();
    (void)regs_take_output_changed();

    uint8_t f[16] = {0x01, 0x06, 0x00, 0x02, 0x04, 0xD2};  /* 40003 = 1234 */
    uint16_t n = send(f, 6);
    CHECK(n == 8 && memcmp(resp, f, 8) == 0);
    CHECK(regs_get(REG_DISPLAY1) == 1234);
    CHECK(regs_take_output_changed() == 1);

    uint8_t g[16] = {0x01, 0x06, 0x00, 0x03, 0x27, 0x10};  /* 40004 = 10000 -> lỗi 03 */
    n = send(g, 6);
    CHECK(n == 5 && resp[1] == 0x86 && resp[2] == MB_EX_ILLEGAL_DATA_VALUE);
    CHECK(regs_get(REG_DISPLAY2) == 0);

    uint8_t h[16] = {0x01, 0x06, 0x00, 0x00, 0x00, 0x01};  /* ghi 40001 read-only */
    n = send(h, 6);
    CHECK(n == 5 && resp[1] == 0x86 && resp[2] == MB_EX_ILLEGAL_DATA_ADDRESS);

    uint8_t k[16] = {0x01, 0x06, 0x00, 0x01, 0x00, 0x01};  /* ghi 40002 read-only */
    n = send(k, 6);
    CHECK(n == 5 && resp[2] == MB_EX_ILLEGAL_DATA_ADDRESS);

    uint8_t l[16] = {0x01, 0x06, 0x00, 0x04, 0x12, 0x05};  /* LED: chỉ giữ byte thấp */
    n = send(l, 6);
    CHECK(n == 8);
    CHECK(regs_get(REG_LEDS) == 0x05);
}

static void test_write_multiple(void)
{
    regs_init();
    uint8_t f[32] = {0x01, 0x10, 0x00, 0x02, 0x00, 0x03, 0x06,
                     0x00, 0x07, 0x27, 0x0F, 0x00, 0xA5};  /* 7, 9999, 0xA5 */
    uint16_t n = send(f, 13);
    CHECK(n == 8 && crc_ok(n));
    CHECK(resp[1] == 0x10 && resp[3] == 0x02 && resp[5] == 0x03);
    CHECK(regs_get(REG_DISPLAY1) == 7 && regs_get(REG_DISPLAY2) == 9999 && regs_get(REG_LEDS) == 0xA5);

    /* Lệnh có 1 giá trị sai: không ghi gì cả */
    uint8_t g[32] = {0x01, 0x10, 0x00, 0x02, 0x00, 0x02, 0x04, 0x00, 0x01, 0xFF, 0xFF};
    n = send(g, 11);
    CHECK(n == 5 && resp[1] == 0x90 && resp[2] == MB_EX_ILLEGAL_DATA_VALUE);
    CHECK(regs_get(REG_DISPLAY1) == 7);

    /* Dải chứa 40002 read-only */
    uint8_t h[32] = {0x01, 0x10, 0x00, 0x01, 0x00, 0x02, 0x04, 0x00, 0x00, 0x00, 0x01};
    n = send(h, 11);
    CHECK(n == 5 && resp[2] == MB_EX_ILLEGAL_DATA_ADDRESS);

    /* byte count sai */
    uint8_t k[32] = {0x01, 0x10, 0x00, 0x02, 0x00, 0x02, 0x02, 0x00, 0x01};
    n = send(k, 9);
    CHECK(n == 5 && resp[2] == MB_EX_ILLEGAL_DATA_VALUE);
}

static void test_addressing(void)
{
    regs_init();
    uint8_t f[16] = {0x02, 0x03, 0x00, 0x00, 0x00, 0x01};  /* slave khác */
    CHECK(send(f, 6) == 0);

    uint8_t g[16] = {0x00, 0x06, 0x00, 0x03, 0x00, 0x2A};  /* broadcast ghi: không trả lời */
    CHECK(send(g, 6) == 0);
    CHECK(regs_get(REG_DISPLAY2) == 42);

    uint8_t h[16] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00};  /* sai CRC */
    CHECK(mb_slave_process(1, h, 8, resp) == 0);

    uint8_t k[16] = {0x01, 0x05, 0x00, 0x00, 0xFF, 0x00};  /* FC05 không hỗ trợ */
    uint16_t n = send(k, 6);
    CHECK(n == 5 && resp[1] == 0x85 && resp[2] == MB_EX_ILLEGAL_FUNCTION);
}

static void test_key_decode(void)
{
    uint8_t none[4] = {0, 0, 0, 0};
    CHECK(panel_decode_keys(none) == 0);

    /* SW1 = KS1/K1 -> byte0 bit2 */
    uint8_t sw1[4] = {0x04, 0, 0, 0};
    CHECK(panel_decode_keys(sw1) == 0x0001);

    /* SW10 = KS2/K2 -> byte0 bit5 */
    uint8_t sw10[4] = {0x20, 0, 0, 0};
    CHECK(panel_decode_keys(sw10) == 0x0200);

    /* SW8 = KS8/K1 -> byte3 bit6; SW16 = KS8/K2 -> byte3 bit5 */
    uint8_t sw8_16[4] = {0, 0, 0, 0x60};
    CHECK(panel_decode_keys(sw8_16) == 0x8080);

    /* SW1, SW9, SW2, SW10 cùng lúc (hình chữ nhật) -> mơ hồ */
    uint8_t ghost[4] = {0x66, 0, 0, 0};
    CHECK(panel_decode_keys(ghost) == KEY_GHOST);
}

static void test_display(void)
{
    uint8_t ram[16];
    panel_build_ram(1234, 7, 0x05, ram);
    CHECK(ram[0] == 0x06 && ram[2] == 0x5B && ram[4] == 0x4F && ram[6] == 0x66);
#if DISPLAY_LEADING_ZERO_BLANK
    CHECK(ram[8] == 0 && ram[10] == 0 && ram[12] == 0 && ram[14] == 0x07);
#endif
    CHECK(ram[1] == 1 && ram[3] == 0 && ram[5] == 1 && ram[7] == 0 && ram[15] == 0);

    panel_build_ram(0, 9999, 0x80, ram);
    CHECK(ram[6] == 0x3F);  /* 0 vẫn hiện chữ số cuối */
    CHECK(ram[8] == 0x6F && ram[14] == 0x6F);
    CHECK(ram[15] == 1);
}

int main(void)
{
    test_crc();
    test_read_keys();
    test_read_all_and_out_of_range();
    test_write_single();
    test_write_multiple();
    test_addressing();
    test_key_decode();
    test_display();
    if (failures) {
        printf("%d test(s) FAILED\n", failures);
        return 1;
    }
    printf("All tests passed\n");
    return 0;
}
