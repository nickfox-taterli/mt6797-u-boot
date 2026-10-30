// SPDX-License-Identifier: GPL-2.0+
#include <asm/io.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/iopoll.h>
#include "hardware.h"

#define PWRAP_BASE ((void __iomem *)0x1000d000)
#define GPIO_BASE ((void __iomem *)0x10005000)
#define IOCFG_B ((void __iomem *)0x10002400)

/* SPL has already configured PWRAP. Use its WACS2 channel without resetting
 * the wrapper or changing the CPU rails managed by BL31. */
static int pmic_wait(unsigned int state, u32 *status)
{
	return readl_poll_timeout(PWRAP_BASE + 0xa4, *status,
				 ((*status >> 16) & 7) == state, 10000);
}

static int pmic_read(u16 reg, u16 *value)
{
	u32 status;
	int ret;

	status = readl(PWRAP_BASE + 0xa4);
	if (!(status & BIT(21)))
		return -EIO;
	if (((status >> 16) & 7) == 6)
		writel(1, PWRAP_BASE + 0xa8);
	ret = pmic_wait(0, &status);
	if (ret)
		return ret;
	writel((reg >> 1) << 16, PWRAP_BASE + 0xa0);
	ret = pmic_wait(6, &status);
	if (ret)
		return ret;
	*value = status & 0xffff;
	writel(1, PWRAP_BASE + 0xa8);
	return 0;
}

int x20_pmic_update(u16 reg, u16 mask, u16 value)
{
	u16 before, after;
	u32 status;
	int ret;

	ret = pmic_read(reg, &before);
	if (ret)
		return ret;
	ret = pmic_wait(0, &status);
	if (ret)
		return ret;
	writel(BIT(31) | ((reg >> 1) << 16) | ((before & ~mask) | (value & mask)),
	       PWRAP_BASE + 0xa0);
	ret = pmic_wait(0, &status);
	if (ret)
		return ret;
	ret = pmic_read(reg, &after);
	if (ret)
		return ret;
	return (after & mask) == (value & mask) ? 0 : -EIO;
}

static void gpio_mode(unsigned int pin, unsigned int mode)
{
	unsigned int shift = (pin % 8) * 4;

	clrsetbits_le32(GPIO_BASE + 0x300 + (pin / 8) * 0x10,
		       0xf << shift, mode << shift);
}

void x20_usb_route(bool device)
{
	/* GPIO86 low routes the single controller to the Micro-USB receptacle. */
	gpio_mode(86, 0);
	writel(BIT(22), GPIO_BASE + 0x120 + (device ? 8 : 4));
	writel(BIT(22), GPIO_BASE + 0x24);
}

int x20_tf_prepare(void)
{
	unsigned int pin;
	int ret;

	/* MT6351 VMC/VMCH: legacy SD signalling at 3.3 V, no UHS switching. */
	ret = x20_pmic_update(0x0ae2, 7 << 8, 7 << 8);
	if (ret)
		return ret;
	ret = x20_pmic_update(0x0ace, BIT(8), BIT(8));
	if (ret)
		return ret;
	ret = x20_pmic_update(0x0aaa, BIT(1), BIT(1));
	if (ret)
		return ret;
	ret = x20_pmic_update(0x0a2e, BIT(1), BIT(1));
	if (ret)
		return ret;
	mdelay(10);
	for (pin = 129; pin <= 134; pin++)
		gpio_mode(pin, 1);
	setbits_le32((void *)0x10002830, 7);
	setbits_le32(IOCFG_B + 0x20, GENMASK(30, 25));
	clrsetbits_le32(IOCFG_B + 0x100, GENMASK(23, 18), BIT(23));
	clrbits_le32(IOCFG_B + 0x110, GENMASK(23, 18));
	setbits_le32(IOCFG_B + 0x120, GENMASK(23, 18));
	clrsetbits_le32(IOCFG_B + 0xc0, GENMASK(27, 16), 0xccc << 16);
	clrsetbits_le32(IOCFG_B + 0xa0, GENMASK(25, 8), 0xc30c << 8);
	clrsetbits_le32(IOCFG_B + 0x1b0, GENMASK(29, 16),
		       (5 << 16) | (6 << 21) | (6 << 24) | (6 << 27));
	/* Keep SPL's MSDCPLL/2 parent (192 MHz), also consumed by Linux.
	 * CLK_CFG registers have shadow latches: changing only the selector
	 * would make software report a rate that the hardware never adopted. */
	clrsetbits_le32((void *)0x10000070, GENMASK(26, 24) | BIT(31), 2 << 24);
	writel(BIT(4), (void *)0x1000108c);
	dsb();
	return 0;
}
