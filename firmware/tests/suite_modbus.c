/* mb_slave, cfgsrv (bản đồ thanh ghi + giao thức cấu hình), mb_master với PLC giả */
#include "testlib.h"
#include "fake_hw.h"
#include "crc.h"
#include "mb_slave.h"
#include "mb_master.h"
#include "cfgsrv.h"
#include "cfg.h"
#include "buzzer.h"
#include "rtcfg.h"
#include "keys.h"
#include "rs485.h"
#include "storage.h"

/* ------------------------------------------------------------------ tiện ích */
static uint16_t seal(uint8_t *f, uint16_t n)
{
    uint16_t c = crc16_modbus(f, n);
    f[n] = (uint8_t)c;
    f[n + 1] = (uint8_t)(c >> 8);
    return (uint16_t)(n + 2);
}

static uint16_t mkreq(uint8_t *f, uint8_t id, uint8_t fn, uint16_t addr, uint16_t v)
{
    f[0] = id; f[1] = fn;
    f[2] = (uint8_t)(addr >> 8); f[3] = (uint8_t)addr;
    f[4] = (uint8_t)(v >> 8); f[5] = (uint8_t)v;
    return seal(f, 6);
}

static uint16_t mkreq16(uint8_t *f, uint8_t id, uint16_t addr, const uint16_t *w, uint16_t qty)
{
    f[0] = id; f[1] = 16;
    f[2] = (uint8_t)(addr >> 8); f[3] = (uint8_t)addr;
    f[4] = (uint8_t)(qty >> 8); f[5] = (uint8_t)qty;
    f[6] = (uint8_t)(qty * 2);
    for (uint16_t i = 0; i < qty; i++) {
        f[7 + 2 * i] = (uint8_t)(w[i] >> 8);
        f[8 + 2 * i] = (uint8_t)w[i];
    }
    return seal(f, (uint16_t)(7 + qty * 2));
}

/* ------------------------------------------------------------------ mb_slave với bộ nhớ đơn giản */
static uint16_t S_regs[300];
static uint8_t  S_coils[300];
static int S_bad_reg = -1;

static uint8_t s_rd(uint16_t a, uint16_t *v, bool in) { (void)in; if (a >= 300) return 2; *v = S_regs[a]; return 0; }
static uint8_t s_chk(uint16_t a, uint16_t v) { (void)v; return (a >= 300 || (int)a == S_bad_reg) ? 2 : 0; }
static uint8_t s_wr(uint16_t a, uint16_t v) { S_regs[a] = v; return 0; }
static uint8_t s_rdb(uint16_t a, uint8_t *v, bool d) { (void)d; if (a >= 300) return 2; *v = S_coils[a]; return 0; }
static uint8_t s_wrb(uint16_t a, uint8_t v) { if (a >= 300) return 2; S_coils[a] = v; return 0; }
static const mbs_ops_t S_ops = { s_rd, s_chk, s_wr, s_rdb, s_wrb };

