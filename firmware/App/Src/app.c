#include "app.h"
#include "main.h"
#include "config.h"
#include "regs.h"
#include "panel.h"
#include "modbus_rtu.h"
#include "stm32g0xx_ll_iwdg.h"

/*
 * IWDG bật bằng LL trong app (không qua CubeMX) để chỉ chạy sau lamp test,
 * và không phụ thuộc HAL_IWDG_MODULE_ENABLED khi sinh lại code.
 */
static void watchdog_start(void)
{
#if IWDG_ENABLE
    __HAL_DBGMCU_FREEZE_IWDG();               /* dừng IWDG khi CPU halt lúc debug */
    LL_IWDG_Enable(IWDG);                     /* LSI ~32 kHz */
    LL_IWDG_EnableWriteAccess(IWDG);
    LL_IWDG_SetPrescaler(IWDG, LL_IWDG_PRESCALER_32);  /* ~1 kHz */
    LL_IWDG_SetReloadCounter(IWDG, IWDG_TIMEOUT_MS);
    while (!LL_IWDG_IsReady(IWDG)) {}
    LL_IWDG_ReloadCounter(IWDG);
#endif
}

void app_init(void)
{
    regs_init();
    panel_init();     /* lamp test rồi hiển thị giá trị mặc định 0 */
    mb_rtu_init();
    watchdog_start();
}

void app_loop(void)
{
#if IWDG_ENABLE
    LL_IWDG_ReloadCounter(IWDG);
#endif
    mb_rtu_poll();
    panel_poll(HAL_GetTick());
}
