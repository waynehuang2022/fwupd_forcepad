/*
 * Copyright (C) 2022 Shihwei Huang <shihwei.huang@focaltech-electronics.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "config.h"

#include <fwupdplugin.h>

#include "fu-focalfp-firmware.h"
#include "fu-focalfp-hid-device.h"
//#include "fu-focalfp-i2c-device.h"

static gboolean
fu_plugin_focalfp_device_created(FuPlugin *plugin, FuDevice *dev, GError **error)
{
	/*if (fu_device_get_specialized_gtype(dev) == FU_TYPE_FOCALFP_I2C_DEVICE &&
	    !fu_context_has_hwid_flag(fu_plugin_get_context(plugin), "focalfp-recovery")) {
		g_set_error_literal(error, FWUPD_ERROR, FWUPD_ERROR_NOT_SUPPORTED, "not required");
		g_print("=forcepad:plugin created return false======================================\n");
		return FALSE;
	}*/
	return TRUE;
}

static void
fu_plugin_focalfp_load(FuContext *ctx)
{
	//fu_context_add_quirk_key(ctx, "FocalfpI2cTargetAddress");
	//fu_context_add_quirk_key(ctx, "FocalfpIapPassword");
	//fu_context_add_quirk_key(ctx, "FocalfpIcPageCount");
}

static void
fu_plugin_focalfp_init(FuPlugin *plugin)
{
        g_print("=forcepad:plugin init=================================================\n");
	//fu_plugin_add_udev_subsystem(plugin, "i2c-dev");
	fu_plugin_add_udev_subsystem(plugin, "hidraw");
	fu_plugin_add_firmware_gtype(plugin, NULL, FU_TYPE_FOCALFP_FIRMWARE);
	//fu_plugin_add_device_gtype(plugin, FU_TYPE_FOCALFP_I2C_DEVICE);
	fu_plugin_add_device_gtype(plugin, FU_TYPE_FOCALFP_HID_DEVICE);
}

void
fu_plugin_init_vfuncs(FuPluginVfuncs *vfuncs)
{
	vfuncs->build_hash = FU_BUILD_HASH;
	//vfuncs->load = fu_plugin_focalfp_load;
	vfuncs->init = fu_plugin_focalfp_init;
	vfuncs->device_created = fu_plugin_focalfp_device_created;
}