static void test_slave_read(void)
{
    uint8_t rq[16], rs[MB_MAX_ADU];
    memset(S_regs, 0, sizeof S_regs);
    S_regs[0] = 0x1234; S_regs[1] = 0xABCD;
    uint16_t n = mkreq(rq, 1, 3, 0, 2);
    uint8_t exp_rq[] = { 0x01, 0x03, 0x00, 0x00, 0x00, 0x02, 0xC4, 0x0B };
    CHECK_MEM(rq, exp_rq, 8);
    uint16_t rl = mbs_process(1, rq, n, rs, &S_ops);
    CHECK_EQ(rl, 9);
    CHECK_EQ(rs[0], 1); CHECK_EQ(rs[1], 3); CHECK_EQ(rs[2], 4);
    CHECK_EQ(rs[3], 0x12); CHECK_EQ(rs[4], 0x34); CHECK_EQ(rs[5], 0xAB); CHECK_EQ(rs[6], 0xCD);
    CHECK_EQ(crc16_modbus(rs, rl), 0);                          /* CRC của khung đã có CRC = 0 */

    n = mkreq(rq, 1, 4, 1, 1);
    rl = mbs_process(1, rq, n, rs, &S_ops);
    CHECK_EQ(rl, 7); CHECK_EQ(rs[1], 4); CHECK_EQ(rs[3], 0xAB);

    /* tối đa 125 thanh ghi */
    n = mkreq(rq, 1, 3, 0, 125);
    rl = mbs_process(1, rq, n, rs, &S_ops);
    CHECK_EQ(rl, 3 + 250 + 2);
    n = mkreq(rq, 1, 3, 0, 126);
    rl = mbs_process(1, rq, n, rs, &S_ops);
    CHECK_EQ(rs[1], 0x83); CHECK_EQ(rs[2], 3);
    n = mkreq(rq, 1, 3, 0, 0);
    rl = mbs_process(1, rq, n, rs, &S_ops);
    CHECK_EQ(rs[1], 0x83); CHECK_EQ(rs[2], 3);
    n = mkreq(rq, 1, 3, 299, 2);                                /* ra ngoài vùng */
    rl = mbs_process(1, rq, n, rs, &S_ops);
    CHECK_EQ(rs[1], 0x83); CHECK_EQ(rs[2], 2);
    n = mkreq(rq, 1, 3, 0xFFFF, 2);                             /* tràn 65536 */
    rl = mbs_process(1, rq, n, rs, &S_ops);
    CHECK_EQ(rs[2], 2);
}

static void test_slave_write(void)
{
    uint8_t rq[300], rs[MB_MAX_ADU];
    memset(S_regs, 0, sizeof S_regs);
    uint16_t n = mkreq(rq, 1, 6, 10, 0xBEEF);
    uint16_t rl = mbs_process(1, rq, n, rs, &S_ops);
    CHECK_EQ(rl, 8); CHECK_MEM(rs, rq, 8);                      /* echo */
    CHECK_EQ(S_regs[10], 0xBEEF);

    uint16_t w[3] = { 1, 2, 3 };
    n = mkreq16(rq, 1, 20, w, 3);
    rl = mbs_process(1, rq, n, rs, &S_ops);
    CHECK_EQ(rl, 8); CHECK_EQ(rs[1], 16); CHECK_EQ(rs[5], 3);
    CHECK_EQ(S_regs[20], 1); CHECK_EQ(S_regs[22], 3);

    /* một giá trị sai thì không thanh ghi nào bị đổi */
    S_bad_reg = 32;
    uint16_t w2[3] = { 7, 8, 9 };
    n = mkreq16(rq, 1, 30, w2, 3);
    rl = mbs_process(1, rq, n, rs, &S_ops);
    CHECK_EQ(rs[1], 0x90); CHECK_EQ(rs[2], 2);
    CHECK_EQ(S_regs[30], 0); CHECK_EQ(S_regs[31], 0);
    S_bad_reg = -1;

    /* sai số byte / số lượng */
    n = mkreq16(rq, 1, 30, w2, 3);
    rq[6] = 5; n = seal(rq, (uint16_t)(n - 2));
    rl = mbs_process(1, rq, n, rs, &S_ops);
    CHECK_EQ(rs[1], 0x90); CHECK_EQ(rs[2], 3);
    uint16_t big[124] = { 0 };
    n = mkreq16(rq, 1, 0, big, 124);
    rl = mbs_process(1, rq, n, rs, &S_ops);
    CHECK_EQ(rs[2], 3);
    n = mkreq16(rq, 1, 0, big, 123);
    rl = mbs_process(1, rq, n, rs, &S_ops);
    CHECK_EQ(rs[1], 16);
}

