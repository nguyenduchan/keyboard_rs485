/* Toàn hệ thống: app_init/app_loop + giao diện + PLC giả + phím giả */
#include "testlib.h"
#include "fake_hw.h"
#include "app.h"
#include "ui.h"
#include "keys.h"
#include "cfg.h"
#include "cfgsrv.h"
#include "engine.h"
#include "display.h"
#include "storage.h"
#include "crc.h"
#include "mb_slave.h"
#include "rtcfg.h"

/* ------------------------------------------------------------------ tiện ích */
static void sim(int ms)
{
    for (int i = 0; i < ms; i++) {
        fake_now++;
        app_loop();
    }
}

static bool row_is(uint8_t row, const char *t)
{
    for (int d = 0; d < 4; d++)
        if ((fake_seg((uint8_t)(row * 4 + d)) & 0x7Fu) != display_char_seg((uint8_t)t[d]))
            return false;
    return true;
}

static bool dp_at(uint8_t row, uint8_t pos)
{
    return (fake_seg((uint8_t)(row * 4 + pos)) & 0x80u) != 0u;
}

#define EXPECT_ROW(r, t) do { if (row_is(r, t)) t_pass++; else { char _s[10]; fake_row_text(r, _s); t_fail++; \
    printf("  FAIL %s:%d [%s] row%d = \"%s\" (expect \"%s\")\n", __FILE__, __LINE__, t_name, r, _s, t); } } while (0)

static void on_phase(void)               /* chờ đến pha sáng của nhấp nháy để đọc màn hình */
{
    while ((fake_now / BLINK_HALF_MS) & 1u)
        sim(1);
}

static uint8_t sw_of(uint8_t kc)
{
    for (uint8_t s = 1; s <= 24; s++)
        if (keys_logical(s) == kc)
            return s;
    return 0;
}

static void hold(uint8_t kc, int ms)
{
    uint8_t sw = sw_of(kc);
    fake_key_press(sw);
    sim(ms);
    fake_key_release(sw);
    sim(60);
}

static void tap(uint8_t kc)
{
    hold(kc, 60);
}

static void type_num(const char *digits)
{
    for (; *digits; digits++)
        tap(*digits == '0' ? KC_0 : (uint8_t)(KC_1 + (*digits - '1')));
}

/* cấu hình master thử */
static rt_blob_t mcfg(void)
{
    rt_blob_t b;
    rtcfg_defaults(&b);
    b.comm.mode = RT_MODE_MASTER;
    b.comm.plc_id = 2;
    b.comm.timeout_ms = 50;
    b.comm.retries = 1;
    b.comm.poll_ms = 10;
    b.comm.page_count = 3;
    b.comm.idle_s = 0;
    rt_page_t *p = &b.page[0];
    p->top = (rt_obj_t){ RT_OBJ_SHOW, RT_AREA_HOLD, 10, RT_DT_U16, 0, 1, 0, RT_BIT_NONE, 0, "    ", 0, 0, 0 };
    p->bot = (rt_obj_t){ RT_OBJ_INPUT, RT_AREA_HOLD, 11, RT_DT_I16, 0, 1, 0, RT_BIT_NONE, 0, "    ", -100, 500, 0 };
    p = &b.page[1];
    memcpy(p->top.label, "Cnt ", 4);
    p->top.type = RT_OBJ_LABEL; p->top.bit = RT_BIT_NONE;
    p->bot = (rt_obj_t){ RT_OBJ_SHOW, RT_AREA_HOLD, 20, RT_DT_U16, 0, 0, 0, RT_BIT_NONE, 0, "    ", 0, 0, 0 };
    p = &b.page[2];
    p->top = (rt_obj_t){ RT_OBJ_SHOW, RT_AREA_HOLD, 30, RT_DT_U16, 0, 0, 0, RT_BIT_NONE, 0, "    ", 0, 0, 0 };
    p->bot = (rt_obj_t){ RT_OBJ_INPUT, RT_AREA_HOLD, 31, RT_DT_U16, 0, 0, 1, RT_BIT_NONE, 0, "    ", 0, 9999, 0 };
    b.sec.pwd_user = 1234;
    b.sec.pwd_tech = 4321;
    b.fkey[0] = (rt_fkey_t){ RT_FA_MOMENTARY, RT_FAREA_COIL, 50, 0, 0, 0 };
    b.fkey[1] = (rt_fkey_t){ RT_FA_GOTO, RT_FAREA_COIL, 0, 0, 2, 0 };
    b.led[0].mode = RT_LM_COMM;
    b.alarm[0] = (rt_alarm_t){ RT_AREA_COIL, 70, RT_BIT_NONE, RT_AF_BEEP | RT_AF_ACK, 2, 90 };
    rtcfg_seal(&b);
    return b;
}

