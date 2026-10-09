/* crc, datafmt, rtcfg, storage */
#include "testlib.h"
#include "fake_hw.h"
#include "crc.h"
#include "datafmt.h"
#include "rtcfg.h"
#include "storage.h"
#include "cfg.h"
#include <math.h>
#include <stdlib.h>

static void test_crc(void)
{
    CHECK_EQ(crc16_modbus((const uint8_t *)"123456789", 9), 0x4B37);
    uint8_t req[] = { 0x01, 0x03, 0x00, 0x00, 0x00, 0x02 };
    CHECK_EQ(crc16_modbus(req, 6), 0x0BC4);                     /* 01 03 00 00 00 02 C4 0B */
    CHECK_EQ(crc32_update(0, (const uint8_t *)"123456789", 9), 0xCBF43926u);
    uint32_t a = crc32_update(0, (const uint8_t *)"12345", 5);
    CHECK_EQ(crc32_update(a, (const uint8_t *)"6789", 4), 0xCBF43926u);   /* tính tiếp được */
}

static rt_obj_t obj(uint8_t dt, uint8_t dec)
{
    rt_obj_t o;
    memset(&o, 0, sizeof o);
    o.type = RT_OBJ_SHOW;
    o.dtype = dt;
    o.decimals = dec;
    o.bit = RT_BIT_NONE;
    return o;
}

static void test_df_orders(void)
{
    rt_obj_t o = obj(RT_DT_U32, 0);
    uint16_t w[2];
    int32_t v;
    /* 0x00012345 = 74565 */
    for (uint8_t ord = RT_ORD_ABCD; ord <= RT_ORD_DCBA; ord++) {
        o.order = ord;
        CHECK(df_encode(&o, 1, 74565, w));
        CHECK(df_decode(&o, 1, w, &v));
        CHECK_EQ(v, 74565);
    }
    o.order = RT_ORD_ABCD;
    df_encode(&o, 1, 0x12345678, w);
    CHECK_EQ(w[0], 0x1234); CHECK_EQ(w[1], 0x5678);
    o.order = RT_ORD_CDAB;
    df_encode(&o, 1, 0x12345678, w);
    CHECK_EQ(w[0], 0x5678); CHECK_EQ(w[1], 0x1234);
    o.order = RT_ORD_BADC;
    df_encode(&o, 1, 0x12345678, w);
    CHECK_EQ(w[0], 0x3412); CHECK_EQ(w[1], 0x7856);
    o.order = RT_ORD_DCBA;
    df_encode(&o, 1, 0x12345678, w);
    CHECK_EQ(w[0], 0x7856); CHECK_EQ(w[1], 0x3412);
    /* order = 0 thì theo mặc định chung */
    o.order = RT_ORD_INHERIT;
    df_encode(&o, RT_ORD_CDAB, 0x12345678, w);
    CHECK_EQ(w[0], 0x5678);
}

static void test_df_types(void)
{
    uint16_t w[2] = { 0, 0 };
    int32_t v;
    rt_obj_t o = obj(RT_DT_U16, 0);
    CHECK(df_encode(&o, 1, 65535, w)); CHECK_EQ(w[0], 65535);
    CHECK(!df_encode(&o, 1, 65536, w));
    CHECK(!df_encode(&o, 1, -1, w));
    w[0] = 0xFFFF; CHECK(df_decode(&o, 1, w, &v)); CHECK_EQ(v, 65535);

    o = obj(RT_DT_I16, 0);
    CHECK(df_encode(&o, 1, -5, w)); CHECK_EQ(w[0], 0xFFFB);
    CHECK(df_decode(&o, 1, w, &v)); CHECK_EQ(v, -5);
    CHECK(!df_encode(&o, 1, 32768, w));
    CHECK(!df_encode(&o, 1, -32769, w));

    o = obj(RT_DT_I32, 0);
    CHECK(df_encode(&o, 1, -2, w)); CHECK_EQ(w[0], 0xFFFF); CHECK_EQ(w[1], 0xFFFE);
    CHECK(df_decode(&o, 1, w, &v)); CHECK_EQ(v, -2);
    o = obj(RT_DT_U32, 0);
    w[0] = 0xFFFF; w[1] = 0xFFFF; CHECK(df_decode(&o, 1, w, &v)); CHECK_EQ(v, INT32_MAX);   /* bão hoà */
    CHECK(!df_encode(&o, 1, -1, w));

    o = obj(RT_DT_BCD16, 0);
    w[0] = 0x1234; CHECK(df_decode(&o, 1, w, &v)); CHECK_EQ(v, 1234);
    w[0] = 0x12A4; CHECK(!df_decode(&o, 1, w, &v));
    CHECK(df_encode(&o, 1, 9876, w)); CHECK_EQ(w[0], 0x9876);
    CHECK(!df_encode(&o, 1, 10000, w));
    o = obj(RT_DT_BCD32, 0);
    CHECK(df_encode(&o, 1, 12345678, w)); CHECK_EQ(w[0], 0x1234); CHECK_EQ(w[1], 0x5678);
    CHECK(df_decode(&o, 1, w, &v)); CHECK_EQ(v, 12345678);

    /* bit trong thanh ghi, cuộn dây */
    o = obj(RT_DT_U16, 0); o.bit = 5;
    w[0] = 0x0020; CHECK(df_decode(&o, 1, w, &v)); CHECK_EQ(v, 1);
    w[0] = 0xFFDF; CHECK(df_decode(&o, 1, w, &v)); CHECK_EQ(v, 0);
    o = obj(RT_DT_U16, 0); o.area = RT_AREA_COIL;
    w[0] = 1; CHECK(df_decode(&o, 1, w, &v)); CHECK_EQ(v, 1);
}

