/* engine (chế độ master) + PLC giả */
#include "testlib.h"
#include "fake_hw.h"
#include "engine.h"
#include "mb_master.h"
#include "buzzer.h"
#include "cfg.h"
#include "rtcfg.h"

static void run(int ms)
{
    for (int i = 0; i < ms; i++) {
        fake_now++;
        mbm_poll(fake_now);
        engine_poll(fake_now);
        buzzer_poll(fake_now);
    }
}

static void obj_set(rt_obj_t *o, uint8_t type, uint8_t area, uint16_t addr, uint8_t dtype, uint8_t dec)
{
    memset(o, 0, sizeof *o);
    o->type = type;
    o->area = area;
    o->addr = addr;
    o->dtype = dtype;
    o->decimals = dec;
    o->bit = RT_BIT_NONE;
    o->min = -1000;
    o->max = 5000;
}

/* cấu hình thử: master, PLC trạm 2, 9600 */
static rt_blob_t *base_cfg(void)
{
    fake_hw_reset_all();
    cfg_load();
    rt_blob_t *b = cfg_mut();
    b->comm.mode = RT_MODE_MASTER;
    b->comm.plc_id = 2;
    b->comm.timeout_ms = 50;
    b->comm.retries = 1;
    b->comm.poll_ms = 10;
    b->comm.page_count = 3;
    b->comm.beep = RT_BF_CLICK | RT_BF_ERROR | RT_BF_ALARM;
    return b;
}

static void start(void)
{
    rtcfg_seal(cfg_mut());
    fake_plc_enable(true);
    fake_plc_id = 2;
    buzzer_init(cfg()->comm.beep);
    mbm_init(2, cfg()->comm.timeout_ms, cfg()->comm.retries);
    engine_init();
}

static void test_poll_merge(void)
{
    rt_blob_t *b = base_cfg();
    obj_set(&b->page[0].top, RT_OBJ_SHOW, RT_AREA_HOLD, 10, RT_DT_U16, 0);
    obj_set(&b->page[0].bot, RT_OBJ_INPUT, RT_AREA_HOLD, 11, RT_DT_I16, 1);
    start();
    fake_plc_hold[10] = 1234;
    fake_plc_hold[11] = (uint16_t)-55;
    engine_set_page(0);
    CHECK_EQ(engine_item_count(), 2);
    run(200);
    int32_t v;
    CHECK(engine_obj_value(&cfg()->page[0].top, &v)); CHECK_EQ(v, 1234);
    CHECK(engine_obj_value(&cfg()->page[0].bot, &v)); CHECK_EQ(v, -55);
    /* hai địa chỉ liền nhau -> một lệnh FC03 qty 2 */
    CHECK_EQ(fake_tx_log[0][1], 3);
    CHECK_EQ(fake_tx_log[0][3], 10); CHECK_EQ(fake_tx_log[0][5], 2);
    CHECK_EQ(fake_tx_log[1][1], 3); CHECK_EQ(fake_tx_log[1][5], 2);        /* chu kỳ kế tiếp */
    CHECK_EQ(engine_error(), ERR_NONE);
    CHECK(engine_comm_ok());

    /* giá trị đổi bên PLC được cập nhật */
    fake_plc_hold[10] = 999;
    run(100);
    CHECK(engine_obj_value(&cfg()->page[0].top, &v)); CHECK_EQ(v, 999);

    /* trang khác: danh sách hỏi vòng dựng lại, giữ cache cũ cho địa chỉ trùng */
    obj_set(&b->page[1].top, RT_OBJ_SHOW, RT_AREA_HOLD, 10, RT_DT_U16, 0);
    obj_set(&b->page[1].bot, RT_OBJ_SHOW, RT_AREA_HOLD, 40, RT_DT_U32, 2);
    engine_set_page(1);
    CHECK(engine_obj_value(&cfg()->page[1].top, &v)); CHECK_EQ(v, 999);    /* dùng ngay, chưa cần hỏi lại */
    CHECK(!engine_obj_value(&cfg()->page[1].bot, &v));                     /* chưa có dữ liệu */
    fake_plc_hold[40] = 0x0001; fake_plc_hold[41] = 0x0002;
    run(200);
    CHECK(engine_obj_value(&cfg()->page[1].bot, &v)); CHECK_EQ(v, 0x00010002);
    int before = fake_plc_requests;
    run(100);
    CHECK(fake_plc_requests > before);
}