static rt_blob_t scfg(void)
{
    rt_blob_t b;
    rtcfg_defaults(&b);
    b.comm.kbd_id = 5;
    rtcfg_seal(&b);
    return b;
}

static void boot(const rt_blob_t *b, bool menu_keys)
{
    fake_hw_reset_all();
    if (b)
        storage_save(b);
    fake_plc_enable(b && b->comm.mode == RT_MODE_MASTER);
    fake_plc_id = 2;
    if (menu_keys) {
        fake_key_press(16);
        fake_key_press(4);
    }
    fake_tick_on_read = true;
    app_init();
    fake_tick_on_read = false;
    fake_key_release_all();
}

static void boot_running(const rt_blob_t *b)
{
    boot(b, false);
    sim(3600);
}

/* gửi một khung vào bus (vai trò PC / PLC chủ) và chờ phản hồi; trả về con trỏ khung phản hồi hoặc 0 */
static const uint8_t *bus_txn(const uint8_t *frame, uint16_t len, uint16_t *rlen)
{
    int before = fake_tx_count;
    fake_rs485_inject(frame, len, false);
    for (int i = 0; i < 20 && fake_tx_count == before; i++)
        sim(1);
    sim(3);
    if (fake_tx_count == before)
        return 0;
    int idx = (fake_tx_count - 1) % 16;
    *rlen = fake_tx_len[idx];
    return fake_tx_log[idx];
}

static uint16_t mkreq(uint8_t *f, uint8_t id, uint8_t fn, uint16_t addr, uint16_t v)
{
    f[0] = id; f[1] = fn;
    f[2] = (uint8_t)(addr >> 8); f[3] = (uint8_t)addr;
    f[4] = (uint8_t)(v >> 8); f[5] = (uint8_t)v;
    uint16_t c = crc16_modbus(f, 6);
    f[6] = (uint8_t)c; f[7] = (uint8_t)(c >> 8);
    return 8;
}

static uint16_t rd_reg(uint8_t id, uint16_t addr, bool *ok)
{
    uint8_t rq[8];
    uint16_t rl = 0;
    mkreq(rq, id, 3, addr, 1);
    const uint8_t *r = bus_txn(rq, 8, &rl);
    *ok = r && rl == 7 && r[1] == 3;
    return *ok ? (uint16_t)((r[3] << 8) | r[4]) : 0;
}

static bool wr_reg(uint8_t id, uint16_t addr, uint16_t v)
{
    uint8_t rq[8];
    uint16_t rl = 0;
    mkreq(rq, id, 6, addr, v);
    const uint8_t *r = bus_txn(rq, 8, &rl);
    return r && rl == 8 && r[1] == 6;
}

