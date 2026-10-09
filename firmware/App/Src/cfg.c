#include "cfg.h"
#include <string.h>

static rt_blob_t g_cfg;
static uint8_t   load_status;
static bool      dirty;

const rt_blob_t *cfg(void)
{
    return &g_cfg;
}

rt_blob_t *cfg_mut(void)
{
    return &g_cfg;
}

bool cfg_load(void)
{
    dirty = false;
    st_result_t r = storage_load(&g_cfg);
    if (r == ST_OK) {
        load_status = 0;
        return true;
    }
    rtcfg_defaults(&g_cfg);
    load_status = storage_blank() ? 1 : 2;
    return false;
}

uint8_t cfg_load_status(void)
{
    return load_status;
}

void cfg_factory(void)
{
    rtcfg_defaults(&g_cfg);
    dirty = true;
}

static uint8_t commit(bool seal)
{
    if (seal)
        rtcfg_seal(&g_cfg);
    uint8_t v = rtcfg_validate(&g_cfg);
    if (v != RT_OK)
        return v;
    st_result_t r = storage_save(&g_cfg);
    if (r != ST_OK)
        return (uint8_t)(0x10u + r);
    dirty = false;
    return 0;
}

uint8_t cfg_save(void)
{
    return commit(true);
}

uint8_t cfg_apply(void)
{
    return commit(false);
}

bool cfg_dirty(void)
{
    return dirty;
}

void cfg_touch(void)
{
    dirty = true;
}