static void test_slave_coils(void)
{
    uint8_t rq[300], rs[MB_MAX_ADU];
    memset(S_coils, 0, sizeof S_coils);
    uint16_t n = mkreq(rq, 1, 5, 3, 0xFF00);
    uint16_t rl = mbs_process(1, rq, n, rs, &S_ops);
    CHECK_EQ(rl, 8); CHECK_EQ(S_coils[3], 1);
    n = mkreq(rq, 1, 5, 3, 0x1234);
    rl = mbs_process(1, rq, n, rs, &S_ops);
    CHECK_EQ(rs[1], 0x85); CHECK_EQ(rs[2], 3);
    S_coils[0] = 1; S_coils[2] = 1; S_coils[9] = 1;
    n = mkreq(rq, 1, 1, 0, 10);
    rl = mbs_process(1, rq, n, rs, &S_ops);
    CHECK_EQ(rl, 7); CHECK_EQ(rs[2], 2);
    CHECK_EQ(rs[3], 0x0D); CHECK_EQ(rs[4], 0x02);               /* bit0, bit2, bit3 ; bit9 */
    n = mkreq(rq, 1, 2, 0, 2000);
    rl = mbs_process(1, rq, n, rs, &S_ops);
    CHECK_EQ(rs[1], 0x82); CHECK_EQ(rs[2], 2);                  /* 2000 bit từ 0 vượt vùng 300 */
    n = mkreq(rq, 1, 2, 0, 2001);
    rl = mbs_process(1, rq, n, rs, &S_ops);
    CHECK_EQ(rs[2], 3);

    /* FC15 */
    uint8_t f[16] = { 1, 15, 0, 50, 0, 10, 2, 0xA5, 0x02 };     /* 10 cuộn dây */
    n = seal(f, 9);
    rl = mbs_process(1, f, n, rs, &S_ops);
    CHECK_EQ(rl, 8); CHECK_EQ(rs[1], 15);
    CHECK_EQ(S_coils[50], 1); CHECK_EQ(S_coils[51], 0); CHECK_EQ(S_coils[52], 1); CHECK_EQ(S_coils[59], 1);
}

static void test_slave_misc(void)
{
    uint8_t rq[16], rs[MB_MAX_ADU];
    memset(S_regs, 0, sizeof S_regs);
    uint16_t n = mkreq(rq, 1, 0x2B, 0, 0);
    uint16_t rl = mbs_process(1, rq, n, rs, &S_ops);
    CHECK_EQ(rs[1], 0xAB); CHECK_EQ(rs[2], 1); CHECK_EQ(rl, 5);

    n = mkreq(rq, 1, 3, 0, 1);
    rq[7] ^= 0xFF;                                              /* CRC sai */
    CHECK_EQ(mbs_process(1, rq, n, rs, &S_ops), 0);
    n = mkreq(rq, 2, 3, 0, 1);                                  /* địa chỉ khác */
    CHECK_EQ(mbs_process(1, rq, n, rs, &S_ops), 0);
    n = mkreq(rq, 0, 3, 0, 1);                                  /* broadcast đọc: bỏ qua */
    CHECK_EQ(mbs_process(1, rq, n, rs, &S_ops), 0);
    n = mkreq(rq, 0, 6, 5, 0x4242);                             /* broadcast ghi: thực hiện, không trả lời */
    CHECK_EQ(mbs_process(1, rq, n, rs, &S_ops), 0);
    CHECK_EQ(S_regs[5], 0x4242);
    CHECK_EQ(mbs_process(1, rq, 3, rs, &S_ops), 0);             /* quá ngắn */
    n = mkreq(rq, 1, 3, 0, 1);
    n = seal(rq, (uint16_t)(n - 2 + 1));                        /* dư 1 byte: độ dài sai */
    rl = mbs_process(1, rq, n, rs, &S_ops);
    CHECK_EQ(rs[1], 0x83); CHECK_EQ(rs[2], 3);
}

/* ------------------------------------------------------------------ cfgsrv */
static uint16_t rd1(uint16_t a)
{
    uint8_t rq[16], rs[MB_MAX_ADU];
    uint16_t n = mkreq(rq, 5, 3, a, 1);
    uint16_t rl = mbs_process(5, rq, n, rs, cfgsrv_ops());
    CHECK_EQ(rl, 7);
    return (uint16_t)((rs[3] << 8) | rs[4]);
}