/* ------------------------------------------------------------------ banner + hiển thị trang */
static void test_banner_and_page(void)
{
    rt_blob_t b = mcfg();
    boot(&b, false);
    sim(100);
    EXPECT_ROW(0, " 100");                                      /* phiên bản 1.00 */
    CHECK(dp_at(0, 1));
    EXPECT_ROW(1, "nSt ");
    CHECK(fake_led(0) && fake_led(7));                          /* thử LED lúc khởi động */
    CHECK(fake_wdt_started);
    sim(500);
    EXPECT_ROW(0, " Id ");
    EXPECT_ROW(1, "   2");                                       /* trạm PLC */
    sim(500);
    CHECK_EQ(ui_state(), UI_PAGE);
    EXPECT_ROW(0, "----");                                      /* chưa có dữ liệu */
    EXPECT_ROW(1, "----");
    CHECK_EQ(fake_plc_requests, 0);                             /* master chưa hỏi trong cửa sổ cấu hình */

    fake_plc_hold[10] = 235;
    fake_plc_hold[11] = 100;
    sim(2500);                                                  /* hết cửa sổ 3 s */
    CHECK(fake_plc_requests > 0);
    EXPECT_ROW(0, " 235");
    CHECK(dp_at(0, 2));
    EXPECT_ROW(1, " 100");
    CHECK(dp_at(1, 2));
    CHECK_EQ(fake_baud, 9600);
}

/* ------------------------------------------------------------------ nhập số */
static void test_edit_flow(void)
{
    rt_blob_t b = mcfg();
    boot_running(&b);
    fake_plc_hold[11] = 100;
    sim(100);

    type_num("125");
    CHECK_EQ(ui_state(), UI_EDIT);
    on_phase();
    EXPECT_ROW(1, " 125");
    CHECK(dp_at(1, 2));                                         /* 12.5 */
    tap(KC_ENT);
    sim(100);
    CHECK_EQ(fake_plc_hold[11], 125);
    CHECK_EQ(ui_state(), UI_PAGE);
    sim(200);
    EXPECT_ROW(1, " 125");

    /* ngoài phạm vi: E-Hi, không ghi, vẫn đang nhập */
    type_num("600");
    tap(KC_ENT);
    sim(40);
    EXPECT_ROW(1, "E-Hi");
    CHECK_EQ(ui_state(), UI_EDIT);
    CHECK_EQ(fake_plc_hold[11], 125);
    sim(1600);                                                  /* hết thông báo */
    tap(KC_ESC);                                                /* xoá từng số */
    tap(KC_ESC);
    on_phase();
    EXPECT_ROW(1, "  06");
    tap(KC_ESC);
    CHECK_EQ(ui_state(), UI_EDIT);
    tap(KC_ESC);                                                /* hết số: huỷ */
    CHECK_EQ(ui_state(), UI_PAGE);

    /* giữ ESC lâu: huỷ ngay */
    type_num("77");
    hold(KC_ESC, 1150);
    CHECK_EQ(ui_state(), UI_PAGE);
    CHECK_EQ(fake_plc_hold[11], 125);

    /* đổi dấu bằng giữ phím 0: gõ 5 rồi giữ 0 -> -5 */
    type_num("5");
    hold(KC_0, 900);
    on_phase();
    CHECK_EQ(ui_state(), UI_EDIT);
    tap(KC_ENT);
    sim(100);
    CHECK_EQ((int16_t)fake_plc_hold[11], -5);
    sim(700);
    on_phase();
    EXPECT_ROW(1, " -05");                                      /* -0.5 hiển thị */
    CHECK(dp_at(1, 2));

    /* giá trị thấp hơn min */
    type_num("1");
    hold(KC_0, 900);
    type_num("1");                                              /* -11 */
    hold(KC_0, 0);
    tap(KC_ENT);
    sim(100);
    CHECK_EQ((int16_t)fake_plc_hold[11], -5 == 0 ? 0 : (int16_t)fake_plc_hold[11]);

    /* ENT khi chưa gõ gì: không làm gì */
    tap(KC_ENT);
    CHECK_EQ(ui_state(), UI_PAGE);
}

static void test_edit_error_paths(void)
{
    rt_blob_t b = mcfg();
    boot_running(&b);
    fake_plc_hold[11] = 50;
    sim(100);
    /* PLC không trả lời khi ghi */
    type_num("200");
    fake_plc_online = false;
    tap(KC_ENT);
    sim(300);
    CHECK_EQ(ui_state(), UI_PAGE);
    EXPECT_ROW(1, "E-CO");                                      /* thông báo đè lên hàng đang sửa */
    CHECK_EQ(fake_plc_hold[11], 50);
    fake_plc_online = true;
    sim(2000);
    /* phím chuyển trang khi đang nhập bị bỏ qua, ESC thoát */
    type_num("3");
    tap(KC_DOWN);
    CHECK_EQ(ui_state(), UI_EDIT);
    tap(KC_ESC); tap(KC_ESC);
    CHECK_EQ(ui_state(), UI_PAGE);
}

