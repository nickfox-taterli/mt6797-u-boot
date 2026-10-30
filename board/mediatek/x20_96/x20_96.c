// SPDX-License-Identifier: GPL-2.0+
#include <init.h>
#ifdef CONFIG_TARGET_MT6797
#include <command.h>
#include <fs.h>
#include <mmc.h>
#include <asm/global_data.h>
#if CONFIG_IS_ENABLED(USB_GADGET)
#include <env.h>
#include <g_dnl.h>
#endif
#endif
#include "hardware.h"

#ifdef CONFIG_TARGET_MT6797
DECLARE_GLOBAL_DATA_PTR;

#if CONFIG_IS_ENABLED(USB_GADGET)
int g_dnl_bind_fixup(struct usb_device_descriptor *desc, const char *name)
{
	struct mmc *mmc;
	char serial[33];

	/* Derive a stable identifier without adding work to normal autoboot. */
	if (!env_get("serial#")) {
		mmc = find_mmc_device(0);
		if (mmc && !mmc_init(mmc)) {
			snprintf(serial, sizeof(serial), "%08x%08x%08x%08x",
				 mmc->cid[0], mmc->cid[1], mmc->cid[2], mmc->cid[3]);
			env_set("serial#", serial);
		}
	}
	if (env_get("serial#"))
		g_dnl_set_serialnumber((char *)env_get("serial#"));
	g_dnl_set_product("MediaTek X20 Development Board");
	return 0;
}
#endif

static int do_tfboot(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	struct mmc *mmc;
	bool disabled = gd->flags & GD_FLG_DISABLE_CONSOLE;
	int ret;

	/* A removable card or boot configuration may be absent. Keep that
	 * expected probe failure quiet and always restore the caller's console. */
	gd->flags |= GD_FLG_DISABLE_CONSOLE;
	mmc = find_mmc_device(1);
	ret = mmc ? mmc_init(mmc) : -ENODEV;
	if (!ret)
		ret = fs_set_blk_dev("mmc", "1:1", FS_TYPE_ANY);
	if (!ret && !fs_exists("/boot/extlinux/extlinux.conf"))
		ret = -ENOENT;
	if (!disabled)
		gd->flags &= ~GD_FLG_DISABLE_CONSOLE;
	if (ret)
		return CMD_RET_SUCCESS;

	run_command("sysboot mmc 1:1 any ${pxefile_addr_r} /boot/extlinux/extlinux.conf", 0);
	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(tfboot, 1, 0, do_tfboot,
	   "boot an optional TF card using its extlinux configuration", "");
#endif

int board_init(void)
{
#ifdef CONFIG_TARGET_MT6797
	int ret = x20_tf_prepare();

	if (ret)
		debug("TF power setup unavailable: %d\n", ret);
#endif
	return 0;
}