static void test_poll_gap_groups(void)
{
    rt_blob_t *b = base_cfg();
    /* 10 và 12 (khoảng cách 1) gộp; 100 tách riêng; cuộn dây tách theo vùng */
    obj_set(&b->page[0].top, RT_OBJ_SHOW, RT_AREA_HOLD, 10, RT_DT_U16, 0);
    obj_set(&b->page[0].bot, RT_OBJ_SHOW, RT_AREA_HOLD, 12, RT_DT_U16, 0);
    b->led[0].mode = RT_LM_STEADY; b->led[0].area = RT_AREA_HOLD; b->led[0].addr = 100; b->led[0].bit = 3;
    b->led[1].mode = RT_LM_STEADY; b->led[1].area = RT_AREA_COIL; b->led[1].addr = 5; b->led[1].bit = RT_BIT_NONE;
    start();
    engine_set_page(0);
    CHECK_EQ(engine_item_count(), 4);
    fake_plc_hold[10] = 1; fake_plc_hold[11] = 77; fake_plc_hold[12] = 3; fake_plc_hold[100] = 8;
    fake_plc_coil[5] = 1;
    run(26);                                                    /* chỉ vòng hỏi đầu tiên (nhật ký giữ 16 khung gần nhất) */
    /* nhóm 1: hold 10..12 (qty 3); nhóm 2: hold 100; nhóm 3: coil 5 (sắp xếp theo vùng) */
    CHECK_EQ(fake_tx_log[0][1], 3); CHECK_EQ(fake_tx_log[0][3], 10); CHECK_EQ(fake_tx_log[0][5], 3);
    CHECK_EQ(fake_tx_log[1][1], 3); CHECK_EQ(fake_tx_log[1][3], 100); CHECK_EQ(fake_tx_log[1][5], 1);
    CHECK_EQ(fake_tx_log[2][1], 1); CHECK_EQ(fake_tx_log[2][3], 5);
    run(300);
    CHECK_EQ(engine_led_on(), 0x03);                            /* bit3 của 8 = 1; cuộn dây = 1 */
}

static void test_bits_leds_comm(void)
{
    rt_blob_t *b = base_cfg();
    b->led[0].mode = RT_LM_COMM;
    b->led[1].mode = RT_LM_STEADY; b->led[1].area = RT_AREA_COIL; b->led[1].addr = 1;
    b->led[2].mode = RT_LM_BLINK;  b->led[2].area = RT_AREA_COIL; b->led[2].addr = 2;
    b->led[3].mode = RT_LM_STEADY; b->led[3].area = RT_AREA_COIL; b->led[3].addr = 3; b->led[3].flags = RT_LF_INVERT;
    start();
    engine_set_page(0);
    fake_plc_coil[1] = 1; fake_plc_coil[2] = 1; fake_plc_coil[3] = 0;
    run(300);
    CHECK_EQ(engine_led_on() & 0x02, 0x02);
    CHECK_EQ(engine_led_blink(), 0x04);
    CHECK_EQ(engine_led_on() & 0x08, 0x08);                     /* đảo mức */
    uint8_t seen = 0;
    for (int i = 0; i < 60; i++) {
        run(7);
        seen |= (engine_led_on() & 1u) ? 1u : 2u;
    }
    CHECK_EQ(seen, 3);                                          /* LED COMM nhấp nháy theo giao dịch */
    fake_plc_coil[1] = 0;
    run(100);
    CHECK_EQ(engine_led_on() & 0x02, 0);

    /* mất liên lạc: E-CO, LED COMM tắt, LED khác giữ nguyên trạng thái cuối */
    fake_plc_online = false;
    run(300);
    CHECK_EQ(engine_error(), ERR_CO);
    CHECK(!engine_comm_ok());
    CHECK_EQ(engine_led_on() & 1, 0);
    CHECK_EQ(engine_led_blink(), 0x04);
    /* tự phục hồi */
    fake_plc_online = true;
    run(700);
    CHECK_EQ(engine_error(), ERR_NONE);
    CHECK(engine_comm_ok());
}