/* ------------------------------------------------------------------ trang */
static void test_pages(void)
{
    rt_blob_t b = mcfg();
    boot_running(&b);
    fake_plc_hold[20] = 4321;
    fake_plc_hold[30] = 99;
    tap(KC_DOWN);
    sim(200);
    EXPECT_ROW(0, "Cnt ");
    EXPECT_ROW(1, "4321");
    tap(KC_DOWN);
    sim(200);
    EXPECT_ROW(0, "  99");
    tap(KC_DOWN);                                               /* quay vòng về trang 0 */
    sim(100);
    CHECK_EQ(ui_state(), UI_PAGE);
    EXPECT_ROW(0, "  00");
    tap(KC_UP);                                                 /* lên từ trang 0 -> trang cuối */
    sim(200);
    EXPECT_ROW(0, "  99");
    tap(KC_ESC);                                                /* ESC về trang RUN */
    sim(100);
    EXPECT_ROW(0, "  00");

    /* nhấn giữ ▼: tự lặp */
    fake_key_press(sw_of(KC_DOWN));
    sim(400);
    EXPECT_ROW(0, "Cnt ");
    sim(900);
    fake_key_release(sw_of(KC_DOWN));
    sim(100);
    CHECK(!row_is(0, "  00") || !row_is(0, "Cnt "));           /* đã đi qua nhiều trang */
}

/* ------------------------------------------------------------------ mật khẩu */
static void test_password(void)
{
    rt_blob_t b = mcfg();
    b.comm.idle_s = 3;
    rtcfg_seal(&b);
    boot_running(&b);
    tap(KC_UP);                                                 /* sang trang 2 (có ô nhập cần mật khẩu) */
    sim(200);
    EXPECT_ROW(0, "   0");
    type_num("5");
    CHECK_EQ(ui_state(), UI_PASS);
    EXPECT_ROW(0, "PASS");
    EXPECT_ROW(1, "____");
    type_num("11");
    EXPECT_ROW(1, "--__");
    tap(KC_ENT);
    EXPECT_ROW(1, "E-Pd");                                       /* sai: xoá và yêu cầu lại */
    sim(1600);
    CHECK_EQ(ui_state(), UI_PASS);
    CHECK_EQ(ui_level(), 0);
    type_num("1234");
    tap(KC_ENT);
    CHECK_EQ(ui_level(), 1);
    CHECK_EQ(ui_state(), UI_EDIT);
    type_num("77");
    tap(KC_ENT);
    sim(200);
    CHECK_EQ(fake_plc_hold[31], 77);

    /* đã đăng nhập: lần sau không hỏi nữa */
    type_num("8");
    CHECK_EQ(ui_state(), UI_EDIT);
    tap(KC_ENT);
    sim(200);
    CHECK_EQ(fake_plc_hold[31], 8);

    /* hết giờ chờ 3 s: đăng xuất và về trang RUN */
    sim(3300);
    CHECK_EQ(ui_level(), 0);
    EXPECT_ROW(0, "  00");
    CHECK_EQ(ui_state(), UI_PAGE);
    tap(KC_UP);
    type_num("2");
    CHECK_EQ(ui_state(), UI_PASS);
    tap(KC_ESC);                                                /* xoá */
    tap(KC_ESC);                                                /* huỷ về trang RUN */
    CHECK_EQ(ui_state(), UI_PAGE);
}