static void test_df_gain(void)
{
    uint16_t w[2];
    int32_t v;
    rt_obj_t o = obj(RT_DT_I16, 1);
    o.gain = 500;                                   /* hiển thị = PLC x 0,5 */
    w[0] = 1001;
    CHECK(df_decode(&o, 1, w, &v)); CHECK_EQ(v, 501);      /* 500,5 làm tròn ra xa 0 */
    CHECK(df_encode(&o, 1, 250, w)); CHECK_EQ(w[0], 500);
    o.gain = -1000;
    w[0] = 10; CHECK(df_decode(&o, 1, w, &v)); CHECK_EQ(v, -10);
    o.gain = 2000;
    CHECK(df_encode(&o, 1, 100, w)); CHECK_EQ(w[0], 50);
}

static uint32_t f2b(float f) { uint32_t b; memcpy(&b, &f, 4); return b; }
static float b2f(uint32_t b) { float f; memcpy(&f, &b, 4); return f; }

static void test_df_float(void)
{
    rt_obj_t o = obj(RT_DT_F32, 1);
    o.order = RT_ORD_ABCD;
    uint16_t w[2];
    int32_t v;
    CHECK(df_encode(&o, 1, 125, w));
    CHECK_EQ(((uint32_t)w[0] << 16) | w[1], 0x41480000u);       /* 12,5 */
    CHECK(df_decode(&o, 1, w, &v)); CHECK_EQ(v, 125);
    CHECK(df_encode(&o, 1, -125, w));
    CHECK_EQ(((uint32_t)w[0] << 16) | w[1], 0xC1480000u);
    w[0] = 0x7FC0; w[1] = 0;
    CHECK(!df_decode(&o, 1, w, &v));                             /* NaN */
    w[0] = 0; w[1] = 0;
    CHECK(df_decode(&o, 1, w, &v)); CHECK_EQ(v, 0);
    CHECK(df_encode(&o, 1, 0, w)); CHECK_EQ(w[0] | w[1], 0);

    /* so với số học float trên PC (cho phép lệch 1 ulp ở phép chia) */
    srand(7);
    int bad = 0;
    for (int i = 0; i < 4000; i++) {
        int32_t disp = (int32_t)((rand() % 2000001) - 1000000);
        uint8_t dec = (uint8_t)(rand() % 4);
        int16_t gain = (rand() & 1) ? 0 : (int16_t)(100 + rand() % 3000);
        o = obj(RT_DT_F32, dec);
        o.gain = gain;
        o.order = RT_ORD_ABCD;
        double pw = (dec == 0) ? 1 : (dec == 1 ? 10 : (dec == 2 ? 100 : 1000));
        double g = gain ? gain : 1000;
        float want = (float)((double)disp * 1000.0 / g / pw);
        df_encode(&o, 1, disp, w);
        uint32_t got = ((uint32_t)w[0] << 16) | w[1];
        int64_t d = (int64_t)got - (int64_t)f2b(want);
        if (disp != 0 && llabs(d) > 1)
            bad++;
        /* vòng lặp ngược: float -> số hiển thị khớp số ban đầu khi không có hệ số */
        if (gain == 0 && disp != 0) {
            int32_t back;
            df_decode(&o, 1, w, &back);
            if (labs(back - disp) > (int32_t)(llabs(disp) / 8000000 + 1))
                bad++;
        }
    }
    CHECK_EQ(bad, 0);
    /* một số giá trị chính xác */
    float samples[] = { 1.0f, 0.5f, 100.25f, -3.75f, 123456.0f };
    for (unsigned i = 0; i < sizeof samples / sizeof samples[0]; i++) {
        o = obj(RT_DT_F32, 2);
        o.order = RT_ORD_ABCD;
        uint32_t b = f2b(samples[i]);
        w[0] = (uint16_t)(b >> 16); w[1] = (uint16_t)b;
        df_decode(&o, 1, w, &v);
        CHECK_EQ(v, (int32_t)lround(b2f(b) * 100.0));
    }
}

