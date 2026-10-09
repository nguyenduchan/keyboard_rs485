#include "fake_hw.h"
#include "mb_slave.h"
#include "rs485.h"
#include "crc.h"
#include "config.h"
#include <string.h>

/* ------------------------------------------------------------------ hệ thống */
uint32_t fake_now;
int  fake_reset_count;
int  fake_wdt_kicks;
bool fake_wdt_started;
bool fake_power_ok = true;

bool     fake_buzzer;
int      fake_buzzer_on_count;
uint32_t fake_buzzer_on_ms;
static uint32_t buz_since;

bool fake_tick_on_read;

uint32_t hw_millis(void)
{
    if (fake_tick_on_read)
        fake_now++;                 /* cho các vòng chờ bận trong app_init chạy được */
    return fake_now;
}
void hw_wdt_start(void) { fake_wdt_started = true; }
void hw_wdt_kick(void) { fake_wdt_kicks++; }
void hw_reset(void) { fake_reset_count++; }
uint32_t hw_uid32(void) { return 0x12345678u; }
bool hw_power_ok(void) { return fake_power_ok; }

void hw_buzzer(bool on)
{
    if (on && !fake_buzzer) {
        fake_buzzer_on_count++;
        buz_since = fake_now;
    }
    if (!on && fake_buzzer)
        fake_buzzer_on_ms += fake_now - buz_since;
    fake_buzzer = on;
}

/* ------------------------------------------------------------------ TM1638 + phím */
uint8_t fake_tm_ram[16];
uint8_t fake_tm_bright;
int     fake_tm_writes;
static bool pressed[3][8];          /* [K1..K3][KS1..KS8] */
static bool tm_dead;

void hw_tm_init(void) { memset(fake_tm_ram, 0, sizeof fake_tm_ram); }

void hw_tm_write(const uint8_t ram[TM_RAM_SIZE], uint8_t brightness)
{
    memcpy(fake_tm_ram, ram, TM_RAM_SIZE);
    fake_tm_bright = brightness;
    fake_tm_writes++;
}

void fake_key_press(uint8_t sw)
{
    uint8_t i = (uint8_t)(sw - 1u);
    pressed[i / 8u][i % 8u] = true;
}

void fake_key_release(uint8_t sw)
{
    uint8_t i = (uint8_t)(sw - 1u);
    pressed[i / 8u][i % 8u] = false;
}

void fake_key_release_all(void)
{
    memset(pressed, 0, sizeof pressed);
}

void fake_tm_dead(bool d) { tm_dead = d; }

bool hw_tm_read_keys(uint8_t keys[4])
{
    if (tm_dead) {
        memset(keys, 0xFF, 4);
        return false;
    }
    bool r[3][8];
    for (int k = 0; k < 3; k++)
        for (int s = 0; s < 8; s++) {
            bool v = pressed[k][s];
            /* "phím ma": 3 phím ở 3 góc hình chữ nhật làm góc thứ 4 cũng dẫn điện */
            for (int k2 = 0; k2 < 3 && !v; k2++)
                for (int s2 = 0; s2 < 8 && !v; s2++)
                    if (k2 != k && s2 != s && pressed[k][s2] && pressed[k2][s2] && pressed[k2][s])
                        v = true;
            r[k][s] = v;
        }
    memset(keys, 0, 4);
    for (int s = 0; s < 8; s++) {
        uint8_t nib = 0;
        if (r[0][s]) nib |= 0x04;       /* K1 */
        if (r[1][s]) nib |= 0x02;       /* K2 */
        if (r[2][s]) nib |= 0x01;       /* K3 */
        keys[s / 2] |= (uint8_t)(nib << ((s % 2) * 4));
    }
    return true;
}

uint8_t fake_seg(uint8_t d) { return fake_tm_ram[2 * d]; }
bool fake_led(uint8_t l) { return fake_tm_ram[2 * l + 1] & 1u; }