static void test_exception_error(void)
{
    rt_blob_t *b = base_cfg();
    obj_set(&b->page[0].top, RT_OBJ_SHOW, RT_AREA_HOLD, 10, RT_DT_U16, 0);
    obj_set(&b->page[0].bot, RT_OBJ_SHOW, RT_AREA_HOLD, 300, RT_DT_U16, 0);   /* địa chỉ ngoài vùng PLC giả */
    start();
    fake_plc_bad_addr_from = 200;
    engine_set_page(0);
    run(500);
    CHECK_EQ(engine_error(), ERR_AD);
    CHECK(engine_comm_ok());                                    /* vẫn có phản hồi */
    int32_t v;
    fake_plc_hold[10] = 5;
    run(200);
    CHECK(engine_obj_value(&cfg()->page[0].top, &v)); CHECK_EQ(v, 5);          /* nhóm tốt vẫn cập nhật */
    CHECK_EQ(engine_error(), ERR_AD);                           /* lỗi không nhấp nháy giữa các nhóm */
    CHECK_EQ(strcmp(err_text(ERR_AD), "E-Ad"), 0);
    CHECK_EQ(strcmp(err_text(ERR_CO), "E-CO"), 0);
}

static void wait_write(wstate_t *st, uint8_t *err)
{
    for (int i = 0; i < 400; i++) {
        run(1);
        *st = engine_write_state(err);
        if (*st != W_BUSY)
            return;
    }
}

static void test_write_obj(void)
{
    rt_blob_t *b = base_cfg();
    obj_set(&b->page[0].top, RT_OBJ_INPUT, RT_AREA_HOLD, 20, RT_DT_I16, 0);
    obj_set(&b->page[0].bot, RT_OBJ_INPUT, RT_AREA_HOLD, 30, RT_DT_I32, 0);
    b->comm.order = RT_ORD_CDAB;
    start();
    engine_set_page(0);
    uint8_t err = 0;
    wstate_t st;
    CHECK_EQ(engine_write_state(&err), W_IDLE);

    CHECK(engine_write_obj(&cfg()->page[0].top, -5));
    CHECK(!engine_write_obj(&cfg()->page[0].top, 1));           /* đang bận */
    wait_write(&st, &err);
    CHECK_EQ(st, W_OK);
    CHECK_EQ(fake_plc_hold[20], 0xFFFB);
    CHECK_EQ(fake_buzzer_on_count > 0, 1);                      /* bíp xác nhận */
    engine_write_done();
    CHECK_EQ(engine_write_state(&err), W_IDLE);
    int32_t v;
    run(60);
    CHECK(engine_obj_value(&cfg()->page[0].top, &v)); CHECK_EQ(v, -5);

    /* 32 bit theo thứ tự word mặc định CDAB: thanh ghi thấp trước */
    CHECK(engine_write_obj(&cfg()->page[0].bot, 0x00012345));
    wait_write(&st, &err);
    CHECK_EQ(st, W_OK);
    CHECK_EQ(fake_plc_hold[30], 0x2345); CHECK_EQ(fake_plc_hold[31], 0x0001);
    engine_write_done();
    CHECK_EQ(fake_tx_log[fake_tx_count - 1 > 15 ? 15 : fake_tx_count - 1][1] != 0, 1);

    /* ghi lỗi PLC: exception 02 -> E-Ad */
    fake_plc_bad_addr_from = 0;
    cfg_mut()->page[0].top.addr = 20;
    CHECK(engine_write_obj(&cfg()->page[0].top, 1));
    wait_write(&st, &err);
    CHECK_EQ(st, W_ERR); CHECK_EQ(err, ERR_AD);                 /* FC06 vào vùng cấm: PLC giả báo ngoài địa chỉ */
    engine_write_done();
    fake_plc_bad_addr_from = -1;

    /* PLC không trả lời: E-CO */
    fake_plc_online = false;
    CHECK(engine_write_obj(&cfg()->page[0].top, 1));
    wait_write(&st, &err);
    CHECK_EQ(st, W_ERR); CHECK_EQ(err, ERR_CO);
    engine_write_done();
    fake_plc_online = true;

    /* giá trị ngoài phạm vi kiểu dữ liệu -> E-dA, không gửi gì */
    int req = fake_plc_requests;
    CHECK(engine_write_obj(&cfg()->page[0].top, 40000));        /* i16 */
    CHECK_EQ(engine_write_state(&err), W_ERR); CHECK_EQ(err, ERR_DA);
    CHECK_EQ(fake_plc_requests, req);
    engine_write_done();

    /* mất nguồn: không ghi */
    fake_power_ok = false;
    CHECK(engine_write_obj(&cfg()->page[0].top, 1));
    CHECK_EQ(engine_write_state(&err), W_ERR); CHECK_EQ(err, ERR_PF);
    engine_write_done();
    fake_power_ok = true;
}