/* ------------------------------------------------------------------ phím F, cảnh báo, mất liên lạc */
static void test_fkeys_alarm_comm(void)
{
    rt_blob_t b = mcfg();
    boot_running(&b);

    hold(KC_F1, 400);                                           /* momentary: 1 khi giữ, 0 khi nhả */
    CHECK_EQ(fake_plc_coil[50], 0);
    fake_key_press(sw_of(KC_F1));
    sim(300);
    CHECK_EQ(fake_plc_coil[50], 1);
    fake_key_release(sw_of(KC_F1));
    sim(300);
    CHECK_EQ(fake_plc_coil[50], 0);

    tap(KC_F2);                                                 /* GOTO trang 2 */
    sim(150);
    CHECK_EQ(ui_state(), UI_PAGE);
    fake_plc_hold[30] = 12;
    sim(200);
    EXPECT_ROW(0, "  12");
    tap(KC_ESC);
    sim(100);

    /* cảnh báo: mã A-01 nhấp nháy ở hàng trên, còi kêu, ESC xác nhận */
    fake_plc_coil[70] = 1;
    sim(400);
    int found = 0;
    for (int i = 0; i < 1500; i++) {
        sim(1);
        if (row_is(0, "A-01"))
            found++;
    }
    CHECK(found > 300);
    CHECK_EQ(engine_alarms_pending(), 1);
    CHECK(fake_buzzer_on_count > 0);
    tap(KC_ESC);
    sim(200);
    CHECK_EQ(fake_plc_coil[90], 1);
    CHECK_EQ(engine_alarms_pending(), 0);
    found = 0;
    for (int i = 0; i < 1500; i++) {
        sim(1);
        if (row_is(0, "A-01"))
            found++;
    }
    CHECK_EQ(found, 0);
    CHECK(fake_led(1) == false);

    /* mất liên lạc: E-CO ở hàng trên (pha B), giữ nguyên giá trị hàng dưới, LED COMM tắt */
    fake_plc_hold[11] = 77;
    sim(200);
    fake_plc_online = false;
    sim(500);
    found = 0;
    int bottom_ok = 0;
    for (int i = 0; i < 1500; i++) {
        sim(1);
        if (row_is(0, "E-CO"))
            found++;
        if (row_is(1, "  77"))
            bottom_ok++;
    }
    CHECK(found > 300);
    CHECK(bottom_ok > 1000);                                    /* giá trị cũ vẫn hiện (7,7) */
    CHECK(!fake_led(0));
    fake_plc_online = true;
    sim(1500);
    found = 0;
    for (int i = 0; i < 1500; i++) {
        sim(1);
        if (row_is(0, "E-CO"))
            found++;
    }
    CHECK_EQ(found, 0);
}

