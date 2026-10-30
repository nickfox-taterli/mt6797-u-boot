// SPDX-License-Identifier: GPL-2.0+
#include <config.h>
#include <blk.h>
#include <cpu_func.h>
#include <debug_uart.h>
#include <hang.h>
#include <init.h>
#include <image.h>
#include <memalign.h>
#include <mmc.h>
#include <part.h>
#include <spl.h>
#include <asm/cache.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/sizes.h>

DECLARE_GLOBAL_DATA_PTR;

extern int x20_hw_init(void);
extern void x20_hw_storage_init(void);
extern void x20_hw_trustzone_init(void);
extern void mtk_uart_init(unsigned int clock, unsigned int baud);
extern void __noreturn x20_enter_bl31(ulong bl33);

#define PART_MAGIC	0x58881688
#define EXT_MAGIC	0x58891689
#define BL33_ADDR	0x46000000
#define TEE_ADDR	0x001005c0
#define BL31_ADDR	0x00100800

static int dram_check(void)
{
	static const ulong addresses[] = {0x40000000, 0x48000000, 0xbf000000};
	unsigned int i, j, pass;

	if (readl((void *)0x10203000) != 0x20102017)
		return -EINVAL;
	for (pass = 0; pass < 2; pass++) {
		for (i = 0; i < ARRAY_SIZE(addresses); i++) {
			volatile u32 *p = (void *)addresses[i];

			for (j = 0; j < 256; j++)
				p[j] = (addresses[i] + j * 4) ^
					(pass ? 0xaaaaaaaa : 0x55555555);
		}
		dsb();
		for (i = 0; i < ARRAY_SIZE(addresses); i++) {
			volatile u32 *p = (void *)addresses[i];

			for (j = 0; j < 256; j++)
				if (p[j] != ((addresses[i] + j * 4) ^
					    (pass ? 0xaaaaaaaa : 0x55555555)))
					return -EIO;
		}
	}
	return 0;
}

void x20_spl_fatal(const char *stage, int code)
{
	printascii("SPL: ");
	printascii(stage);
	printascii(" failed 0x");
	printhex8(code);
	printascii("\n");
	hang();
}

void board_init_f(ulong dummy)
{
	int ret;

	/* Leave the BootROM-inherited watchdog off while initializing DRAM. */
	writel(0x22000000, (void *)0x10007000);
	mtk_uart_init(26000000, 921600);
	ret = x20_hw_init();
	if (ret)
		x20_spl_fatal("power/DRAM", ret);
	ret = dram_check();
	if (ret)
		x20_spl_fatal("DRAM check", ret);
	gd->ram_size = 0x80000000;
	ret = spl_early_init();
	if (ret)
		x20_spl_fatal("driver model", ret);
	preloader_console_init();
	puts("SPL: MT6797 X20-96, DRAM 2048 MiB\n");
}

u32 spl_boot_device(void)
{
	return BOOT_DEVICE_BOARD;
}

static int load_partition(struct blk_desc *desc, const char *partition,
			  const char *image, ulong address, u32 maximum,
			  u32 *size)
{
	struct disk_partition part;
	u32 *header;
	u32 data_size, load_addr;
	lbaint_t blocks;
	ALLOC_CACHE_ALIGN_BUFFER(u8, buffer, 512);

	if (part_get_info_by_name(desc, partition, &part) < 0)
		return -ENOENT;
	if (part.blksz != 512 || blk_dread(desc, part.start, 1, buffer) != 1)
		return -EIO;
	header = (u32 *)buffer;
	data_size = le32_to_cpu(header[1]);
	load_addr = le32_to_cpu(header[10]);
	if (le32_to_cpu(header[0]) != PART_MAGIC ||
	    strncmp((char *)buffer + 8, image, 32) ||
	    le32_to_cpu(header[12]) != EXT_MAGIC ||
	    le32_to_cpu(header[13]) != 512 ||
	    le32_to_cpu(header[14]) != 1 ||
	    le32_to_cpu(header[18]) || le32_to_cpu(header[19]) ||
	    (load_addr != 0xffffffff && load_addr != address) ||
	    !data_size || data_size > maximum)
		return -EINVAL;
	blocks = DIV_ROUND_UP(data_size, 512);
	if (blocks >= part.size)
		return -EFBIG;
	if (blk_dread(desc, part.start + 1, blocks, (void *)address) != blocks)
		return -EIO;
	*size = data_size;
	return 0;
}

static int x20_mmc_load(struct spl_image_info *info,
			struct spl_boot_device *bootdev)
{
	struct blk_desc *desc;
	struct mmc *mmc;
	u32 size;
	int ret;

	x20_hw_storage_init();
	ret = mmc_initialize(NULL);
	if (ret)
		return ret;
	mmc = find_mmc_device(0);
	if (!mmc)
		return -ENODEV;
	ret = mmc_init(mmc);
	if (ret)
		return ret;
	ret = mmc_switch_part(mmc, 0);
	if (ret)
		return ret;
	desc = mmc_get_blk_desc(mmc);
	ret = load_partition(desc, "lk", "lk", BL33_ADDR, SZ_512K - 512, &size);
	if (ret)
		x20_spl_fatal("load U-Boot", ret);
	info->size = size;
	ret = load_partition(desc, "tee1", "atf", TEE_ADDR, 81408, &size);
	if (ret)
		x20_spl_fatal("load BL31", ret);
	if (size != 81408 || memcmp((void *)TEE_ADDR, "EET KTM @", 9) ||
	    readl((void *)(TEE_ADDR + 8)) != 0x240)
		x20_spl_fatal("BL31 header", -EINVAL);
	info->os = IH_OS_U_BOOT;
	info->load_addr = BL33_ADDR;
	info->entry_point = BL33_ADDR;
	puts("SPL: eMMC U-Boot + BL31 loaded\n");
	return 0;
}
SPL_LOAD_IMAGE_METHOD("X20 eMMC", 0, BOOT_DEVICE_BOARD, x20_mmc_load);

void __noreturn jump_to_image(struct spl_image_info *info)
{
	x20_hw_trustzone_init();
	puts("SPL: BL31\n");
	/* The image was written by PIO with D-cache off; invalidate stale I-cache
	 * before requesting the same AArch64 warm reset used by the Preloader. */
	invalidate_icache_all();
	dsb();
	x20_enter_bl31(info->entry_point);
}

void reset_cpu(void)
{
	writel(0x22000014, (void *)0x10007000);
	writel(0x1209, (void *)0x10007014);
	while (1)
		asm volatile("wfi");
}