static uint8_t wr1(uint16_t a, uint16_t v)          /* trả về exception (0 = OK) */
{
    uint8_t rq[16], rs[MB_MAX_ADU];
    uint16_t n = mkreq(rq, 5, 6, a, v);
    uint16_t rl = mbs_process(5, rq, n, rs, cfgsrv_ops());
    return (rs[1] & 0x80) ? rs[2] : (rl == 8 ? 0 : 0xFF);
}

static void cfg_fresh(void)
{
    fake_hw_reset_all();
    cfg_load();
    buzzer_init(7);
    cfgsrv_init();
}

static void test_cfgsrv_info(void)
{
    cfg_fresh();
    CHECK_EQ(rd1(REG_INFO), 0x504B);
    CHECK_EQ(rd1(REG_INFO + 1), FW_MODEL_ID);
    CHECK_EQ(rd1(REG_INFO + 2), (FW_VER_MAJOR << 8) | FW_VER_MINOR);
    CHECK_EQ(rd1(REG_INFO + 4), RT_SCHEMA);
    CHECK_EQ(rd1(REG_INFO + 5), 0x1234); CHECK_EQ(rd1(REG_INFO + 6), 0x5678);
    CHECK_EQ(rd1(REG_INFO + 8), 32);
    CHECK_EQ(rd1(REG_INFO + 10), 1918);
    CHECK_EQ(rd1(REG_INFO + 12) & 0x08, 0);                     /* chưa ở chế độ cấu hình */
    CHECK_EQ(rd1(7), 0); CHECK_EQ(rd1(8), 0); CHECK_EQ(rd1(9), 0);   /* chỗ trống đọc ra 0 */

    uint8_t rq[16], rs[MB_MAX_ADU];
    uint16_t n = mkreq(rq, 5, 3, 140, 1);                       /* vùng không có */
    mbs_process(5, rq, n, rs, cfgsrv_ops());
    CHECK_EQ(rs[1], 0x83); CHECK_EQ(rs[2], 2);
    n = mkreq(rq, 5, 3, 1200, 1);
    mbs_process(5, rq, n, rs, cfgsrv_ops());
    CHECK_EQ(rs[1], 0x83);

    CHECK_EQ(wr1(REG_INFO, 1), 2);                              /* chỉ đọc */
    CHECK_EQ(wr1(REG_KEY_CODE, 1), 2);
    CHECK_EQ(wr1(7, 1), 2);
    CHECK_EQ(wr1(130, 1), 2);
}

static void test_cfgsrv_slave_regs(void)
{
    cfg_fresh();
    slave_state_t *s = cfgsrv_state();
    cfgsrv_on_key(KC_5, 10);
    cfgsrv_on_key(KC_ENT, 4);
    CHECK_EQ(rd1(REG_KEY_CODE), KC_ENT);
    CHECK_EQ(rd1(REG_KEY_COUNT), 2);
    cfgsrv_set_keys(0x00FF0003);
    CHECK_EQ(rd1(REG_KEYS_LO), 3); CHECK_EQ(rd1(REG_KEYS_EXT), 0xFF);
    cfgsrv_set_entered(-123456, 7);
    CHECK_EQ(rd1(REG_VAL_HI), 0xFFFE); CHECK_EQ(rd1(REG_VAL_LO), 0x1DC0);
    CHECK_EQ(rd1(REG_FLAGS), (7u << 8) | FLG_NEW_VALUE);
    CHECK_EQ(wr1(REG_FLAGS, 0), 0);                             /* PLC xoá cờ */
    CHECK_EQ(rd1(REG_FLAGS), 0);

    CHECK_EQ(wr1(REG_TOP_HI, 0x0001), 0); CHECK_EQ(wr1(REG_TOP_LO, 0x0002), 0);
    CHECK_EQ(s->top, 0x00010002);
    CHECK_EQ(wr1(REG_BOT_LO, 1234), 0);
    CHECK_EQ(s->bot, 1234);
    CHECK_EQ(wr1(REG_LED, 0x0105), 0); CHECK_EQ(s->led, 0x0105);
    CHECK_EQ(wr1(REG_FMT, FMT_WIDE | 2), 0);
    CHECK_EQ(wr1(REG_RAW0 + 3, 0x7F), 0); CHECK_EQ(s->seg[3], 0x7F);
    CHECK_EQ(rd1(REG_RAW0 + 3), 0x7F);

    int on0 = fake_buzzer_on_count;
    CHECK_EQ(wr1(REG_CTRL, CTRL_BEEP_SHORT | 5), 0);
    CHECK_EQ(s->ctrl & 7, 5);
    CHECK(fake_buzzer_on_count > on0);                          /* bíp theo lệnh */
    CHECK_EQ(rd1(REG_CTRL) & CTRL_BEEP_SHORT, 0);               /* tự xoá */

    /* cuộn dây: LED */
    uint8_t rq[16], rs[MB_MAX_ADU];
    uint16_t n = mkreq(rq, 5, 5, 9, 0xFF00);
    mbs_process(5, rq, n, rs, cfgsrv_ops());
    CHECK_EQ(s->led & 0x0200, 0x0200);
    n = mkreq(rq, 5, 5, 40, 0xFF00);
    mbs_process(5, rq, n, rs, cfgsrv_ops());
    CHECK_EQ(rs[1], 0x85);
}

