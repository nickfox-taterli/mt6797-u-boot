// SPDX-License-Identifier: GPL-2.0+
#include <init.h>
#include <fdtdec.h>
#include <asm/io.h>
#include <asm/system.h>

int dram_init(void)
{
	/* Training is performed by SPL or the recovery Preloader. */
	return fdtdec_setup_mem_size_base();
}

int dram_init_banksize(void)
{
	return fdtdec_setup_memory_banksize();
}

int board_early_init_f(void)
{
	/* Allow interactive bring-up before the DM watchdog is available. */
	writel(0x22000000, (void __iomem *)0x10007000);
	/* No SCP firmware is loaded, matching the working LK handoff. */
	writel(0xffff, (void __iomem *)0x100a402c);
	writel(1, (void __iomem *)0x100a4000);
	writel(0, (void __iomem *)0x100a4004);
	writel(0, (void __iomem *)0x100a0000);
	writel(0x100, (void __iomem *)0x100a4020);
	return 0;
}

void reset_cpu(void)
{
#ifdef CONFIG_ARM64
	psci_system_reset();
#else
	writel(0x22000014, (void __iomem *)0x10007000);
	writel(0x1209, (void __iomem *)0x10007014);
	while (1)
		asm volatile("wfi");
#endif
}

int print_cpuinfo(void)
{
	printf("Machine: MediaTek X20 Development Board\n");
	printf("SoC:   MediaTek Helio X20 (MT6797)\n");
	return 0;
}
