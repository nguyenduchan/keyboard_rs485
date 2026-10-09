#include "cfgsrv.h"
#include "cfg.h"
#include "engine.h"
#include "mb_master.h"
#include "buzzer.h"
#include "hwif.h"
#include "config.h"
#include <string.h>

static slave_state_t st;
static bool     config_mode;
static uint16_t last_cmd, cmd_result, last_error;
static uint32_t ident_until;
static uint32_t reset_at;
static uint32_t last_req;
static uint16_t slave_frames;

slave_state_t *cfgsrv_state(void)
{
    return &st;
}

void cfgsrv_init(void)
{
    memset(&st, 0, sizeof st);
    st.ctrl = cfg()->comm.brightness & 7u;
    config_mode = false;
    last_cmd = cmd_result = last_error = 0;
    ident_until = 0;
    reset_at = 0;
}

bool cfgsrv_config_mode(void)
{
    return config_mode;
}

void cfgsrv_enter_config(void)
{
    config_mode = true;
    last_req = hw_millis();
}

bool cfgsrv_identify_active(uint32_t now)
{
    return ident_until && (int32_t)(ident_until - now) > 0;
}

bool cfgsrv_reset_due(uint32_t now)
{
    return reset_at && (int32_t)(now - reset_at) >= 0;
}

void cfgsrv_on_key(uint8_t kc, uint8_t sw)
{
    (void)sw;
    st.key_code = kc;
    st.key_count++;
}

void cfgsrv_set_keys(uint32_t raw)
{
    st.keys_lo = (uint16_t)raw;
    st.keys_ext = (uint16_t)((raw >> 16) & 0xFFu);
}

void cfgsrv_set_entered(int32_t v, uint8_t obj)
{
    st.val_hi = (uint16_t)((uint32_t)v >> 16);
    st.val_lo = (uint16_t)v;
    st.flags = (uint16_t)(FLG_NEW_VALUE | ((uint16_t)obj << 8));
}

void cfgsrv_note_request(uint32_t now)
{
    last_req = now;
    slave_frames++;
}

uint32_t cfgsrv_last_request(void)
{
    return last_req;
}

/* ------------------------------------------------------------------ đọc */
static uint16_t info_reg(uint16_t a)
{
    switch (a) {
    case 100: return 0x504Bu;                               /* 'PK' */
    case 101: return FW_MODEL_ID;
    case 102: return (uint16_t)((FW_VER_MAJOR << 8) | FW_VER_MINOR);
    case 103: return FW_VER_PATCH;
    case 104: return RT_SCHEMA;
    case 105: return (uint16_t)(hw_uid32() >> 16);
    case 106: return (uint16_t)hw_uid32();
    case 107: {
        uint16_t caps = 0x0006u;                            /* b1 master, b2 slave */
        if (config_mode)
            caps |= 0x0010u;
        return caps;
    }
    case 108: return RT_MAX_PAGES;
    case 109: return (uint16_t)((RT_MAX_FKEYS << 8) | RT_MAX_LEDS);
    case 110: return RT_BLOB_SIZE;
    case 111: return (uint16_t)(STORAGE_BANK_SIZE - 64u);
    case 112: {
        uint16_t s = 0;
        if (cfg_load_status() == 0u) s |= 0x01u;
        if (cfg()->comm.mode == RT_MODE_MASTER && !engine_comm_ok()) s |= 0x02u;
        if (cfg_dirty()) s |= 0x04u;
        if (config_mode) s |= 0x08u;
        if (hw_power_ok()) s |= 0x10u;
        return s;
    }
    case 113: return cmd_result;
    case 114: return last_error;
    case 115: return (uint16_t)(hw_millis() / 1000u);
    case 116: return (uint16_t)storage_seq();
    case 117: return cfg()->revision;
    case 120: return last_cmd;
    case 121: return cmd_result;
    case 130: return mbm_stats()->ok;
    case 131: return mbm_stats()->timeouts;
    case 132: return mbm_stats()->crc_err;
    case 133: return mbm_stats()->exceptions;
    case 134: return (uint16_t)((mbm_stats()->last_err << 8) | mbm_stats()->last_exc);
    case 135: return slave_frames;
    default:  return 0;
    }
}

static uint8_t rd_reg(uint16_t a, uint16_t *v, bool input)
{
    (void)input;
    if (a <= 28u) {
        switch (a) {
        case REG_KEY_CODE:  *v = st.key_code; break;
        case REG_KEY_COUNT: *v = st.key_count; break;
        case REG_VAL_HI:    *v = st.val_hi; break;
        case REG_VAL_LO:    *v = st.val_lo; break;
        case REG_FLAGS:     *v = st.flags; break;
        case REG_KEYS_LO:   *v = st.keys_lo; break;
        case REG_KEYS_EXT:  *v = st.keys_ext; break;
        case REG_TOP_HI:    *v = (uint16_t)((uint32_t)st.top >> 16); break;
        case REG_TOP_LO:    *v = (uint16_t)st.top; break;
        case REG_BOT_HI:    *v = (uint16_t)((uint32_t)st.bot >> 16); break;
        case REG_BOT_LO:    *v = (uint16_t)st.bot; break;
        case REG_LED:       *v = st.led; break;
        case REG_CTRL:      *v = (uint16_t)(st.ctrl & ~(CTRL_BEEP_SHORT | CTRL_BEEP_LONG)); break;
        case REG_FMT:       *v = st.fmt; break;
        default:
            if (a >= REG_RAW0 && a < REG_RAW0 + 8u)
                *v = st.seg[a - REG_RAW0];
            else
                *v = 0;
            break;
        }
        return 0;
    }
    if (a >= REG_INFO && a <= 137u) {
        *v = info_reg(a);
        return 0;
    }
    if (a >= REG_WINDOW && a < REG_WINDOW + WINDOW_REGS) {
        uint16_t off = (uint16_t)((a - REG_WINDOW) * 2u);
        const uint8_t *b = (const uint8_t *)cfg();
        uint8_t hi = off < RT_BLOB_SIZE ? b[off] : 0u;
        uint8_t lo = (off + 1u) < RT_BLOB_SIZE ? b[off + 1u] : 0u;
        *v = (uint16_t)((hi << 8) | lo);
        return 0;
    }
    return MB_EX_ADDRESS;
}