static void write_blob(const rt_blob_t *b)
{
    const uint8_t *p = (const uint8_t *)b;
    uint16_t regs = (uint16_t)((RT_BLOB_SIZE + 1) / 2);
    for (uint16_t off = 0; off < regs; off += 60) {
        uint16_t cnt = (uint16_t)((regs - off) > 60 ? 60 : regs - off);
        uint16_t w[60];
        for (uint16_t i = 0; i < cnt; i++) {
            uint16_t bi = (uint16_t)(2 * (off + i));
            w[i] = (uint16_t)((p[bi] << 8) | (bi + 1 < RT_BLOB_SIZE ? p[bi + 1] : 0));
        }
        uint8_t rq[200], rs[MB_MAX_ADU];
        uint16_t n = mkreq16(rq, 5, (uint16_t)(REG_WINDOW + off), w, cnt);
        uint16_t rl = mbs_process(5, rq, n, rs, cfgsrv_ops());
        CHECK_EQ(rl, 8);
    }
}

static void test_cfgsrv_config(void)
{
    cfg_fresh();
    rt_blob_t nb;
    rtcfg_defaults(&nb);
    nb.comm.mode = RT_MODE_MASTER;
    nb.comm.plc_id = 7;
    nb.comm.timeout_ms = 321;
    nb.revision = 9;
    nb.page[0].bot.type = RT_OBJ_INPUT;
    nb.page[0].bot.addr = 100;
    nb.page[0].bot.max = 5000;
    rtcfg_seal(&nb);

    /* đọc cửa sổ = khối cấu hình hiện tại (big-endian từng cặp byte) */
    CHECK_EQ(rd1(REG_WINDOW), 0x504B);
    CHECK_EQ(rd1(REG_WINDOW + 1), 0x5254);
    uint8_t rq[16], rs[MB_MAX_ADU];
    uint16_t n = mkreq(rq, 5, 3, REG_WINDOW, 60);
    uint16_t rl = mbs_process(5, rq, n, rs, cfgsrv_ops());
    CHECK_EQ(rl, 3 + 120 + 2);
    CHECK_MEM(&rs[3], cfg(), 120);

    /* ghi khi chưa ở chế độ cấu hình: bị từ chối */
    uint16_t w[1] = { 0 };
    n = mkreq16(rq, 5, REG_WINDOW, w, 1);
    mbs_process(5, rq, n, rs, cfgsrv_ops());
    CHECK_EQ(rs[1], 0x90); CHECK_EQ(rs[2], 4);
    CHECK_EQ(wr1(REG_CMD, CMD_APPLY), 0);
    CHECK_EQ(rd1(REG_CMD_RESULT), CR_NOT_CONFIG);

    CHECK_EQ(wr1(REG_CMD, CMD_ENTER_CONFIG), 0);
    CHECK(cfgsrv_config_mode());
    CHECK_EQ(rd1(REG_INFO + 12) & 0x08, 0x08);

    /* nạp khối hợp lệ rồi áp dụng */
    write_blob(&nb);
    CHECK(cfg_dirty());
    CHECK_EQ(wr1(REG_CMD, CMD_APPLY), 0);
    CHECK_EQ(rd1(REG_CMD_RESULT), 0);
    CHECK(!cfg_dirty());
    rt_blob_t chk;
    CHECK_EQ(storage_load(&chk), ST_OK);
    CHECK_MEM(&chk, &nb, sizeof nb);
    CHECK_EQ(cfg()->comm.plc_id, 7);
    CHECK_EQ(rd1(REG_INFO + 16), 1);                            /* seq */
    CHECK_EQ(rd1(REG_INFO + 17), 9);                            /* revision */

    /* khối sai CRC: từ chối, flash không đổi, RELOAD trả về bản đã lưu */
    rt_blob_t bad = nb;
    bad.comm.plc_id = 99;                                       /* không seal lại */
    write_blob(&bad);
    CHECK_EQ(wr1(REG_CMD, CMD_APPLY), 0);
    CHECK_EQ(rd1(REG_CMD_RESULT), RT_E_CRC);
    CHECK_EQ(rd1(REG_INFO + 14), RT_E_CRC);                     /* lỗi gần nhất */
    CHECK_EQ(storage_load(&chk), ST_OK);
    CHECK_MEM(&chk, &nb, sizeof nb);
    CHECK_EQ(wr1(REG_CMD, CMD_RELOAD), 0);
    CHECK_EQ(cfg()->comm.plc_id, 7);

    /* khối đúng CRC nhưng sai phạm vi */
    rt_blob_t bad2 = nb;
    bad2.comm.baud_idx = 12;
    rtcfg_seal(&bad2);
    write_blob(&bad2);
    CHECK_EQ(wr1(REG_CMD, CMD_APPLY), 0);
    CHECK_EQ(rd1(REG_CMD_RESULT), RT_E_COMM);

    /* mặc định nhà máy */
    CHECK_EQ(wr1(REG_CMD, CMD_FACTORY), 0);
    CHECK_EQ(rd1(REG_CMD_RESULT), 0);
    CHECK_EQ(cfg()->comm.mode, RT_MODE_SLAVE);
    CHECK_EQ(storage_load(&chk), ST_OK);
    CHECK_EQ(chk.comm.mode, RT_MODE_SLAVE);

    CHECK_EQ(wr1(REG_CMD, 99), 0);
    CHECK_EQ(rd1(REG_CMD_RESULT), CR_UNKNOWN);

    /* nhận diện + thoát */
    fake_now = 1000;
    CHECK_EQ(wr1(REG_CMD, CMD_IDENTIFY), 0);
    CHECK(cfgsrv_identify_active(1500));
    CHECK(!cfgsrv_identify_active(5000));
    CHECK(!cfgsrv_reset_due(1000));
    CHECK_EQ(wr1(REG_CMD, CMD_EXIT), 0);
    CHECK(!cfgsrv_reset_due(1050));
    CHECK(cfgsrv_reset_due(1100));
}