static void test_write_verify_bits(void)
{
    rt_blob_t *b = base_cfg();
    obj_set(&b->page[0].top, RT_OBJ_INPUT, RT_AREA_HOLD, 20, RT_DT_U16, 0);
    b->page[0].top.flags = RT_OF_VERIFY;
    obj_set(&b->page[0].bot, RT_OBJ_INPUT, RT_AREA_HOLD, 21, RT_DT_U16, 0);
    b->page[0].bot.bit = 4;
    b->page[1].top.type = RT_OBJ_INPUT; b->page[1].top.area = RT_AREA_COIL; b->page[1].top.addr = 8; b->page[1].top.bit = RT_BIT_NONE;
    start();
    engine_set_page(0);
    uint8_t err = 0;
    wstate_t st;

    int req = fake_plc_requests;
    CHECK(engine_write_obj(&cfg()->page[0].top, 321));
    wait_write(&st, &err);
    CHECK_EQ(st, W_OK);
    CHECK(fake_plc_requests - req >= 2);                        /* ghi + đọc lại xác nhận */
    engine_write_done();

    /* bit trong thanh ghi: đọc - sửa - ghi, các bit khác giữ nguyên */
    fake_plc_hold[21] = 0x00A1;
    CHECK(engine_write_obj(&cfg()->page[0].bot, 1));
    wait_write(&st, &err);
    CHECK_EQ(st, W_OK);
    CHECK_EQ(fake_plc_hold[21], 0x00B1);
    engine_write_done();
    CHECK(engine_write_obj(&cfg()->page[0].bot, 0));
    wait_write(&st, &err);
    CHECK_EQ(fake_plc_hold[21], 0x00A1);
    engine_write_done();

    /* cuộn dây */
    cfg_mut()->comm.flags = RT_CF_VERIFY;                       /* đọc lại sau mọi lệnh ghi */
    CHECK(engine_write_obj(&cfg()->page[1].top, 1));
    wait_write(&st, &err);
    CHECK_EQ(st, W_OK); CHECK_EQ(fake_plc_coil[8], 1);
    engine_write_done();
    CHECK(engine_write_obj(&cfg()->page[1].top, 0));
    wait_write(&st, &err);
    CHECK_EQ(fake_plc_coil[8], 0);
    engine_write_done();
}