/* ------------------------------------------------------------------ ghi */
static uint8_t chk_wr_reg(uint16_t a, uint16_t v)
{
    (void)v;
    if (a == REG_FLAGS || (a >= REG_TOP_HI && a <= REG_FMT) || (a >= REG_RAW0 && a < REG_RAW0 + 8u) || a == REG_CMD)
        return 0;
    if (a >= REG_WINDOW && a < REG_WINDOW + WINDOW_REGS)
        return config_mode ? 0 : MB_EX_DEVICE;      /* chỉ ghi cấu hình khi đang ở chế độ cấu hình */
    return MB_EX_ADDRESS;
}

static void run_command(uint16_t cmd)
{
    last_cmd = cmd;
    cmd_result = CR_OK;
    switch (cmd) {
    case CMD_APPLY:
        if (!config_mode) { cmd_result = CR_NOT_CONFIG; break; }
        cmd_result = cfg_apply();
        break;
    case CMD_RELOAD:
        if (!config_mode) { cmd_result = CR_NOT_CONFIG; break; }
        cfg_load();
        break;
    case CMD_FACTORY:
        if (!config_mode) { cmd_result = CR_NOT_CONFIG; break; }
        cfg_factory();
        cmd_result = cfg_save();
        break;
    case CMD_IDENTIFY:
        ident_until = hw_millis() + 3000u;
        buzzer_identify(3000u);
        break;
    case CMD_EXIT:
        reset_at = hw_millis() + 80u;
        break;
    case CMD_ENTER_CONFIG:
        cfgsrv_enter_config();
        break;
    case CMD_CLEAR_DIAG:
        slave_frames = 0;
        break;
    default:
        cmd_result = CR_UNKNOWN;
        break;
    }
    if (cmd_result != CR_OK)
        last_error = cmd_result;
}

static uint8_t wr_reg(uint16_t a, uint16_t v)
{
    switch (a) {
    case REG_FLAGS: st.flags = v; return 0;
    case REG_TOP_HI: st.top = (int32_t)(((uint32_t)v << 16) | (uint16_t)st.top); return 0;
    case REG_TOP_LO: st.top = (int32_t)(((uint32_t)st.top & 0xFFFF0000u) | v); return 0;
    case REG_BOT_HI: st.bot = (int32_t)(((uint32_t)v << 16) | (uint16_t)st.bot); return 0;
    case REG_BOT_LO: st.bot = (int32_t)(((uint32_t)st.bot & 0xFFFF0000u) | v); return 0;
    case REG_LED: st.led = v; return 0;
    case REG_CTRL:
        st.ctrl = (uint16_t)(v & (0x0007u | CTRL_KEYLOCK));
        if (v & CTRL_BEEP_SHORT)
            buzzer_force(60);
        if (v & CTRL_BEEP_LONG)
            buzzer_force(500);
        return 0;
    case REG_FMT: st.fmt = v; return 0;
    case REG_CMD: run_command(v); return 0;
    default:
        break;
    }
    if (a >= REG_RAW0 && a < REG_RAW0 + 8u) {
        st.seg[a - REG_RAW0] = (uint8_t)v;
        return 0;
    }
    if (a >= REG_WINDOW && a < REG_WINDOW + WINDOW_REGS) {
        uint16_t off = (uint16_t)((a - REG_WINDOW) * 2u);
        uint8_t *b = (uint8_t *)cfg_mut();
        if (off < RT_BLOB_SIZE)
            b[off] = (uint8_t)(v >> 8);
        if ((off + 1u) < RT_BLOB_SIZE)
            b[off + 1u] = (uint8_t)v;
        cfg_touch();
        return 0;
    }
    return MB_EX_ADDRESS;
}

static uint8_t rd_bit(uint16_t a, uint8_t *v, bool discrete)
{
    (void)discrete;
    if (a >= 16u)
        return MB_EX_ADDRESS;
    *v = (uint8_t)((st.led >> a) & 1u);             /* cuộn dây 0..7 LED, 8..15 nhấp nháy */
    return 0;
}

static uint8_t wr_bit(uint16_t a, uint8_t v)
{
    if (a < 16u) {
        if (v)
            st.led |= (uint16_t)(1u << a);
        else
            st.led &= (uint16_t)~(1u << a);
        return 0;
    }
    if (a == 16u) {                                 /* cuộn dây 16: bíp ngắn */
        if (v)
            buzzer_force(60);
        return 0;
    }
    return MB_EX_ADDRESS;
}

static const mbs_ops_t ops = { rd_reg, chk_wr_reg, wr_reg, rd_bit, wr_bit };

const mbs_ops_t *cfgsrv_ops(void)
{
    return &ops;
}