/* ------------------------------------------------------------------ master + PLC giả */
typedef struct { int calls; mbm_status_t st; uint8_t exc; uint8_t data[260]; uint8_t dlen; } cap_t;

static void cap_cb(void *ctx, mbm_status_t st, uint8_t exc, const uint8_t *d, uint8_t dl)
{
    cap_t *c = (cap_t *)ctx;
    c->calls++;
    c->st = st;
    c->exc = exc;
    c->dlen = dl;
    if (d && dl)
        memcpy(c->data, d, dl);
}

static void step(int ms)
{
    for (int i = 0; i < ms; i++) {
        fake_now++;
        mbm_poll(fake_now);
    }
}

static void master_fresh(uint16_t tmo, uint8_t retries)
{
    fake_hw_reset_all();
    fake_plc_enable(true);
    fake_plc_id = 3;
    mbm_init(3, tmo, retries);
}

static void test_master_basic(void)
{
    master_fresh(100, 2);
    fake_plc_hold[10] = 0x1111; fake_plc_hold[11] = 0x2222; fake_plc_hold[12] = 0x3333;
    fake_plc_coil[5] = 1; fake_plc_coil[7] = 1;
    cap_t c = { 0 };
    mbm_req_t r = { .fn = 3, .addr = 10, .qty = 3, .cb = cap_cb, .ctx = &c };
    CHECK(mbm_submit_urgent(&r));
    CHECK(!mbm_idle());
    step(30);
    CHECK_EQ(c.calls, 1); CHECK_EQ(c.st, MBM_OK); CHECK_EQ(c.dlen, 6);
    CHECK_EQ(c.data[0], 0x11); CHECK_EQ(c.data[5], 0x33);
    CHECK(mbm_idle());
    CHECK(mbm_comm_ok());

    r = (mbm_req_t){ .fn = 1, .addr = 4, .qty = 5, .cb = cap_cb, .ctx = &c };
    mbm_submit_urgent(&r);
    step(30);
    CHECK_EQ(c.st, MBM_OK); CHECK_EQ(c.dlen, 1); CHECK_EQ(c.data[0], 0x0A);   /* bit1 (5) và bit3 (7) */

    r = (mbm_req_t){ .fn = 6, .addr = 20, .data = { 0xCAFE }, .cb = cap_cb, .ctx = &c };
    mbm_submit_urgent(&r);
    step(30);
    CHECK_EQ(c.st, MBM_OK); CHECK_EQ(fake_plc_hold[20], 0xCAFE);

    r = (mbm_req_t){ .fn = 16, .addr = 30, .qty = 2, .data = { 0x0001, 0x86A0 }, .cb = cap_cb, .ctx = &c };
    mbm_submit_urgent(&r);
    step(30);
    CHECK_EQ(c.st, MBM_OK); CHECK_EQ(fake_plc_hold[30], 1); CHECK_EQ(fake_plc_hold[31], 0x86A0);

    r = (mbm_req_t){ .fn = 5, .addr = 9, .qty = 1, .cb = cap_cb, .ctx = &c };
    mbm_submit_urgent(&r);
    step(30);
    CHECK_EQ(c.st, MBM_OK); CHECK_EQ(fake_plc_coil[9], 1);
    r.qty = 0;
    mbm_submit_urgent(&r);
    step(30);
    CHECK_EQ(fake_plc_coil[9], 0);

    CHECK_EQ(mbm_stats()->ok, 6);
    /* khung gửi đi đúng chuẩn */
    uint8_t exp[] = { 3, 3, 0, 10, 0, 3 };
    CHECK_MEM(fake_tx_log[0], exp, 6);
    CHECK_EQ(crc16_modbus(fake_tx_log[0], fake_tx_len[0]), 0);
}