void fake_row_text(uint8_t row, char out[9])
{
    /* bảng ngược của font: 0-9 và vài chữ thường gặp */
    static const struct { uint8_t seg; char c; } map[] = {
        { 0x00, ' ' }, { 0x3F, '0' }, { 0x06, '1' }, { 0x5B, '2' }, { 0x4F, '3' }, { 0x66, '4' }, { 0x6D, '5' },
        { 0x7D, '6' }, { 0x07, '7' }, { 0x7F, '8' }, { 0x6F, '9' }, { 0x40, '-' }, { 0x77, 'A' }, { 0x7C, 'b' },
        { 0x39, 'C' }, { 0x5E, 'd' }, { 0x79, 'E' }, { 0x71, 'F' }, { 0x38, 'L' }, { 0x54, 'n' }, { 0x5C, 'o' },
        { 0x73, 'P' }, { 0x50, 'r' }, { 0x78, 't' }, { 0x3E, 'U' }, { 0x76, 'H' }, { 0x30, 'I' }, { 0x58, 'c' },
        { 0x74, 'h' }, { 0x04, 'i' }, { 0x1E, 'J' }, { 0x1C, 'u' }, { 0x6E, 'y' }, { 0x08, '_' },
    };
    int n = 0;
    for (int d = 0; d < 4; d++) {
        uint8_t s = fake_tm_ram[2 * (row * 4 + d)];
        char c = '?';
        for (unsigned i = 0; i < sizeof map / sizeof map[0]; i++)
            if (map[i].seg == (s & 0x7Fu))
                c = map[i].c;
        out[n++] = c;
        if (s & 0x80u)
            out[n++] = '.';
    }
    out[n] = 0;
}

/* ------------------------------------------------------------------ flash */
#define FL_BASE 0x08007000u
#define FL_SIZE 4096u
static uint8_t flash[FL_SIZE];
int fake_flash_erases, fake_flash_writes;
static int cut_after = -1;
static int writes_done;

void fake_flash_power_cut_after(int n)
{
    cut_after = n;
    writes_done = 0;
}

void fake_flash_corrupt(uint32_t addr, uint8_t mask)
{
    flash[addr - FL_BASE] ^= mask;
}

bool hw_flash_erase_page(uint32_t addr)
{
    if (cut_after >= 0 && writes_done >= cut_after)
        return false;
    if (addr < FL_BASE || addr >= FL_BASE + FL_SIZE || (addr & 0x7FFu))
        return false;
    memset(&flash[addr - FL_BASE], 0xFF, 2048);
    fake_flash_erases++;
    writes_done++;
    return true;
}

bool hw_flash_write(uint32_t addr, const void *data, uint32_t len)
{
    if (addr < FL_BASE || addr + len > FL_BASE + FL_SIZE || (addr & 7u) || (len & 7u))
        return false;
    if (cut_after >= 0 && writes_done >= cut_after)
        return false;
    for (uint32_t i = 0; i < len; i++)
        if (flash[addr - FL_BASE + i] != 0xFFu)
            return false;                           /* ghi vào ô chưa xoá: phần cứng thật báo lỗi */
    memcpy(&flash[addr - FL_BASE], data, len);
    fake_flash_writes++;
    writes_done++;
    return true;
}

const uint8_t *hw_flash_ptr(uint32_t addr)
{
    return &flash[addr - FL_BASE];
}

/* ------------------------------------------------------------------ RS485 giả */
uint8_t  fake_tx_log[16][256];
uint16_t fake_tx_len[16];
int      fake_tx_count;
uint32_t fake_baud;
uint8_t  fake_parity, fake_stop;
int      fake_rs485_inits;

static uint8_t txbuf[MB_MAX_ADU];
static uint8_t rxbuf[MB_MAX_ADU];
static uint16_t rxlen;
static bool rxbad, rxready;
static uint32_t rx_at;              /* thời điểm khung "đến" */
static bool rx_scheduled;
static uint32_t tx_done_at;
static bool tx_busy;

uint16_t fake_plc_hold[2048];
uint8_t  fake_plc_coil[2048];
uint8_t  fake_plc_id = 1;
bool     fake_plc_online = true;
int      fake_plc_requests;
int      fake_plc_bad_addr_from = -1;
int      fake_plc_drop_next;
int      fake_plc_corrupt_next;
uint8_t  fake_plc_last_req[256];
uint16_t fake_plc_last_len;
static bool plc_on;

void fake_plc_enable(bool on) { plc_on = on; }

void fake_plc_clear(void)
{
    memset(fake_plc_hold, 0, sizeof fake_plc_hold);
    memset(fake_plc_coil, 0, sizeof fake_plc_coil);
    fake_plc_online = true;
    fake_plc_requests = 0;
    fake_plc_bad_addr_from = -1;
    fake_plc_drop_next = 0;
    fake_plc_corrupt_next = 0;
}

static uint8_t plc_rd_reg(uint16_t a, uint16_t *v, bool in)
{
    (void)in;
    if (a >= 2048u || (fake_plc_bad_addr_from >= 0 && a >= (uint16_t)fake_plc_bad_addr_from))
        return MB_EX_ADDRESS;
    *v = fake_plc_hold[a];
    return 0;
}
static uint8_t plc_chk(uint16_t a, uint16_t v)
{
    (void)v;
    return (a >= 2048u || (fake_plc_bad_addr_from >= 0 && a >= (uint16_t)fake_plc_bad_addr_from)) ? MB_EX_ADDRESS : 0;
}
static uint8_t plc_wr_reg(uint16_t a, uint16_t v) { fake_plc_hold[a] = v; return 0; }
static uint8_t plc_rd_bit(uint16_t a, uint8_t *v, bool d)
{
    (void)d;
    if (a >= 2048u)
        return MB_EX_ADDRESS;
    *v = fake_plc_coil[a];
    return 0;
}
static uint8_t plc_wr_bit(uint16_t a, uint8_t v)
{
    if (a >= 2048u)
        return MB_EX_ADDRESS;
    fake_plc_coil[a] = v;
    return 0;
}
static const mbs_ops_t plc_ops = { plc_rd_reg, plc_chk, plc_wr_reg, plc_rd_bit, plc_wr_bit };

