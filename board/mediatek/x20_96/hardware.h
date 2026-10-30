/* SPDX-License-Identifier: GPL-2.0+ */
#ifndef __X20_HARDWARE_H
#define __X20_HARDWARE_H

#include <linux/types.h>

int x20_pmic_update(u16 reg, u16 mask, u16 value);
int x20_tf_prepare(void);
void x20_usb_route(bool device);

#endif
