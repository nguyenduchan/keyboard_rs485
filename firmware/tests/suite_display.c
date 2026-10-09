/* display, keys, buzzer */
#include "testlib.h"
#include "fake_hw.h"
#include "display.h"
#include "keys.h"
#include "buzzer.h"
#include "rtcfg.h"

static void show(void)
{
    display_flush(fake_now);
}

static void row(uint8_t r, char out[9])
{
    show();
    fake_row_text(r, out);
}

#define EXPECT_ROW(r, txt) do { char _t[10]; row(r, _t); if (strcmp(_t, txt) == 0) t_pass++; else { t_fail++; printf("  FAIL %s:%d [%s] row%d = \"%s\" (expect \"%s\")\n", __FILE__, __LINE__, t_name, r, _t, txt); } } while (0)

static void dfresh(void)
{
    fake_hw_reset_all();
    display_init(5);
    fake_now = 1000;
}

static void test_numbers(void)
{
    dfresh();
    display_number(0, 1234, 0, 0, 4);  EXPECT_ROW(0, "1234");
    display_number(0, 12, 0, 0, 4);    EXPECT_ROW(0, "  12");
    display_number(0, 0, 0, 0, 4);     EXPECT_ROW(0, "   0");
    display_number(0, -5, 0, 0, 4);    EXPECT_ROW(0, "  -5");
    display_number(0, -999, 0, 0, 4);  EXPECT_ROW(0, "-999");
    display_number(0, -1000, 0, 0, 4); EXPECT_ROW(0, "----");
    display_number(0, 10000, 0, 0, 4); EXPECT_ROW(0, "----");
    display_number(0, 12, 0, DISP_FLAG_ZEROS, 4); EXPECT_ROW(0, "0012");
    display_number(0, -5, 0, DISP_FLAG_ZEROS, 4); EXPECT_ROW(0, "-005");
    display_number(1, 7, 0, 0, 4);     EXPECT_ROW(1, "   7");
    display_number(0, 125, 1, 0, 4);   EXPECT_ROW(0, " 12.5");
    display_number(0, 5, 2, 0, 4);     EXPECT_ROW(0, " 0.05");
    display_number(0, 0, 3, 0, 4);     EXPECT_ROW(0, "0.000");
    display_number(0, 9999, 3, 0, 4);  EXPECT_ROW(0, "9.999");
    display_number(0, -25, 1, 0, 4);   EXPECT_ROW(0, " -2.5");
    display_number(0, -1234, 1, 0, 4); EXPECT_ROW(0, "----");     /* -123.4 cần 5 ô */
    display_number(0, 12345, 1, 0, 4); EXPECT_ROW(0, "----");

    /* ghép 8 số */
    display_number(0, 12345678, 0, 0, 8);
    show();
    char r0[10], r1[10];
    fake_row_text(0, r0); fake_row_text(1, r1);
    CHECK(strcmp(r0, "1234") == 0); CHECK(strcmp(r1, "5678") == 0);
    display_number(0, 42, 0, 0, 8);
    show(); fake_row_text(0, r0); fake_row_text(1, r1);
    CHECK(strcmp(r0, "    ") == 0); CHECK(strcmp(r1, "  42") == 0);
    display_number(0, -42, 2, 0, 8);
    show(); fake_row_text(0, r0); fake_row_text(1, r1);
    CHECK(strcmp(r1, "-0.42") == 0 || strcmp(r1, " 0.42") != 0);
}

static void test_text_leds(void)
{
    dfresh();
    display_str(0, "PASS");
    show();
    CHECK_EQ(fake_seg(0), 0x73); CHECK_EQ(fake_seg(1), 0x77); CHECK_EQ(fake_seg(2), 0x6D);
    display_str(1, "ab");
    show();
    CHECK_EQ(fake_seg(4), 0x5F); CHECK_EQ(fake_seg(5), 0x7C); CHECK_EQ(fake_seg(6), 0x00);
    uint8_t t[4] = { 'P' | 0x80, 'M', 'W', 'x' };            /* M, W không vẽ được -> '-' ; có chấm */
    display_text(0, t);
    show();
    CHECK_EQ(fake_seg(0), 0x73 | 0x80);
    CHECK_EQ(fake_seg(1), 0x40); CHECK_EQ(fake_seg(2), 0x40);
    CHECK(display_char_ok('A')); CHECK(!display_char_ok('M')); CHECK(!display_char_ok('W')); CHECK(display_char_ok('-'));
    display_raw(7, 0xA5);
    show();
    CHECK_EQ(fake_seg(7), 0xA5);

    display_clear();
    display_leds(0x05, 0);
    show();
    CHECK(fake_led(0)); CHECK(!fake_led(1)); CHECK(fake_led(2));
    CHECK_EQ(fake_tm_bright, 5);
    display_set_brightness(2);
    show();
    CHECK_EQ(fake_tm_bright, 2);
}