static void test_fkeys(void)
{
    rt_blob_t *b = base_cfg();
    rt_fkey_t *f = b->fkey;
    f[0] = (rt_fkey_t){ RT_FA_MOMENTARY, RT_FAREA_COIL, 50, 0, 0, 0 };
    f[1] = (rt_fkey_t){ RT_FA_TOGGLE, RT_FAREA_COIL, 51, 0, 0, 0 };
    f[2] = (rt_fkey_t){ RT_FA_SET, RT_FAREA_COIL, 52, 0, 0, 0 };
    f[3] = (rt_fkey_t){ RT_FA_RESET, RT_FAREA_COIL, 52, 0, 0, 0 };
    f[4] = (rt_fkey_t){ RT_FA_WRITE, RT_FAREA_HOLDWORD, 60, 0, 4321, 0 };
    f[5] = (rt_fkey_t){ RT_FA_MOMENTARY, RT_FAREA_HOLDBIT, 61, 7, 0, 0 };
    f[6] = (rt_fkey_t){ RT_FA_TOGGLE, RT_FAREA_HOLDBIT, 62, 0, 0, 0 };
    f[7] = (rt_fkey_t){ RT_FA_GOTO, RT_FAREA_COIL, 99, 0, 1, 0 };      /* do giao diện xử lý */
    start();
    engine_set_page(0);
    run(20);

    engine_fkey(0, true);
    run(60);
    CHECK_EQ(fake_plc_coil[50], 1);
    engine_fkey(0, false);
    run(60);
    CHECK_EQ(fake_plc_coil[50], 0);

    engine_fkey(1, true); run(80); CHECK_EQ(fake_plc_coil[51], 1);
    engine_fkey(1, false); run(30); CHECK_EQ(fake_plc_coil[51], 1);        /* thả không làm gì */
    engine_fkey(1, true); run(80); CHECK_EQ(fake_plc_coil[51], 0);

    engine_fkey(2, true); run(40); CHECK_EQ(fake_plc_coil[52], 1);
    engine_fkey(3, true); run(40); CHECK_EQ(fake_plc_coil[52], 0);

    engine_fkey(4, true); run(40); CHECK_EQ(fake_plc_hold[60], 4321);

    fake_plc_hold[61] = 0x0003;
    engine_fkey(5, true); run(80);
    CHECK_EQ(fake_plc_hold[61], 0x0083);
    engine_fkey(5, false); run(80);
    CHECK_EQ(fake_plc_hold[61], 0x0003);

    fake_plc_hold[62] = 0x0000;
    engine_fkey(6, true); run(80); CHECK_EQ(fake_plc_hold[62], 0x0001);
    engine_fkey(6, true); run(80); CHECK_EQ(fake_plc_hold[62], 0x0000);

    int req = fake_plc_requests;
    engine_fkey(7, true); run(40);                              /* GOTO: engine bỏ qua */
    CHECK_EQ(fake_plc_coil[99], 0);
    engine_fkey(9, true); run(40);                              /* phím chưa cấu hình */
    CHECK(fake_plc_requests >= req);

    /* mất nguồn: không ghi */
    fake_power_ok = false;
    engine_fkey(2, true);
    run(60);
    fake_power_ok = true;
    CHECK_EQ(fake_plc_coil[52], 0);
}

static void test_fkey_serialized_and_release(void)
{
    rt_blob_t *b = base_cfg();
    b->fkey[0] = (rt_fkey_t){ RT_FA_TOGGLE, RT_FAREA_HOLDBIT, 70, 3, 0, 0 };
    b->fkey[1] = (rt_fkey_t){ RT_FA_MOMENTARY, RT_FAREA_COIL, 71, 0, 0, 0 };
    start();
    engine_set_page(0);
    run(20);

    /* hai lần đảo liên tiếp rất nhanh: không được mất lần nào (mỗi lệnh đọc-sửa-ghi chạy nối tiếp) */
    fake_plc_hold[70] = 0;
    engine_fkey(0, true);
    run(3);                                     /* lệnh đầu đang bay */
    engine_fkey(0, true);
    run(300);
    CHECK_EQ(fake_plc_hold[70], 0);

    /* thả không có nhấn trước: không ghi gì */
    int req = fake_plc_requests;
    engine_fkey(1, false);
    run(100);
    CHECK_EQ(fake_plc_requests, req);

    /* nhấn rồi thả sát nhau: đúng thứ tự, cuối cùng về 0 */
    engine_fkey(1, true);
    engine_fkey(1, false);
    run(200);
    CHECK_EQ(fake_plc_coil[71], 0);

    /* cấu hình sai: Toggle trên cả thanh ghi bị từ chối */
    rt_blob_t bad = *cfg();
    bad.fkey[2] = (rt_fkey_t){ RT_FA_SET, RT_FAREA_HOLDWORD, 80, 0, 0, 0 };
    rtcfg_seal(&bad);
    CHECK_EQ(rtcfg_validate(&bad), RT_E_FKEY);
}