static void test_master_errors(void)
{
    /* PLC bỏ qua 2 yêu cầu đầu: thử lại thành công ở lần thứ 3 */
    master_fresh(50, 2);
    fake_plc_hold[1] = 7;
    fake_plc_drop_next = 2;
    cap_t c = { 0 };
    mbm_req_t r = { .fn = 3, .addr = 1, .qty = 1, .cb = cap_cb, .ctx = &c };
    mbm_submit_urgent(&r);
    step(400);
    CHECK_EQ(c.calls, 1); CHECK_EQ(c.st, MBM_OK);
    CHECK_EQ(fake_plc_requests, 3);
    CHECK_EQ(mbm_stats()->ok, 1);

    /* PLC tắt: sau retries+1 lần báo timeout */
    master_fresh(50, 2);
    fake_plc_online = false;
    c = (cap_t){ 0 };
    r.ctx = &c;
    mbm_submit_urgent(&r);
    step(100);
    CHECK_EQ(c.calls, 0);                                       /* đang thử lại */
    step(100);
    CHECK_EQ(c.calls, 1); CHECK_EQ(c.st, MBM_TIMEOUT);
    CHECK(!mbm_comm_ok());
    CHECK_EQ(fake_plc_requests, 3);
    CHECK_EQ(mbm_stats()->timeouts, 1);
    fake_plc_online = true;                                     /* hồi phục */
    mbm_submit_urgent(&r);
    step(40);
    CHECK_EQ(c.st, MBM_OK);
    CHECK(mbm_comm_ok());

    /* CRC phản hồi sai: thử lại */
    master_fresh(50, 2);
    fake_plc_corrupt_next = 1;
    c = (cap_t){ 0 };
    mbm_submit_urgent(&r);
    step(100);
    CHECK_EQ(c.st, MBM_OK);
    CHECK_EQ(mbm_stats()->crc_err, 0);                          /* lỗi trung gian không tính vào thống kê cuối */
    CHECK_EQ(fake_plc_requests, 2);

    /* retries = 0 */
    master_fresh(40, 0);
    fake_plc_online = false;
    c = (cap_t){ 0 };
    mbm_submit_urgent(&r);
    step(60);
    CHECK_EQ(c.st, MBM_TIMEOUT); CHECK_EQ(fake_plc_requests, 1);

    /* exception: không thử lại, vẫn coi là có liên lạc */
    master_fresh(50, 2);
    fake_plc_bad_addr_from = 5;
    c = (cap_t){ 0 };
    r = (mbm_req_t){ .fn = 3, .addr = 6, .qty = 1, .cb = cap_cb, .ctx = &c };
    mbm_submit_urgent(&r);
    step(40);
    CHECK_EQ(c.st, MBM_EXC); CHECK_EQ(c.exc, 2);
    CHECK_EQ(fake_plc_requests, 1);
    CHECK(mbm_comm_ok());
    CHECK_EQ(mbm_stats()->exceptions, 1);
    CHECK_EQ(mbm_stats()->last_exc, 2);
}