static void test_blink_refresh(void)
{
    dfresh();
    display_clear();
    display_leds(0, 0x02);                                      /* LED 2 nhấp nháy */
    display_str(0, "1234");
    display_row_blink(0, true);
    fake_now = 1000;                                            /* (1000/250)&1 = 0 : pha sáng */
    show();
    CHECK(fake_led(1)); CHECK_EQ(fake_seg(0), 0x06);
    fake_now = 1250;                                            /* pha tối */
    show();
    CHECK(!fake_led(1)); CHECK_EQ(fake_seg(0), 0x00);
    CHECK_EQ(fake_seg(4), 0x00);
    display_row_blink(0, false);
    show();
    CHECK_EQ(fake_seg(0), 0x06);

    /* chỉ ghi khi nội dung đổi hoặc đến kỳ làm tươi */
    int w = fake_tm_writes;
    show(); show();
    CHECK_EQ(fake_tm_writes, w);
    fake_now += 100; show();
    CHECK_EQ(fake_tm_writes, w);
    fake_now += 160; show();
    CHECK_EQ(fake_tm_writes, w + 1);                            /* > 250 ms: làm tươi */
    display_force();
    show();
    CHECK_EQ(fake_tm_writes, w + 2);
}

/* ------------------------------------------------------------------ phím */
static void kfresh(bool ext)
{
    fake_hw_reset_all();
    keys_init(ext);
    fake_now = 0;
}

static void kstep(int ms)
{
    for (int i = 0; i < ms; i++) {
        fake_now++;
        keys_poll(fake_now);
    }
}

static int ev_count(uint8_t type, uint8_t sw)
{
    key_event_t e;
    int n = 0;
    while (keys_pop(&e))
        if (e.type == type && e.sw == sw)
            n++;
    return n;
}

static void test_key_decode(void)
{
    uint8_t k[4] = { 0x04, 0, 0, 0 };
    CHECK_EQ(keys_decode(k, true), 1u << 0);                    /* K1/KS1 = SW1 */
    k[0] = 0x20;
    CHECK_EQ(keys_decode(k, true), 1u << 9);                    /* K2/KS2 = SW10 */
    k[0] = 0; k[3] = 0x40;                                      /* byte3 nửa cao = KS8, bit2... */
    CHECK_EQ(keys_decode(k, true), 1u << 7);                    /* 0x40: bit6 = bit2 của nửa cao = K1/KS8 = SW8 */
    k[3] = 0x20;
    CHECK_EQ(keys_decode(k, true), 1u << 15);                   /* K2/KS8 = SW16 */
    k[3] = 0x10;
    CHECK_EQ(keys_decode(k, true), 1u << 23);                   /* K3/KS8 = SW24 */
    CHECK_EQ(keys_decode(k, false), 0);
    uint8_t all[4] = { 0x77, 0x77, 0x77, 0x77 };
    CHECK_EQ(keys_decode(all, true), 0xFFFFFFu);

    CHECK_EQ(keys_logical(16), KC_ESC); CHECK_EQ(keys_logical(4), KC_ENT);
    CHECK_EQ(keys_logical(12), KC_UP); CHECK_EQ(keys_logical(8), KC_DOWN);
    CHECK_EQ(keys_logical(2), KC_0); CHECK_EQ(keys_logical(13), KC_7);
    CHECK_EQ(keys_logical(1), KC_F1); CHECK_EQ(keys_logical(3), KC_F2);
    CHECK_EQ(keys_logical(17), KC_F3); CHECK_EQ(keys_logical(0), KC_NONE);
    CHECK_EQ(kc_digit(KC_0), 0); CHECK_EQ(kc_digit(KC_9), 9); CHECK_EQ(kc_digit(KC_ESC), -1);
    /* mọi SW trong bố cục 4x4 đều khác nhau */
    for (int a = 1; a <= 24; a++)
        for (int b = a + 1; b <= 24; b++)
            CHECK(keys_logical((uint8_t)a) != keys_logical((uint8_t)b));
}

static void test_key_debounce(void)
{
    kfresh(false);
    fake_key_press(13);
    kstep(20);
    CHECK_EQ(keys_active(), 0);                                 /* chưa đủ 25 ms */
    kstep(15);
    CHECK_EQ(keys_active(), 13);
    CHECK_EQ(ev_count(KEV_PRESS, 13), 1);
    CHECK_EQ(keys_count(), 1);
    kstep(100);
    CHECK(keys_held_ms(fake_now) >= 100);
    CHECK_EQ(ev_count(KEV_PRESS, 13), 0);                       /* không lặp */
    fake_key_release(13);
    kstep(20);
    CHECK_EQ(keys_active(), 13);                                /* nhả chưa đủ lâu */
    kstep(15);
    CHECK_EQ(keys_active(), 0);
    CHECK_EQ(ev_count(KEV_RELEASE, 13), 1);

    /* nhiễu ngắn không tạo sự kiện */
    fake_key_press(5);
    kstep(10);
    fake_key_release(5);
    kstep(50);
    CHECK_EQ(keys_active(), 0);
    CHECK_EQ(ev_count(KEV_PRESS, 5), 0);

    /* dội: nhấn-nhả-nhấn nhanh */
    for (int i = 0; i < 4; i++) {
        fake_key_press(5); kstep(10);
        fake_key_release(5); kstep(10);
    }
    CHECK_EQ(keys_active(), 0);
    fake_key_press(5);
    kstep(40);
    CHECK_EQ(keys_active(), 5);
}

