/* SPDX-License-Identifier: GPL-2.0+ */
#ifndef __X20_96_H
#define __X20_96_H

#ifdef CONFIG_TARGET_MT6797_SPL
#define CFG_SYS_TIMER_RATE 13000000
#define CFG_SYS_TIMER_COUNTER 0x10008048
#endif

#define CFG_EXTRA_ENV_SETTINGS \
	"kernel_addr_r=0x48000000\0" \
	"fdt_addr_r=0x44000000\0" \
	"ramdisk_addr_r=0x48000000\0" \
	"loadaddr=0x48000000\0" \
	"kernel_comp_addr_r=0x50000000\0" \
	"kernel_comp_size=0x4000000\0" \
	"x20_boot=if mmc dev 0 && " \
	"ext4load mmc 0:3 ${kernel_addr_r} /boot/u-boot/Image.gz && " \
	"ext4load mmc 0:3 ${fdt_addr_r} /boot/u-boot/x20-96.dtb; " \
	"then booti ${kernel_addr_r} - ${fdt_addr_r}; " \
	"else echo X20 boot files unavailable; false; fi\0"

#endif