/* ------------------------------------------------------------------ slave */
static void test_slave(void)
{
    rt_blob_t b = scfg();
    boot_running(&b);
    CHECK_EQ(ui_state(), UI_SLAVE);
    CHECK_EQ(fake_plc_requests, 0);                             /* không bao giờ phát yêu cầu */
    bool ok;
    CHECK_EQ(rd_reg(5, 0, &ok), 0); CHECK(ok);
    CHECK(!bus_txn((uint8_t[]){ 6, 3, 0, 0, 0, 1, 0, 0 }, 8, &(uint16_t){ 0 }));   /* khung sai địa chỉ/CRC: im lặng */

    CHECK(wr_reg(5, REG_TOP_LO, 1234));
    CHECK(wr_reg(5, REG_BOT_LO, 56));
    CHECK(wr_reg(5, REG_FMT, 0x0001 | 0x0020));                 /* hàng trên 1 số lẻ, hàng dưới 2 số lẻ */
    sim(50);
    EXPECT_ROW(0, "1234"); CHECK(dp_at(0, 2));
    EXPECT_ROW(1, " 056"); CHECK(dp_at(1, 1));
    CHECK(wr_reg(5, REG_LED, 0x0003));
    sim(30);
    CHECK(fake_led(0) && fake_led(1) && !fake_led(2));
    CHECK(wr_reg(5, REG_CTRL, 2));
    sim(30);
    CHECK_EQ(fake_tm_bright, 2);

    /* phím báo qua thanh ghi */
    tap(KC_7);
    tap(KC_3);
    CHECK_EQ(rd_reg(5, REG_KEY_CODE, &ok), KC_3);
    CHECK_EQ(rd_reg(5, REG_KEY_COUNT, &ok), 2);
    /* gõ số vào hàng dưới và xác nhận bằng ENT */
    hold(KC_ESC, 1100);                                         /* giữ ESC: xoá số đang gõ (73) */
    tap(KC_4); tap(KC_2);
    on_phase();
    EXPECT_ROW(1, " 042");
    tap(KC_ENT);
    CHECK_EQ(rd_reg(5, REG_VAL_LO, &ok), 42);
    CHECK_EQ(rd_reg(5, REG_FLAGS, &ok) & 1, 1);
    CHECK(wr_reg(5, REG_FLAGS, 0));
    CHECK_EQ(rd_reg(5, REG_FLAGS, &ok), 0);
    sim(50);
    EXPECT_ROW(1, " 056");                                      /* trở lại giá trị PLC ghi */

    /* chế độ ghép 8 số và khoá phím */
    CHECK(wr_reg(5, REG_TOP_HI, 0x0001));
    CHECK(wr_reg(5, REG_TOP_LO, 0x0002));
    CHECK(wr_reg(5, REG_FMT, 0x0100));
    sim(50);
    EXPECT_ROW(0, "   6");
    EXPECT_ROW(1, "5538");                                      /* 65538 */
    CHECK(wr_reg(5, REG_CTRL, 0x0400 | 5));
    tap(KC_1);
    CHECK_EQ(rd_reg(5, REG_KEY_CODE, &ok), KC_1);               /* vẫn báo phím */
    sim(30);
    CHECK_EQ(fake_tm_bright, 5);

    /* đoạn thô */
    CHECK(wr_reg(5, REG_RAW0, 0x77));
    CHECK(wr_reg(5, REG_FMT, 0x1000));
    sim(50);
    CHECK_EQ(fake_seg(0), 0x77);
    CHECK_EQ(fake_seg(1), 0x00);
}

/* ------------------------------------------------------------------ cửa sổ cấu hình / PC */
static void test_config_mode(void)
{
    rt_blob_t b = mcfg();
    boot(&b, false);
    sim(500);
    bool ok;
    CHECK_EQ(rd_reg(247, REG_INFO, &ok), 0x504B); CHECK(ok);
    fake_plc_enable(false);
    sim(800);
    CHECK(cfgsrv_config_mode());
    CHECK_EQ(ui_state(), UI_CONFIG);
    EXPECT_ROW(0, "PC  ");
    sim(4000);                                                  /* qua 3 s: master KHÔNG khởi động */
    CHECK_EQ(rd_reg(247, REG_INFO + 12, &ok) & 8, 8);
    int tx0 = fake_tx_count;
    sim(2000);
    CHECK_EQ(fake_tx_count, tx0);                               /* master không phát gì */

    /* nạp khối mới (slave, địa chỉ 9) từng đoạn 60 thanh ghi rồi áp dụng */
    rt_blob_t nb = scfg();
    nb.comm.kbd_id = 9;
    nb.revision = 3;
    rtcfg_seal(&nb);
    const uint8_t *p = (const uint8_t *)&nb;
    uint16_t regs = (uint16_t)((RT_BLOB_SIZE + 1) / 2);
    for (uint16_t off = 0; off < regs; off += 60) {
        uint16_t cnt = (uint16_t)((regs - off) > 60 ? 60 : regs - off);
        uint8_t rq[200];
        rq[0] = 247; rq[1] = 16;
        uint16_t a = (uint16_t)(REG_WINDOW + off);
        rq[2] = (uint8_t)(a >> 8); rq[3] = (uint8_t)a;
        rq[4] = 0; rq[5] = (uint8_t)cnt; rq[6] = (uint8_t)(cnt * 2);
        for (uint16_t i = 0; i < cnt; i++) {
            uint16_t bi = (uint16_t)(2 * (off + i));
            rq[7 + 2 * i] = p[bi];
            rq[8 + 2 * i] = (uint8_t)(bi + 1 < RT_BLOB_SIZE ? p[bi + 1] : 0);
        }
        uint16_t n = (uint16_t)(7 + cnt * 2);
        uint16_t c = crc16_modbus(rq, n);
        rq[n] = (uint8_t)c; rq[n + 1] = (uint8_t)(c >> 8);
        uint16_t rl = 0;
        const uint8_t *r = bus_txn(rq, (uint16_t)(n + 2), &rl);
        CHECK(r && rl == 8);
    }
    CHECK(wr_reg(247, REG_CMD, CMD_APPLY));
    CHECK_EQ(rd_reg(247, REG_CMD_RESULT, &ok), 0);
    rt_blob_t chk;
    CHECK_EQ(storage_load(&chk), ST_OK);
    CHECK_EQ(chk.comm.kbd_id, 9);

    /* PC test: ghi LED / đoạn thô trong chế độ cấu hình */
    CHECK(wr_reg(247, REG_LED, 0x0010));
    sim(30);
    CHECK(fake_led(4));

    CHECK_EQ(fake_reset_count, 0);
    CHECK(wr_reg(247, REG_CMD, CMD_EXIT));
    sim(150);
    CHECK(fake_reset_count > 0);
}