static void test_rtcfg(void)
{
    rt_blob_t b;
    CHECK_EQ(sizeof(rt_blob_t), 1918);
    rtcfg_defaults(&b);
    CHECK_EQ(rtcfg_validate(&b), RT_OK);
    CHECK_EQ(b.comm.mode, RT_MODE_SLAVE);
    CHECK_EQ(b.magic, RT_MAGIC);

    rt_blob_t c = b;
    c.comm.baud_idx = 9;
    CHECK_EQ(rtcfg_validate(&c), RT_E_CRC);                      /* chưa seal lại */
    rtcfg_seal(&c);
    CHECK_EQ(rtcfg_validate(&c), RT_E_COMM);

    c = b; c.magic = 0; CHECK_EQ(rtcfg_validate(&c), RT_E_MAGIC);
    c = b; c.schema = 2; CHECK_EQ(rtcfg_validate(&c), RT_E_SCHEMA);
    c = b; c.length = 100; CHECK_EQ(rtcfg_validate(&c), RT_E_LENGTH);

    c = b; c.comm.plc_id = 0; rtcfg_seal(&c); CHECK_EQ(rtcfg_validate(&c), RT_E_COMM);
    c = b; c.comm.timeout_ms = 5; rtcfg_seal(&c); CHECK_EQ(rtcfg_validate(&c), RT_E_COMM);
    c = b; c.comm.page_count = 40; rtcfg_seal(&c); CHECK_EQ(rtcfg_validate(&c), RT_E_COMM);
    c = b; c.comm.run_page = 3; rtcfg_seal(&c); CHECK_EQ(rtcfg_validate(&c), RT_E_COMM);
    c = b; c.sec.pwd_user = 10000; rtcfg_seal(&c); CHECK_EQ(rtcfg_validate(&c), RT_E_SEC);

    c = b; c.page[0].bot.type = RT_OBJ_INPUT; c.page[0].bot.area = RT_AREA_INPUT; rtcfg_seal(&c);
    CHECK_EQ(rtcfg_validate(&c), RT_E_PAGE);                     /* không ghi được vùng đầu vào */
    c = b; c.page[0].bot.type = RT_OBJ_INPUT; c.page[0].bot.min = 5; c.page[0].bot.max = 1; rtcfg_seal(&c);
    CHECK_EQ(rtcfg_validate(&c), RT_E_PAGE);
    c = b; c.page[0].bot.type = RT_OBJ_INPUT; c.page[0].bot.max = 100; rtcfg_seal(&c);
    CHECK_EQ(rtcfg_validate(&c), RT_OK);
    c = b; c.page[0].top.decimals = 4; c.page[0].top.type = RT_OBJ_SHOW; rtcfg_seal(&c);
    CHECK_EQ(rtcfg_validate(&c), RT_E_PAGE);
    c = b; c.page[5].top.type = 99; rtcfg_seal(&c);
    CHECK_EQ(rtcfg_validate(&c), RT_OK);                         /* trang ngoài page_count được bỏ qua */

    c = b; c.fkey[2].action = 99; rtcfg_seal(&c); CHECK_EQ(rtcfg_validate(&c), RT_E_FKEY);
    c = b; c.fkey[2].action = RT_FA_GOTO; c.fkey[2].value = 40; rtcfg_seal(&c); CHECK_EQ(rtcfg_validate(&c), RT_E_FKEY);
    c = b; c.led[1].mode = 9; rtcfg_seal(&c); CHECK_EQ(rtcfg_validate(&c), RT_E_LED);
    c = b; c.alarm[3].led = 9; rtcfg_seal(&c); CHECK_EQ(rtcfg_validate(&c), RT_E_ALARM);

    CHECK_EQ(rtcfg_baud(0), 2400);
    CHECK_EQ(rtcfg_baud(6), 115200);
    CHECK_EQ(rtcfg_baud(200), 9600);

    rt_obj_t o = obj(RT_DT_F32, 0);
    CHECK_EQ(rt_nregs(&o), 2);
    o.bit = 3; CHECK_EQ(rt_nregs(&o), 1);
    o = obj(RT_DT_U32, 0); o.area = RT_AREA_COIL; CHECK_EQ(rt_nregs(&o), 1);
}