static void test_alarms(void)
{
    rt_blob_t *b = base_cfg();
    b->alarm[0] = (rt_alarm_t){ RT_AREA_COIL, 70, RT_BIT_NONE, RT_AF_BEEP | RT_AF_ACK | RT_AF_LEDBLINK, 6, 90 };
    b->alarm[1] = (rt_alarm_t){ RT_AREA_HOLD, 71, 2, RT_AF_BEEP, 0, 0 };
    start();
    engine_set_page(0);
    run(200);
    CHECK_EQ(engine_alarms_active(), 0);
    CHECK(!fake_buzzer);

    fake_plc_coil[70] = 1;
    run(300);
    CHECK_EQ(engine_alarms_active(), 1);
    CHECK_EQ(engine_alarms_pending(), 1);
    CHECK_EQ(engine_led_blink(), 0x20);                         /* LED 6 nhấp nháy */
    int on0 = fake_buzzer_on_count;
    run(2000);
    CHECK(fake_buzzer_on_count - on0 >= 2);                     /* còi kêu lặp */

    /* xác nhận: ghi 1 vào cuộn dây ack, hết kêu, LED ngừng nháy */
    CHECK(engine_alarm_ack());
    run(100);
    CHECK_EQ(fake_plc_coil[90], 1);
    CHECK_EQ(engine_alarms_pending(), 0);
    CHECK_EQ(engine_led_blink(), 0);
    CHECK(!engine_alarm_ack());
    on0 = fake_buzzer_on_count;
    run(1500);
    CHECK_EQ(fake_buzzer_on_count, on0);

    /* hết báo rồi báo lại: phải yêu cầu xác nhận lại */
    fake_plc_coil[70] = 0;
    run(150);
    CHECK_EQ(engine_alarms_active(), 0);
    fake_plc_coil[70] = 1;
    run(150);
    CHECK_EQ(engine_alarms_pending(), 1);

    /* cảnh báo bit trong thanh ghi, không cần xác nhận: kêu khi còn báo */
    fake_plc_hold[71] = 0x0004;
    run(200);
    CHECK_EQ(engine_alarms_active(), 3);
    CHECK_EQ(engine_alarms_pending(), 3);
    CHECK(engine_alarm_ack());
    run(100);
    CHECK_EQ(engine_alarms_pending(), 2);                       /* cảnh báo 1 không có cờ ACK nên vẫn "đang chờ" */
    fake_plc_hold[71] = 0;
    run(150);
    CHECK_EQ(engine_alarms_active() & 2, 0);
}

static void test_page_change_inflight(void)
{
    /* đổi trang khi lệnh hỏi vòng đang bay không làm hỏng bộ nhớ đệm */
    rt_blob_t *b = base_cfg();
    obj_set(&b->page[0].top, RT_OBJ_SHOW, RT_AREA_HOLD, 10, RT_DT_U16, 0);
    obj_set(&b->page[0].bot, RT_OBJ_SHOW, RT_AREA_HOLD, 400, RT_DT_U16, 0);
    obj_set(&b->page[1].top, RT_OBJ_SHOW, RT_AREA_HOLD, 20, RT_DT_U16, 0);
    start();
    fake_plc_hold[10] = 11; fake_plc_hold[20] = 22;
    engine_set_page(0);
    for (int i = 0; i < 30; i++) {
        run(3);
        engine_set_page((uint8_t)(i & 1));
    }
    run(300);
    int32_t v;
    engine_set_page(1);
    run(100);
    CHECK(engine_obj_value(&cfg()->page[1].top, &v)); CHECK_EQ(v, 22);
    engine_set_page(0);
    run(100);
    CHECK(engine_obj_value(&cfg()->page[0].top, &v)); CHECK_EQ(v, 11);
}

void suite_engine(void)
{
    RUN(test_poll_merge);
    RUN(test_poll_gap_groups);
    RUN(test_bits_leds_comm);
    RUN(test_exception_error);
    RUN(test_write_obj);
    RUN(test_write_verify_bits);
    RUN(test_fkeys);
    RUN(test_fkey_serialized_and_release);
    RUN(test_alarms);
    RUN(test_page_change_inflight);
}
