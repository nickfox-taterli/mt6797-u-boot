# SPDX-License-Identifier: GPL-2.0+
ifeq ($(CONFIG_TARGET_MT6797_SPL),y)
obj-$(CONFIG_XPL_BUILD) += spl.o transition.o early/
ifneq ($(CONFIG_XPL_BUILD),y)
obj-y += init.o
endif
else
obj-y += init.o
endif
