/* Phần cứng còn lại của giao diện hwif.h (chỉ biên dịch cho chip): thời gian, còi, watchdog, flash cấu hình. */
#include "hwif.h"
#include "config.h"
#include "main.h"
#include "stm32g0xx_ll_iwdg.h"
#include <string.h>

uint32_t hw_millis(void)
{
    return HAL_GetTick();
}

void hw_buzzer(bool on)
{
    HAL_GPIO_WritePin(BZ_ON_GPIO_Port, BZ_ON_Pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* ---- watchdog độc lập: LSI ~32 kHz, chia 32 => ~1 tick/ms ---- */
static bool wdt_running;

void hw_wdt_start(void)
{
    LL_IWDG_Enable(IWDG);
    LL_IWDG_EnableWriteAccess(IWDG);
    LL_IWDG_SetPrescaler(IWDG, LL_IWDG_PRESCALER_32);
    LL_IWDG_SetReloadCounter(IWDG, IWDG_TIMEOUT_MS);
    while (!LL_IWDG_IsReady(IWDG)) {
    }
    LL_IWDG_ReloadCounter(IWDG);
    wdt_running = true;
}

void hw_wdt_kick(void)
{
    if (wdt_running)
        LL_IWDG_ReloadCounter(IWDG);
}

void hw_reset(void)
{
    NVIC_SystemReset();
    for (;;) {
    }
}

uint32_t hw_uid32(void)
{
    const uint32_t *u = (const uint32_t *)UID_BASE;
    return u[0] ^ u[1] ^ u[2];
}

/* STM32G030 không có PVD: bảo vệ sụt áp dựa vào POR/PDR (~1,7 V) + lưu cấu hình A/B có CRC */
bool hw_power_ok(void)
{
    return true;
}

/* ---- flash cấu hình ---- */
bool hw_flash_erase_page(uint32_t addr)
{
    FLASH_EraseInitTypeDef e;
    uint32_t err = 0;
    memset(&e, 0, sizeof e);
    e.TypeErase = FLASH_TYPEERASE_PAGES;
    e.Page = (addr - FLASH_BASE) / FLASH_PAGE_SIZE;
    e.NbPages = 1;
    HAL_FLASH_Unlock();
    HAL_StatusTypeDef r = HAL_FLASHEx_Erase(&e, &err);
    HAL_FLASH_Lock();
    return r == HAL_OK;
}

bool hw_flash_write(uint32_t addr, const void *data, uint32_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    bool ok = true;
    HAL_FLASH_Unlock();
    for (uint32_t i = 0; i + 8u <= len && ok; i += 8u) {
        uint64_t dw;
        memcpy(&dw, p + i, 8);
        ok = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, addr + i, dw) == HAL_OK;
    }
    HAL_FLASH_Lock();
    return ok;
}

const uint8_t *hw_flash_ptr(uint32_t addr)
{
    return (const uint8_t *)addr;
}