void fake_bus_reset(void)
{
    rxlen = 0;
    rxbad = rxready = rx_scheduled = tx_busy = false;
    fake_tx_count = 0;
    memset(fake_tx_len, 0, sizeof fake_tx_len);
}

void fake_rs485_inject(const uint8_t *frame, uint16_t len, bool bad)
{
    memcpy(rxbuf, frame, len);
    rxlen = len;
    rxbad = bad;
    rx_scheduled = true;
    rx_at = fake_now + 1u;
    rxready = false;
}

static void update(void)
{
    if (tx_busy && (int32_t)(fake_now - tx_done_at) >= 0)
        tx_busy = false;
    if (rx_scheduled && (int32_t)(fake_now - rx_at) >= 0) {
        rx_scheduled = false;
        rxready = true;
    }
}

void rs485_init(uint32_t baud, uint8_t parity, uint8_t stop)
{
    fake_baud = baud;
    fake_parity = parity;
    fake_stop = stop;
    fake_rs485_inits++;
    rxlen = 0;
    rxbad = rxready = rx_scheduled = false;
}

uint8_t *rs485_txbuf(void) { return txbuf; }
bool rs485_tx_busy(void) { update(); return tx_busy; }
bool rs485_rx_ready(void) { update(); return rxready; }

const uint8_t *rs485_rx(uint16_t *len, bool *bad)
{
    *len = rxlen;
    *bad = rxbad;
    return rxbuf;
}

void rs485_rx_release(void)
{
    rxlen = 0;
    rxbad = rxready = rx_scheduled = false;
}

bool rs485_send(uint16_t len)
{
    update();
    if (tx_busy)
        return false;
    memcpy(fake_tx_log[fake_tx_count % 16], txbuf, len);          /* vòng 16 khung gần nhất */
    fake_tx_len[fake_tx_count % 16] = len;
    fake_tx_count++;
    tx_busy = true;
    tx_done_at = fake_now + 1u;
    rxlen = 0;                      /* sau khi gửi, bộ đệm nhận của chính mình trống */
    rxready = rx_scheduled = false;
    if (plc_on) {
        fake_plc_requests++;
        memcpy(fake_plc_last_req, txbuf, len);
        fake_plc_last_len = len;
        if (fake_plc_online && fake_plc_drop_next > 0) {
            fake_plc_drop_next--;
        } else if (fake_plc_online) {
            uint8_t resp[MB_MAX_ADU];
            uint16_t rl = mbs_process(fake_plc_id, txbuf, len, resp, &plc_ops);
            if (rl) {
                if (fake_plc_corrupt_next > 0) {
                    fake_plc_corrupt_next--;
                    resp[rl - 1] ^= 0x55;
                }
                memcpy(rxbuf, resp, rl);
                rxlen = rl;
                rxbad = false;
                rx_scheduled = true;
                rx_at = fake_now + 3u;
                rxready = false;
            }
        }
    } else {
        /* sau khi gửi xong, bộ đệm nhận trống */
        rxlen = 0;
        rxready = rx_scheduled = false;
    }
    return true;
}

void rs485_uart_irq(void) {}
void rs485_timer_irq(void) {}

/* ------------------------------------------------------------------ khởi tạo lại */
void fake_hw_keep_flash_reset(void)
{
    fake_now = 0;
    fake_reset_count = 0;
    fake_wdt_kicks = 0;
    fake_wdt_started = false;
    fake_power_ok = true;
    fake_tick_on_read = false;
    fake_buzzer = false;
    fake_buzzer_on_count = 0;
    fake_buzzer_on_ms = 0;
    memset(fake_tm_ram, 0, sizeof fake_tm_ram);
    fake_tm_writes = 0;
    fake_key_release_all();
    tm_dead = false;
    cut_after = -1;
    fake_bus_reset();
    fake_plc_clear();
    plc_on = false;
}

void fake_hw_reset_all(void)
{
    memset(flash, 0xFF, sizeof flash);
    fake_flash_erases = fake_flash_writes = 0;
    fake_hw_keep_flash_reset();
}