static void test_key_ghost(void)
{
    kfresh(false);
    /* SW1 (K1,KS1) đang giữ, rồi nhấn thêm SW2 (K1,KS2) và SW9 (K2,KS1): SW10 (K2,KS2) hiện "ma" */
    fake_key_press(1);
    kstep(40);
    CHECK_EQ(keys_active(), 1);
    fake_key_press(2); fake_key_press(9);
    kstep(100);
    CHECK_EQ(keys_active(), 1);
    CHECK((keys_raw() & (1u << 9)) != 0);                       /* phím ma có mặt trong dữ liệu thô */
    CHECK_EQ(ev_count(KEV_PRESS, 10), 0);
    CHECK_EQ(ev_count(KEV_PRESS, 2), 0);
    CHECK_EQ(ev_count(KEV_PRESS, 9), 0);
    fake_key_release(1);
    kstep(40);
    CHECK_EQ(keys_active(), 0);                                 /* nhiều phím => không nhận phím mới */
    kstep(100);
    CHECK_EQ(keys_active(), 0);
    fake_key_release_all();
    kstep(40);
    /* hai phím nhấn cùng lúc: chờ đến khi chỉ còn một */
    fake_key_press(6); fake_key_press(7);
    kstep(100);
    CHECK_EQ(keys_active(), 0);
    fake_key_release(7);
    kstep(40);
    CHECK_EQ(keys_active(), 6);
    CHECK_EQ(ev_count(KEV_PRESS, 6), 1);
}

static void test_key_misc(void)
{
    kfresh(false);
    fake_key_press(17);                                         /* K3 khi chưa bật phím mở rộng */
    kstep(60);
    CHECK_EQ(keys_active(), 0);
    kfresh(true);
    fake_key_press(17);
    kstep(60);
    CHECK_EQ(keys_active(), 17);
    CHECK_EQ(keys_logical(17), KC_F3);

    /* TM1638 không trả lời: giữ nguyên trạng thái, không sinh nhả giả */
    fake_tm_dead(true);
    kstep(200);
    CHECK_EQ(keys_active(), 17);
    CHECK_EQ(ev_count(KEV_RELEASE, 17), 0);
    fake_tm_dead(false);
    fake_key_release(17);
    kstep(40);
    CHECK_EQ(keys_active(), 0);
}

/* ------------------------------------------------------------------ còi */
static void bstep(int ms)
{
    for (int i = 0; i < ms; i++) {
        fake_now++;
        buzzer_poll(fake_now);
    }
}

static void test_buzzer(void)
{
    fake_hw_reset_all();
    buzzer_init(RT_BF_CLICK | RT_BF_ERROR | RT_BF_ALARM);
    CHECK(!fake_buzzer);
    buzzer_click();
    CHECK(fake_buzzer);
    bstep(10);
    CHECK(fake_buzzer);
    bstep(20);
    CHECK(!fake_buzzer);
    uint32_t on = fake_buzzer_on_ms;
    CHECK(on >= 15 && on <= 30);
    buzzer_error();
    bstep(100);
    CHECK(fake_buzzer);
    bstep(400);
    CHECK(!fake_buzzer);

    /* tắt từng nhóm theo cờ */
    buzzer_init(RT_BF_ERROR);
    buzzer_click();
    CHECK(!fake_buzzer);
    buzzer_error();
    CHECK(fake_buzzer);
    bstep(500);
    buzzer_init(RT_BF_CLICK);
    buzzer_error();
    CHECK(!fake_buzzer);
    buzzer_alarm(true);
    bstep(1000);
    CHECK(!fake_buzzer);

    /* cảnh báo lặp 200 ms kêu / 400 ms im */
    buzzer_init(RT_BF_ALARM);
    buzzer_alarm(true);
    int on_edges = fake_buzzer_on_count;
    bstep(3000);
    CHECK(fake_buzzer_on_count - on_edges >= 4);
    buzzer_alarm(false);
    bstep(700);
    CHECK(!fake_buzzer);

    /* bíp bắt buộc bỏ qua cờ */
    buzzer_init(0);
    buzzer_force(50);
    CHECK(fake_buzzer);
    bstep(60);
    CHECK(!fake_buzzer);
    buzzer_identify(1000);
    on_edges = fake_buzzer_on_count;
    bstep(1100);
    CHECK(fake_buzzer_on_count - on_edges >= 4);
    CHECK(!fake_buzzer);
}

void suite_display(void)
{
    RUN(test_numbers);
    RUN(test_text_leds);
    RUN(test_blink_refresh);
    RUN(test_key_decode);
    RUN(test_key_debounce);
    RUN(test_key_ghost);
    RUN(test_key_misc);
    RUN(test_buzzer);
}
