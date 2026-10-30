// SPDX-License-Identifier: GPL-2.0-only
/* MT6797 USB2 sequence adapted from the verified Linux mtk-tphy port. */
#include <dm.h>
#include <generic-phy.h>
#include <asm/io.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include "hardware.h"

struct x20_usb_phy {
	void __iomem *base;
	bool powered;
};

static int x20_phy_power_on(struct phy *phy)
{
	struct x20_usb_phy *priv = dev_get_priv(phy->dev);
	void __iomem *base = priv->base;
	void __iomem *com = base + 0x800;
	u32 fm, calibration;
	int ret, i;

	if (priv->powered)
		return 0;
	ret = x20_pmic_update(0xa16, BIT(1), BIT(1));
	if (ret)
		return ret;
	ret = x20_pmic_update(0xb10, 7 << 8, 1 << 8);
	if (ret)
		return ret;
	ret = x20_pmic_update(0xa6e, BIT(1), BIT(1));
	if (ret)
		return ret;
	/* VA10 -> ref, bus, sys clocks -> analog VUSB10, as on Linux. */
	setbits_le32((void *)0x100000b0, BIT(8));
	writel(BIT(9), (void *)0x100010ac);
	writel(BIT(18), (void *)0x100010ac);
	writel(BIT(24), (void *)0x100010ac);
	udelay(50);
	setbits_le32(base + 0xb04, BIT(29));
	x20_usb_route(true);
	clrbits_le32(com + 0x18, BIT(31));
	clrbits_le32(com + 0x68, BIT(26) | BIT(18));
	clrbits_le32(com + 0x6c, BIT(16));
	clrbits_le32(com + 0x20, BIT(9) | BIT(8) | BIT(17) | BIT(16));
	setbits_le32(com + 0x20, BIT(18));
	clrbits_le32(com + 0x18, BIT(23));
	setbits_le32(com + 0x14, BIT(11));
	clrsetbits_le32(com + 0x18, 0xf, BIT(20) | 2);
	clrsetbits_le32(com + 0x70, GENMASK(19, 16), 9 << 16);
	udelay(800);
	setbits_le32(com + 0x6c, BIT(13) | BIT(12) | BIT(10));
	clrbits_le32(com + 0x68,
		     BIT(23) | BIT(21) | BIT(20) | BIT(19) | BIT(17) |
		     GENMASK(13, 10) | BIT(7) | BIT(6) | GENMASK(5, 4) | BIT(2));
	clrsetbits_le32(base + 0xc00, GENMASK(11, 10), 2 << 10);
	setbits_le32(base + 0x18, BIT(9));
	setbits_le32(base + 0x310, 0xf);
	clrsetbits_le32(base + 0xb00, GENMASK(15, 10), 0x2d << 10);
	writel(0, base + 0x30c);
	udelay(800);
	clrsetbits_le32(com + 0x6c, BIT(4), BIT(5) | BIT(2));
	/* This board uses a fixed peripheral route, including the PHY ID pin. */
	setbits_le32(com + 0x6c, BIT(9) | BIT(1));
	/* Bounded slew calibration, with the BSP's conservative fallback. */
	setbits_le32(com + 0x14, BIT(15));
	udelay(1);
	setbits_le32(base + 0x110, BIT(8));
	clrsetbits_le32(base + 0x100, GENMASK(23, 0), 1024);
	setbits_le32(base + 0x100, BIT(24));
	fm = 0;
	for (i = 0; i < 10; i++) {
		mdelay(1);
		fm = readl(base + 0x10c);
		if (fm)
			break;
	}
	clrbits_le32(base + 0x100, BIT(24));
	clrbits_le32(base + 0x110, BIT(8));
	calibration = fm ? (1024 * 26 * 28) / ((u64)fm * 1000) : 4;
	clrsetbits_le32(com + 0x14, GENMASK(14, 12), (calibration & 7) << 12);
	clrbits_le32(com + 0x14, BIT(15));
	priv->powered = true;
	return 0;
}

static int x20_phy_power_off(struct phy *phy)
{
	struct x20_usb_phy *priv = dev_get_priv(phy->dev);

	if (!priv->powered)
		return 0;
	clrbits_le32(priv->base + 0xb04, BIT(29));
	udelay(10);
	writel(BIT(24) | BIT(18) | BIT(9), (void *)0x100010a8);
	x20_pmic_update(0xa6e, BIT(1), 0);
	x20_pmic_update(0xa16, BIT(1), 0);
	x20_usb_route(false);
	priv->powered = false;
	return 0;
}

static int x20_phy_probe(struct udevice *dev)
{
	struct x20_usb_phy *priv = dev_get_priv(dev);

	priv->base = dev_read_addr_ptr(dev);
	return priv->base ? 0 : -EINVAL;
}

static const struct phy_ops x20_phy_ops = {
	.power_on = x20_phy_power_on,
	.power_off = x20_phy_power_off,
};

static const struct udevice_id x20_phy_ids[] = {
	{ .compatible = "archermind,x20-96-usb2-phy" },
	{ }
};

U_BOOT_DRIVER(x20_usb2_phy) = {
	.name = "x20-usb2-phy",
	.id = UCLASS_PHY,
	.of_match = x20_phy_ids,
	.probe = x20_phy_probe,
	.ops = &x20_phy_ops,
	.priv_auto = sizeof(struct x20_usb_phy),
};