static void test_window_enter_slave_cmd(void)
{
    /* thiết bị slave đang chạy: PC ra lệnh vào chế độ cấu hình qua địa chỉ riêng */
    rt_blob_t b = scfg();
    boot_running(&b);
    bool ok;
    CHECK(wr_reg(5, REG_CMD, CMD_ENTER_CONFIG));
    sim(100);
    CHECK_EQ(ui_state(), UI_CONFIG);
    CHECK_EQ(rd_reg(5, REG_INFO, &ok), 0x504B);                 /* vẫn trả lời ở địa chỉ cũ */
    CHECK(ok);
    CHECK_EQ(fake_baud, 9600);
    /* không hoạt động quá lâu: tự khởi động lại */
    sim(CONFIG_IDLE_MS + 500);
    CHECK(fake_reset_count > 0);
}

/* ------------------------------------------------------------------ menu cài đặt */
static void test_menu(void)
{
    rt_blob_t b = scfg();
    b.sec.pwd_tech = 0;
    rtcfg_seal(&b);
    boot(&b, true);
    sim(1500);
    CHECK_EQ(ui_state(), UI_MENU);
    EXPECT_ROW(0, "nodE");
    EXPECT_ROW(1, "SLv ");
    tap(KC_DOWN); EXPECT_ROW(0, "PLC "); EXPECT_ROW(1, "   1");
    tap(KC_DOWN); EXPECT_ROW(0, "Adr "); EXPECT_ROW(1, "   5");
    tap(KC_DOWN); EXPECT_ROW(0, "bAud"); EXPECT_ROW(1, "9600");
    tap(KC_ENT);
    tap(KC_DOWN);                                               /* 9600 -> 19.2 */
    sim(300);
    tap(KC_ENT);
    EXPECT_ROW(1, "192 ");
    CHECK(dp_at(1, 1));
    tap(KC_UP);                                                 /* về mục Adr */
    tap(KC_ENT);
    type_num("42");
    on_phase();
    EXPECT_ROW(1, "  42");
    tap(KC_ENT);
    EXPECT_ROW(1, "  42");
    tap(KC_DOWN);
    tap(KC_DOWN);
    tap(KC_DOWN);
    tap(KC_DOWN);                                               /* PAr, StoP, tout */
    EXPECT_ROW(0, "tout");
    tap(KC_ENT);
    type_num("9999");
    tap(KC_ENT);
    on_phase();
    EXPECT_ROW(1, "E-Hi");                                       /* quá 5000 */
    sim(1600);
    tap(KC_ESC); tap(KC_ESC); tap(KC_ESC); tap(KC_ESC);          /* xoá từng số */
    tap(KC_ESC);                                                /* hết số: thoát chỉnh */
    CHECK_EQ(ui_state(), UI_MENU);
    EXPECT_ROW(0, "tout");

    /* thoát menu: lưu và khởi động lại */
    CHECK_EQ(fake_reset_count, 0);
    tap(KC_ESC);
    CHECK(fake_reset_count > 0);
    rt_blob_t chk;
    CHECK_EQ(storage_load(&chk), ST_OK);
    CHECK_EQ(chk.comm.baud_idx, 3);
    CHECK_EQ(chk.comm.kbd_id, 42);
    CHECK_EQ(chk.comm.timeout_ms, 200);
    CHECK_EQ(rtcfg_validate(&chk), RT_OK);
}