static void test_master_priority(void)
{
    master_fresh(50, 1);
    cap_t bg = { 0 }, u1 = { 0 }, u2 = { 0 };
    mbm_req_t rb = { .fn = 3, .addr = 1, .qty = 1, .cb = cap_cb, .ctx = &bg };
    CHECK(mbm_bg_free());
    CHECK(mbm_submit_bg(&rb));
    CHECK(!mbm_bg_free());
    CHECK(!mbm_submit_bg(&rb));                                 /* chỉ một khe nền */
    mbm_req_t ru = { .fn = 6, .addr = 2, .data = { 1 }, .cb = cap_cb, .ctx = &u1 };
    mbm_submit_urgent(&ru);
    ru.ctx = &u2; ru.addr = 3;
    mbm_submit_urgent(&ru);
    step(100);
    CHECK(u1.calls && u2.calls && bg.calls);
    /* khung thứ nhất phải là lệnh ghi ưu tiên */
    CHECK_EQ(fake_tx_log[0][1], 6); CHECK_EQ(fake_tx_log[1][1], 6); CHECK_EQ(fake_tx_log[2][1], 3);
    /* hàng đợi ưu tiên đầy */
    for (unsigned i = 0; i < MB_URGENT_DEPTH; i++)
        CHECK(mbm_submit_urgent(&ru));
    CHECK(!mbm_submit_urgent(&ru));
}

void suite_modbus(void)
{
    RUN(test_slave_read);
    RUN(test_slave_write);
    RUN(test_slave_coils);
    RUN(test_slave_misc);
    RUN(test_cfgsrv_info);
    RUN(test_cfgsrv_slave_regs);
    RUN(test_cfgsrv_config);
    RUN(test_master_basic);
    RUN(test_master_errors);
    RUN(test_master_priority);
}
