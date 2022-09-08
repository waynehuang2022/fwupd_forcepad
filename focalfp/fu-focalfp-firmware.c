/*
 * Copyright (C) 2022 Shihwei Huang <shihwei.huang@focaltech-electronics.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "config.h"

#include <fwupdplugin.h>

#include "fu-focalfp-common.h"
#include "fu-focalfp-firmware.h"

struct _FuFocalfpFirmware {
	FuFirmwareClass parent_instance;
	guint16 module_id;
};

G_DEFINE_TYPE(FuFocalfpFirmware, fu_focalfp_firmware, FU_TYPE_FIRMWARE)

/* firmware block update */
#define FOCAL_NAME_START_ADDR_WRDS 0x011E

/*********************************************************
Define COMM_HID
**********************************************************/
#define COMM_HID_OK				0x00
#define COMM_HID_INVLID_PARAM			0x01
#define COMM_HID_INVLID_DEVICE                  0x02
#define COMM_HID_INVALID_BUFFER_SIZE            0x03
#define COMM_HID_WRITE_USB_ERROR                0x04
#define COMM_HID_READ_USB_ERROR                 0x05
#define COMM_HID_FIND_NO_DEVICE                 0x06    
#define COMM_HID_PACKET_COMMAND_ERROR           0x07
#define COMM_HID_PACKET_DATA_ERROR              0x08
#define COMM_HID_PACKET_CHECKSUM_ERROR          0x09

const guint8 focalfp_signature[] = {0xFF};

guint16
fu_focalfp_firmware_get_module_id(FuFocalfpFirmware *self)
{
	g_return_val_if_fail(FU_IS_FOCALFP_FIRMWARE(self), 0);
	return self->module_id;
}

static void
fu_focalfp_firmware_export(FuFirmware *firmware, FuFirmwareExportFlags flags, XbBuilderNode *bn)
{
	FuFocalfpFirmware *self = FU_FOCALFP_FIRMWARE(firmware);
	fu_xmlb_builder_insert_kx(bn, "module_id", self->module_id);
}

static gboolean
fu_focalfp_firmware_check_magic(FuFirmware *firmware, GBytes *fw, gsize offset, GError **error)
{
	gsize bufsz = g_bytes_get_size(fw);
	const guint8 *buf = g_bytes_get_data(fw, NULL);                                              
	for (gsize i = 0; i < sizeof(focalfp_signature); i++) {
		guint8 tmp = 0x0;
		if (!fu_memread_uint8_safe(buf,
					   bufsz,
					   bufsz - sizeof(focalfp_signature) + i,
					   &tmp,
					   error)){
			return FALSE;
		}
		if (tmp != focalfp_signature[i]) {
			g_set_error(error,
				    FWUPD_ERROR,
				    FWUPD_ERROR_INVALID_FILE,
				    "signature[%u] invalid: got 0x%2x, expected 0x%02x",
				    (guint)i,
				    tmp,
				    focalfp_signature[i]);
			return FALSE;
		}
	}
        g_print("=forcepad:firmware check_magic true==================\n");      
	/* success */
	return TRUE;
}

static gboolean
fu_focalfp_firmware_parse(FuFirmware *firmware,
			 GBytes *fw,
			 guint64 addr_start,
			 guint64 addr_end,
			 FwupdInstallFlags flags,
			 GError **error)
{
	FuFocalfpFirmware *self = FU_FOCALFP_FIRMWARE(firmware);
	gsize bufsz = 0;
	guint16 force_addr_wrds;
	guint16 module_id_wrds;
	const guint8 *buf = g_bytes_get_data(fw, &bufsz);
        //g_print("=forcepad:firmware parse new=%d=======================\n",bufsz);
	/* presumably in words */
	
	if (!fu_common_read_uint16_safe(buf,
				    bufsz,
				    FOCAL_NAME_START_ADDR_WRDS,
				    &force_addr_wrds,
				    G_LITTLE_ENDIAN,
				    error)){
		return FALSE;
	}
	if (force_addr_wrds !=0x2e58) {
		g_set_error(error,
			    FWUPD_ERROR,
			    FWUPD_ERROR_INVALID_FILE,
			    "Force pad address invalid: 0x%x",
			    force_addr_wrds);
		return FALSE;
	}else{
	        if (!fu_common_read_uint16_safe(buf,
			    bufsz,
			    FOCAL_NAME_START_ADDR_WRDS,
			    &self->module_id,
			    G_LITTLE_ENDIAN,
			    error))
		return FALSE;       
	        
	}
  	
	/* whole image */
	fu_firmware_set_bytes(firmware, fw);		
	return TRUE;
}

static gboolean
fu_focalfp_firmware_build(FuFirmware *firmware, XbNode *n, GError **error)
{
	FuFocalfpFirmware *self = FU_FOCALFP_FIRMWARE(firmware);
	guint64 tmp;
	/* simple properties */
	tmp = xb_node_query_text_as_uint(n, "module_id", NULL);
	if (tmp != G_MAXUINT64 && tmp <= G_MAXUINT16)
		self->module_id = tmp;

	/* success */
	return TRUE;
}

static GBytes *
fu_focalfp_firmware_write(FuFirmware *firmware, GError **error)
{
	FuFocalfpFirmware *self = FU_FOCALFP_FIRMWARE(firmware);
	g_autoptr(GByteArray) buf = g_byte_array_new();
	g_autoptr(GBytes) blob = NULL;
	/* only one image supported */
	blob = fu_firmware_get_bytes_with_patches(firmware, error);
	if (blob == NULL){
		return NULL;
	}

	fu_byte_array_append_bytes(buf, blob);
	//g_byte_array_append(buf, focalfp_signature, sizeof(focalfp_signature));
	return g_byte_array_free_to_bytes(g_steal_pointer(&buf));
}

static void
fu_focalfp_firmware_init(FuFocalfpFirmware *self)
{
     
}

static void
fu_focalfp_firmware_class_init(FuFocalfpFirmwareClass *klass)
{
	FuFirmwareClass *klass_firmware = FU_FIRMWARE_CLASS(klass);
	//klass_firmware->check_magic = fu_focalfp_firmware_check_magic;
	klass_firmware->parse = fu_focalfp_firmware_parse;
	klass_firmware->build = fu_focalfp_firmware_build;
	klass_firmware->write = fu_focalfp_firmware_write;
	klass_firmware->export = fu_focalfp_firmware_export;
}

FuFirmware *
fu_focalfp_firmware_new(void)
{
	return FU_FIRMWARE(g_object_new(FU_TYPE_FOCALFP_FIRMWARE, NULL));
}