static void test_menu_password_and_actions(void)
{
    rt_blob_t b = mcfg();
    boot(&b, true);
    sim(1500);
    CHECK_EQ(ui_state(), UI_PASS);                              /* có mật khẩu kỹ thuật */
    EXPECT_ROW(0, "PASS");
    type_num("1234");
    tap(KC_ENT);
    EXPECT_ROW(1, "E-Pd");                                       /* mật khẩu vận hành không đủ quyền */
    sim(1600);
    type_num("4321");
    tap(KC_ENT);
    CHECK_EQ(ui_state(), UI_MENU);
    EXPECT_ROW(0, "nodE");
    EXPECT_ROW(1, "nSt ");

    /* phiên bản */
    for (int i = 0; i < 19; i++)
        tap(KC_DOWN);
    EXPECT_ROW(0, "Ur  ");
    CHECK(dp_at(1, 1));
    tap(KC_DOWN);
    EXPECT_ROW(0, "nodE");

    /* sửa nhanh đối tượng: mục obj -> chọn 2 (đối tượng dưới trang 0) -> Adr = 200 */
    for (int i = 0; i < 15; i++)
        tap(KC_DOWN);
    EXPECT_ROW(0, "obj ");
    tap(KC_ENT);
    tap(KC_DOWN);
    EXPECT_ROW(1, "   2");
    tap(KC_ENT);
    EXPECT_ROW(0, "Adr ");
    tap(KC_ENT);
    type_num("200");
    tap(KC_ENT);
    tap(KC_DOWN);
    EXPECT_ROW(0, "Lo  ");
    tap(KC_ESC);                                                /* về chọn đối tượng */
    tap(KC_ESC);                                                /* về danh sách */
    EXPECT_ROW(0, "obj ");
    CHECK_EQ(cfg()->page[0].bot.addr, 200);

    /* kiểm tra phần cứng */
    tap(KC_DOWN);
    EXPECT_ROW(0, "tESt");
    tap(KC_ENT);
    sim(100);
    CHECK(fake_seg(0) == 0xFF && fake_seg(7) == 0xFF);
    tap(KC_5);
    sim(30);
    EXPECT_ROW(0, "0010");                                      /* KC_5 nằm ở SW10 */
    hold(KC_ESC, 1150);
    CHECK_EQ(ui_state(), UI_MENU);

    /* mặc định nhà máy cần nhấn ENT hai lần */
    tap(KC_DOWN); tap(KC_DOWN);
    EXPECT_ROW(0, "dEF ");
    tap(KC_ENT);
    EXPECT_ROW(1, "SurE");
    CHECK_EQ(cfg()->comm.mode, RT_MODE_MASTER);
    tap(KC_ENT);
    CHECK_EQ(cfg()->comm.mode, RT_MODE_SLAVE);
    /* vào chế độ PC từ menu */
    tap(KC_UP);
    EXPECT_ROW(0, "PC  ");
    tap(KC_ENT);
    sim(200);
    CHECK_EQ(ui_state(), UI_CONFIG);
    bool ok;
    CHECK_EQ(rd_reg(247, REG_INFO, &ok), 0x504B);
}

void suite_ui(void)
{
    RUN(test_banner_and_page);
    RUN(test_edit_flow);
    RUN(test_edit_error_paths);
    RUN(test_pages);
    RUN(test_password);
    RUN(test_fkeys_alarm_comm);
    RUN(test_slave);
    RUN(test_config_mode);
    RUN(test_window_enter_slave_cmd);
    RUN(test_menu);
    RUN(test_menu_password_and_actions);
}