static rt_blob_t sample(uint16_t rev)
{
    rt_blob_t b;
    rtcfg_defaults(&b);
    b.revision = rev;
    b.comm.timeout_ms = (uint16_t)(100 + rev);
    rtcfg_seal(&b);
    return b;
}

static void test_storage(void)
{
    fake_hw_reset_all();
    rt_blob_t out, a = sample(1), b = sample(2), c = sample(3);
    CHECK_EQ(storage_load(&out), ST_NONE);
    CHECK(storage_blank());
    CHECK_EQ(storage_save(&a), ST_OK);
    CHECK(!storage_blank());
    CHECK_EQ(storage_seq(), 1);
    memset(&out, 0, sizeof out);
    CHECK_EQ(storage_load(&out), ST_OK);
    CHECK_MEM(&out, &a, sizeof a);

    CHECK_EQ(storage_save(&b), ST_OK);
    CHECK_EQ(storage_seq(), 2);
    CHECK_EQ(storage_load(&out), ST_OK);
    CHECK_MEM(&out, &b, sizeof b);

    CHECK_EQ(storage_save(&c), ST_OK);                          /* quay lại trang đầu */
    CHECK_EQ(storage_load(&out), ST_OK);
    CHECK_MEM(&out, &c, sizeof c);
    CHECK_EQ(storage_seq(), 3);

    /* hỏng bản mới nhất -> quay về bản liền trước */
    uint32_t newest_bank = STORAGE_BANK_A;                      /* seq 3 nằm ở trang A (1->A, 2->B, 3->A) */
    fake_flash_corrupt(newest_bank + 100, 0x01);
    CHECK_EQ(storage_load(&out), ST_OK);
    CHECK_MEM(&out, &b, sizeof b);
    CHECK_EQ(storage_seq(), 2);
    fake_flash_corrupt(STORAGE_BANK_B + 200, 0x80);
    CHECK_EQ(storage_load(&out), ST_NONE);
    CHECK(!storage_blank());                                   /* hỏng chứ không phải trống */
}

static void test_storage_power_cut(void)
{
    /* cắt điện ở mọi bước của một lần lưu: luôn đọc lại được bản cũ hoặc bản mới, không bao giờ rác */
    for (int cut = 0; cut < 12; cut++) {
        fake_hw_reset_all();
        rt_blob_t a = sample(1), b = sample(2), out;
        CHECK_EQ(storage_save(&a), ST_OK);
        fake_flash_power_cut_after(cut);
        st_result_t r = storage_save(&b);
        fake_flash_power_cut_after(-1);
        st_result_t l = storage_load(&out);
        CHECK_EQ(l, ST_OK);
        if (r == ST_OK)
            CHECK_MEM(&out, &b, sizeof b);
        else
            CHECK(memcmp(&out, &a, sizeof a) == 0 || memcmp(&out, &b, sizeof b) == 0);
        /* sau đó vẫn lưu được */
        rt_blob_t c = sample(3);
        CHECK_EQ(storage_save(&c), ST_OK);
        CHECK_EQ(storage_load(&out), ST_OK);
        CHECK_MEM(&out, &c, sizeof c);
    }
}

static void test_cfg_load(void)
{
    fake_hw_reset_all();
    CHECK(!cfg_load());
    CHECK_EQ(cfg_load_status(), 1);                              /* trống */
    CHECK_EQ(cfg()->comm.mode, RT_MODE_SLAVE);
    cfg_mut()->comm.timeout_ms = 777;
    cfg_touch();
    CHECK(cfg_dirty());
    CHECK_EQ(cfg_save(), 0);
    CHECK(!cfg_dirty());
    cfg_mut()->comm.timeout_ms = 5;                              /* sai phạm vi */
    CHECK_EQ(cfg_save(), RT_E_COMM);
    CHECK(cfg_load());
    CHECK_EQ(cfg()->comm.timeout_ms, 777);
    fake_flash_corrupt(STORAGE_BANK_A + 300, 0x10);
    fake_flash_corrupt(STORAGE_BANK_B + 300, 0x10);
    CHECK(!cfg_load());
    CHECK_EQ(cfg_load_status(), 2);                              /* hỏng */
}

void suite_basic(void)
{
    RUN(test_crc);
    RUN(test_df_orders);
    RUN(test_df_types);
    RUN(test_df_gain);
    RUN(test_df_float);
    RUN(test_rtcfg);
    RUN(test_storage);
    RUN(test_storage_power_cut);
    RUN(test_cfg_load);
}
