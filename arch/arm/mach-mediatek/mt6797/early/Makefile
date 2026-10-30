# Hardware code imported from the verified X20 Preloader; original notices apply.
hw-objects := pll.o gpio.o pmic_wrap_init.o pmic.o i2c.o rtc.o emi.o \
	dramc_pi_main.o dramc_pi_basic_api.o dramc_pi_calibration_api.o \
	Hal_io.o memory.o timer.o uart.o da9214.o fan53555.o fan53526.o \
	mt_ptp.o spm_mtcmos.o wdt.o clkbuf_ctl.o sync_write.o charging_bat.o \
	device_apc.o msdc_io.o cust_msdc.o tz_sec_cfg.o tz_emi_mpu.o

obj-y += hardware.o $(hw-objects)

# Isolate the donor's types and configuration from U-Boot headers. All objects
# are rebuilt from C, with both the donor r8 and U-Boot gd/r9 kept reserved.
hw-includes := generated drivers/inc core/inc security/inc security/trustzone/inc \
	init/inc custom/inc common/inc
hw-root := $(srctree)/$(src)
hw-flags := -Os -g -std=gnu89 -march=armv7-a -mthumb -msoft-float \
	-ffixed-r8 -ffixed-r9 -fno-short-enums -fshort-wchar -ffunction-sections -fdata-sections \
	-fno-strict-aliasing -fno-common -fno-builtin -ffreestanding \
	-mno-unaligned-access -include $(hw-root)/features.h \
	-include $(hw-root)/core/inc/print.h \
	$(addprefix -I$(hw-root)/,$(hw-includes)) \
	-Dget_timer=x20_hw_get_timer -Dudelay=x20_hw_udelay -Dmdelay=x20_hw_mdelay \
	-Dserial_setbrg=x20_hw_serial_setbrg

quiet_cmd_hwcc = HWCC    $@
      cmd_hwcc = $(CC) $(hw-flags) -MMD -MP -MF $(depfile) -c -o $@ $<

$(obj)/%.o: $(src)/drivers/%.c FORCE
	$(call if_changed_dep,hwcc)

$(obj)/hardware.o: $(src)/hardware.c FORCE
	$(call if_changed_dep,hwcc)
