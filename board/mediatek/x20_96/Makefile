# SPDX-License-Identifier: GPL-2.0+
obj-y += x20_96.o
ifeq ($(CONFIG_TARGET_MT6797),y)
obj-y += hardware.o
obj-$(CONFIG_USB_GADGET) += usb-phy.o
endif
