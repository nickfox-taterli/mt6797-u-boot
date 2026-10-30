/* SPDX-License-Identifier: GPL-2.0+ */
/* Glue for the imported, hardware-only X20 initialization routines. */
#include "typedefs.h"
#include "platform.h"
#include "pll.h"
#include "gpio.h"
#include "pmic_wrap_init.h"
#include "pmic.h"
#include "i2c.h"
#include "rtc.h"
#include "timer.h"
#include "emi.h"
#include "mt_ptp.h"
#include "wdt.h"
#include "mmc_core.h"
#include "msdc.h"
#include "tz_mem.h"

u32 g_ddr_reserve_enable;
u32 g_ddr_reserve_success;
boot_mode_t g_boot_mode = NORMAL_BOOT;
boot_reason_t g_boot_reason = BR_POWER_KEY;

unsigned int msdc_uffs(unsigned int value)
{
    return __builtin_ffs(value);
}

/* Read-only eFuse register mapping, matching MT6797's 36 devinfo words.
 * No SecLib/SecPlat binary is used by SPL. */
u32 seclib_get_devinfo_with_index(u32 index)
{
    static const u32 addresses[] = {
        0x10206020, 0x10206030, 0x10206038, 0x10206040,
        0x10206044, 0x10206048, 0x10206060, 0x10206180,
        0x10206184, 0x10206188, 0x10206120, 0x10206130,
        0x10206140, 0x10206144, 0x1020618c, 0x10206190,
        0x10206194, 0x1020671c, 0x10206720, 0x10206710,
        0x08000000, 0x1020604c, 0x10206050, 0x10206054,
        0x10206058, 0x10206198, 0x1020619c, 0x102061a0,
        0x102061a4, 0x102061a8, 0x10206714, 0x10206718,
        0x10206724, 0x102068d8, 0x08000008, 0x102066bc,
    };

    if (index >= sizeof(addresses) / sizeof(addresses[0]))
        return 0xffffffff;
    return DRV_Reg32(addresses[index]);
}

chip_ver_t platform_chip_ver(void)
{
    return DRV_Reg32(APHW_VER) == 0xca00 ? CHIP_INFO_1 : CHIP_INFO_2;
}

extern void x20_spl_fatal(const char *stage, int code);

void platform_assert(char *file, int line, char *expr)
{
    /* Keep an actionable failure location; no calibration trace is emitted. */
    x20_spl_fatal(file, line);
}

int x20_hw_init(void)
{
    int ret;

    mtk_timer_init();
    mt_pll_init();
    mtk_uart_init(UART_SRC_CLK_FRQ, CFG_LOG_BAUDRATE);
    mt_gpio_init();
    ret = pwrap_init_preloader();
    if (ret)
        return ret;
    i2c_hw_init();
    ret = pmic_init();
    if (ret)
        return ret;
    mt_pll_post_init();
    PMIC_enable_long_press_reboot();

    /* Same MP2 isolation and rail-off state as the verified Preloader. */
    DRV_WriteReg32(SPM_BASE + 0x290, DRV_Reg32(SPM_BASE + 0x290) | 2);
    da9214_config_interface(0, 0, 0xf, 0);
    da9214_config_interface(0x5e, 0, 1, 0);
    DRV_WriteReg32(0x102222b0, DRV_Reg32(0x102222b0) & ~0xf0);
    ptp_init();

    check_ddr_reserve_status();
    rtc_bbpu_power_on();
    mt_mem_init();
    return 0;
}

void x20_hw_storage_init(void)
{
    struct mmc_host host;

    memset(&host, 0, sizeof(host));
    host.id = 0;
    host.cur_pwr = VOL_3000;
    msdc_gpio_and_pad_init(&host);
    msdc_clock(&host, 1);
    msdc_card_power(&host, 1);
}

void x20_hw_trustzone_init(void)
{
    device_APC_dom_setup();
    tz_dapc_sec_init();
    tz_dapc_sec_postinit();
    tz_sram_sec_init(0x12a000);
    /* BL31 lives in SRAM; no secure DRAM window is needed at 0x44600000. */
}
