/* SPDX-License-Identifier: GPL-2.0+ */
#include <linux/linkage.h>

/* MT6797's BootROM starts AArch32. Preserve its verified warm-reset
 * transition to AArch64 BL31; r4/r5 are optional tags, r6 is BL33. */
ENTRY(x20_enter_bl31)
	mov r6, r0
	mov r4, #0
	mov r5, #0
	mov r7, #0x00100000
	ldr r0, =0x00101000
	/* MCUSYS_CFGREG_BASE + 0x38 (not the PLL helper's old MCUCFG alias). */
	ldr r1, =0x10220038
	str r0, [r1]
	mrc p15, 0, r0, c12, c0, 2
	orr r0, r0, #1
	mcr p15, 0, r0, c12, c0, 2
	dsb
	isb
	mrc p15, 0, r0, c12, c0, 2
	orr r0, r0, #2
	mcr p15, 0, r0, c12, c0, 2
	dsb
	isb
	mov r0, #0xc0000000
1:
	wfi
	b 1b
ENDPROC(x20_enter_bl31)
