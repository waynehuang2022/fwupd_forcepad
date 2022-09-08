/*
 * Copyright (C) 2022 Shihwei Huang <shihwei.huang@focaltech-electronics.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "config.h"

#include <fwupdplugin.h>

#include <linux/hidraw.h>
#include <linux/input.h>

#include "fu-focalfp-common.h"
#include "fu-focalfp-firmware.h"
#include "fu-focalfp-hid-device.h"

struct _FuFocalfpHidDevice {
	FuUdevDevice parent_instance;
	guint16 iap_ctrl;
	guint16 module_id;
};
enum USB_UPGRADE_STEP{
    USB_UPGRADE_ERASE_FLASH=0,
	USB_UPGRADE_CHECK_ERASE_READY,
	USB_UPGRADE_SEND_DATA,
	USB_UPGRADE_CHECK_SUM,
	USB_UPGRADE_EXIT,	
	USB_UPGRADE_END
};
G_DEFINE_TYPE(FuFocalfpHidDevice, fu_focalfp_hid_device, FU_TYPE_UDEV_DEVICE)

#define FU_FOCALFP_DEVICE_IOCTL_TIMEOUT 5000 /* ms */

static gboolean
fu_focalfp_hid_device_detach(FuDevice *device, FuProgress *progress, GError **error);

static void
fu_focalfp_hid_device_to_string(FuDevice *device, guint idt, GString *str)
{
	FuFocalfpHidDevice *self = FU_FOCALFP_HID_DEVICE(device);
	fu_common_string_append_kx(str, idt, "ModuleId", self->module_id);
}

static gboolean
fu_focalfp_hid_device_probe(FuDevice *device, GError **error)
{
	if (!FU_DEVICE_CLASS(fu_focalfp_hid_device_parent_class)->probe(device, error)){
		return FALSE;
	}
	/* check is valid */
	if (g_strcmp0(fu_udev_device_get_subsystem(FU_UDEV_DEVICE(device)), "hidraw") != 0) {
		g_set_error(error,
			    FWUPD_ERROR,
			    FWUPD_ERROR_NOT_SUPPORTED,
			    "is not correct subsystem=%s, expected hidraw",
			    fu_udev_device_get_subsystem(FU_UDEV_DEVICE(device)));
		return FALSE;
	}

	// i2c-hid 
	if (fu_udev_device_get_model(FU_UDEV_DEVICE(device)) !=0x0106) {
		g_set_error_literal(error,
				    FWUPD_ERROR,
				    FWUPD_ERROR_NOT_SUPPORTED,
				    "not i2c-hid touchpad");
		return FALSE;
	}

	/* set the physical ID */
	return fu_udev_device_set_physical_id(FU_UDEV_DEVICE(device), "hid", error);
}



static guint8 
fu_focaltp_hid_device_checksum(guint8 *buf, guint8 len)
{
	guint8 i;
	guint8 checksum = 0;
	for(i = 0; i < len; i++)
		checksum ^= buf[i];
	checksum++;
	return checksum;	
}

static gboolean
fu_focalfp_hid_device_io(FuFocalfpHidDevice *self,
			      guint8 *wbuf,
			      gsize write_len,
			      guint8 *rbuf,
			      gsize read_len,
			      GError **error)
{

    guint8 cmdlen = 0;	
    guint8 ReCode = COMM_HID_OK;	
    g_autofree guint8 *buf = NULL;
    gsize bufsz = 64;
    gboolean ret=TRUE;
    guint32 i =0;
    buf = g_malloc0(bufsz);
	//buf[0] = tx[0]; /* report number */

    

    if(write_len>0)
    {
        if (g_getenv("FWUPD_FOCALFP_VERBOSE") != NULL)
		fu_common_dump_raw(G_LOG_DOMAIN, "SetReport", wbuf, write_len);
	
	cmdlen = 4 + write_len;		
	buf[0] = 0x06; 
	buf[1] = 0xff;
	buf[2] = 0xff;
	buf[3] = cmdlen;
	ret=fu_memcpy_safe(buf, bufsz, 0x04, wbuf, write_len, 0x00, write_len, error);		
	buf[cmdlen] = fu_focaltp_hid_device_checksum(&buf[1], cmdlen - 1);

        if (!fu_udev_device_ioctl(FU_UDEV_DEVICE(self),
				  HIDIOCSFEATURE(cmdlen+1),
				  buf,
				  NULL,
				  error)){
		return FALSE;		
	}
		
    }	
	
    if(read_len>0)
    {		 
		//memset(buf, 0, sizeof(buf));
		//ReCode = ftp_hid_read(buf, read_len);
		//memcpy(rbuf,buf,read_len);		
		buf[0] = 0x06;
		/* GetFeature */	
    	if (!fu_udev_device_ioctl(FU_UDEV_DEVICE(self),
    				  HIDIOCGFEATURE(bufsz),
    				  buf,
    				  NULL,
    				  error)){
    		return FALSE;
    	}
    	if (g_getenv("FWUPD_FOCALFP_VERBOSE") != NULL)
    		fu_common_dump_raw(G_LOG_DOMAIN, "GetReport", buf, bufsz);

    	/* success */
    	return fu_memcpy_safe(rbuf, read_len, 0x0, buf, bufsz, 0x00, read_len, error);
		
    }
	
}

static gboolean
fu_focalfp_hid_device_read_reg(FuFocalfpHidDevice *self,			      
			      guint8 RegAddr,
			      guint8 *pRegData,GError **error)
{	
	gsize bufsz = 64;
	guint8 CmdPacket[bufsz];
	guint8 RePacket[bufsz];
	
	gboolean ret = FALSE;
    
    int i=0;
    //memset(CmdPacket, 0xff, REPORT_SIZE);
    //memset(RePacket, 0xff, REPORT_SIZE);    


    CmdPacket[0] = CMD_READ_REGISTER;
    CmdPacket[1] = RegAddr;
    
    ret = fu_focalfp_hid_device_io(self,CmdPacket, 2, RePacket, 0,error); //Write 
    
    g_usleep(200);
    for(i=0; i<4; i++)
    {
        ret = fu_focalfp_hid_device_io(self,CmdPacket, 0, RePacket, 8,error); //Read
        if(ret == TRUE)
        {
            g_print("=forcepad:read_reg result=[0]=%2x,[1]=%2x,[2]=%2x,[3]=%2x,[4]=%2x,[5]=%2x,[6]=%2x,[7]=%2x,=======================\n",RePacket[0],RePacket[1],RePacket[2],RePacket[3],RePacket[4],RePacket[5],RePacket[6],RePacket[7]);
            if(RePacket[RePacket[3]] == fu_focaltp_hid_device_checksum(RePacket+1, RePacket[3]-1)) //check crc
            {
                if(RePacket[4] == CMD_READ_REGISTER)//����
                {
                    *pRegData = RePacket[6];                    
                    return TRUE;
                }
                else
                {
                    ret = FALSE;                     
                }
            }
            else
            {
                ret = FALSE;
            }
        }
        g_usleep(200);
    }
    return ret;
    
	
}

/************************************************
 COMM_FLASH_EnterUpgradeMode: Enter Upgrade mode
*************************************************/
static gboolean  COMM_FLASH_EnterUpgradeMode(FuFocalfpHidDevice *self, GError **error)
{
	gboolean ret = TRUE;
	guint8 CmdPacket[REPORT_SIZE];
	guint8 RePacket[REPORT_SIZE];
	//memset(CmdPacket, 0xff, REPORT_SIZE);
	//memset(RePacket, 0xff, REPORT_SIZE);	
	CmdPacket[0] = CMD_ENTER_UPGRADE_MODE;	
	
	ret = fu_focalfp_hid_device_io(self,CmdPacket, 1, RePacket, 6,error);
	if(ret)
	{		
		//intf("Repacket End[%02x checksum[%d] \r\n", RePacket[RePacket[3]], i);
		if(RePacket[RePacket[3]] == fu_focaltp_hid_device_checksum(RePacket+1, RePacket[3]-1))	//check crc
		{
			if(RePacket[4] == CMD_ACK)
			{
				return TRUE;
			}
			else
			{
		        g_prefix_error(error, "Enter Bootload UpgradeMode Error: COMM_HID_PACKET_COMMAND_ERROR");
				return FALSE;
			}
		}
		else
		{
			g_prefix_error(error, "Enter Bootload UpgradeMode Error: COMM_HID_PACKET_CHECKSUM_ERROR");
			return FALSE;
		}
	}else{
	        g_prefix_error(error, "Enter Bootload UpgradeMode Error: CMD_ENTER_UPGRADE_MODE return error");
	        return FALSE;
	}	
	return ret;
}

/**********************************************************
 COMM_FLASH_CheckCurrentState: Get BootLoader Current State
***********************************************************/
static gboolean COMM_FLASH_CheckCurrentState(FuFocalfpHidDevice *self, guint8 *pData, GError **error)
{
	gboolean ret = TRUE;
	guint8 CmdPacket[REPORT_SIZE];
	guint8 RePacket[REPORT_SIZE];
	//memset(CmdPacket, 0xff, REPORT_SIZE);
	//memset(RePacket, 0xff, REPORT_SIZE);
	
	CmdPacket[0] = CMD_CHECK_CURRENT_STATE;

	ret = fu_focalfp_hid_device_io(self,CmdPacket,1, RePacket, 7,error);

	if(ret)
	{			
		if(RePacket[RePacket[3]] == fu_focaltp_hid_device_checksum(RePacket+1, RePacket[3]-1))	//check crc
		{
			if(RePacket[4] == CMD_CHECK_CURRENT_STATE)
			{
				*pData = RePacket[5];
			}
			else
			{
			        g_prefix_error(error, "Enter CheckCurrentState Error: COMM_HID_PACKET_COMMAND_ERROR");
	                        return FALSE;
			}
		}
		else
		{
			g_prefix_error(error, "Enter CheckCurrentState Error: COMM_HID_PACKET_CHECKSUM_ERROR");
	                return FALSE;
		}		
	}else{
	        g_prefix_error(error, "Enter CheckCurrentState Error: CMD_CHECK_CURRENT_STATE return error");
	        return FALSE;
	}
	return ret;

}


/**********************************************************
COMM_FLASH_ExitUpgradeMode: exit upgrade mode
***********************************************************/
static gboolean COMM_FLASH_ExitUpgradeMode(FuFocalfpHidDevice *self, GError **error)
{
	gboolean ret = TRUE;
	guint8 CmdPacket[REPORT_SIZE];
	guint8 RePacket[REPORT_SIZE];
	//memset(CmdPacket, 0xff, REPORT_SIZE);
	//memset(RePacket, 0xff, REPORT_SIZE);


	CmdPacket[0] = CMD_EXIT_UPRADE_MODE;

	ret = fu_focalfp_hid_device_io( self,CmdPacket,1, RePacket, 6,error);

	if(ret)
	{
		if(RePacket[RePacket[3]] == fu_focaltp_hid_device_checksum(RePacket+1, RePacket[3]-1))	//check crc
		{
			if(RePacket[4] == CMD_ACK)
				ret = TRUE;			
			else			
				ret = FALSE;				
		}
		else		
			ret=FALSE;		
	}
	return ret;
}
/**********************************************************
COMM_FLASH_CheckTPIsReadyForUpgrade: Check ready
***********************************************************/
static gboolean COMM_FLASH_CheckTPIsReadyForUpgrade(FuFocalfpHidDevice *self, GError **error)
{
	gboolean ret = TRUE;
	guint8 CmdPacket[REPORT_SIZE];
	guint8 RePacket[REPORT_SIZE];
	//memset(CmdPacket, 0xff, REPORT_SIZE);
	//memset(RePacket, 0xff, REPORT_SIZE);
	
	CmdPacket[0] = CMD_READY_FOR_UPGRADE;
	ret = fu_focalfp_hid_device_io(self,CmdPacket, 1, RePacket, 7,error);
	
	if(ret)
	{	
		if(RePacket[RePacket[3]] == fu_focaltp_hid_device_checksum(RePacket+1, RePacket[3]-1))	
		{
			if(RePacket[4] == CMD_READY_FOR_UPGRADE)
			{
				if(2 == RePacket[5])				
					ret = TRUE;				
				else{
					ret = FALSE;
					g_prefix_error(error, "CheckTPIsReadyForUpgrade Error: COMM_HID_PACKET_COMMAND_ERROR");
				}
			}	
			else{
				ret = FALSE;
				g_prefix_error(error, "CheckTPIsReadyForUpgrade Error: COMM_HID_PACKET_DATA_ERROR");
			}
		}
		else{	
		        ret = FALSE;
			g_prefix_error(error, "CheckTPIsReadyForUpgrade Error: COMM_HID_PACKET_CHECKSUM_ERROR");
		}
	}
	return ret;

}

/**********************************************************
COMM_FLASH_USB_ReadUpdateID: Get Bootload ID
***********************************************************/
static gboolean COMM_FLASH_USB_ReadUpdateID(FuFocalfpHidDevice *self,guint16 *usIcID, GError **error)
{
	gboolean ret = TRUE;
	guint8 CmdPacket[REPORT_SIZE];
	guint8 RePacket[REPORT_SIZE];
	int i=0;
	//memset(CmdPacket, 0xff, REPORT_SIZE);
	//memset(RePacket, 0xff, REPORT_SIZE);

	CmdPacket[0] = CMD_USB_READ_UPGRADE_ID;    

	for(i = 0; i < 10; i++)
	{
		g_usleep(100);
		ret = fu_focalfp_hid_device_io(self,CmdPacket,1, RePacket, 8,error);
		if(ret)
		{	
			if(RePacket[RePacket[3]] == fu_focaltp_hid_device_checksum(RePacket+1, RePacket[3]-1))	//check crc
			{
				if(RePacket[4] == CMD_USB_READ_UPGRADE_ID)
				{
					*usIcID= ((RePacket[5])<<8) + RePacket[6];					
					ret = TRUE;
					break;					
				}
				else
				{
					ret = FALSE;
					g_prefix_error(error, "Enter COMM_FLASH_USB_ReadUpdateID Error: COMM_HID_PACKET_COMMAND_ERROR");
				}
			}
			else
			{
				ret = FALSE;
				g_prefix_error(error, "Enter COMM_FLASH_USB_ReadUpdateID Error: COMM_HID_PACKET_CHECKSUM_ERROR");
			}
		}

	}
	return ret;
}

/**********************************************************
COMM_FLASH_USB_EraseFlash: Erase Flash
***********************************************************/
static gboolean COMM_FLASH_USB_EraseFlash(FuFocalfpHidDevice *self, GError **error)
{
	gboolean ret = TRUE;
	guint8 CmdPacket[REPORT_SIZE];
	guint8 RePacket[REPORT_SIZE];


	CmdPacket[0] = CMD_USB_ERASE_FLASH;    

	ret = fu_focalfp_hid_device_io(self,CmdPacket,1, RePacket, 6,error);
	if(ret)
	{	
		if(RePacket[RePacket[3]] == fu_focaltp_hid_device_checksum(RePacket+1, RePacket[3]-1))	//check crc
		{
			if(RePacket[4] == CMD_ACK)
			{				
				ret = TRUE;				
			}
			else
			{
				ret = FALSE;
				g_prefix_error(error, "Enter COMM_FLASH_USB_EraseFlash Error: COMM_HID_PACKET_COMMAND_ERROR");
			}
		}
		else
		{
			ret = FALSE;
			g_prefix_error(error, "Enter COMM_FLASH_USB_EraseFlash Error: COMM_HID_PACKET_CHECKSUM_ERROR");
		}
	}

	return ret;
}

/**********************************************************
COMM_FLASH_SendDataByUSB: Send Write data
***********************************************************/
static gboolean COMM_FLASH_SendDataByUSB(FuFocalfpHidDevice *self,guint8 PacketType, guint8 * SendData, guint8 DataLength, GError **error)
{

	if(DataLength > REPORT_SIZE - 8) 
	    return FALSE;
	gboolean ret = TRUE; 
	guint8 CmdPacket[REPORT_SIZE];
	guint8 RePacket[REPORT_SIZE];
	int retry=0;
	
	CmdPacket[0] = CMD_SEND_DATA;
	CmdPacket[1] = PacketType;
	//memcpy(CmdPacket + 2, SendData, DataLength);	
	fu_memcpy_safe(CmdPacket, REPORT_SIZE, 0x02, SendData, DataLength, 0x00, DataLength, error);
	ret = fu_focalfp_hid_device_io(self,CmdPacket, DataLength + 2, RePacket, 0,error);
	
	if(ret)
	{		
		for(retry=0; retry<4; retry++)
		{
			ret = fu_focalfp_hid_device_io(self,CmdPacket, 0, RePacket, 7,error);
			if(RePacket[RePacket[3]] == fu_focaltp_hid_device_checksum(RePacket+1, RePacket[3]-1))	//check crc
			{
				if(RePacket[4] == CMD_ACK)
				{
					return TRUE;						
				}
				else
				{
					ret = FALSE;
				        g_prefix_error(error, "Enter COMM_FLASH_SendDataByUSB Error: COMM_HID_PACKET_COMMAND_ERROR");
				}
			}
			else
			{
				ret = FALSE;
				g_prefix_error(error, "Enter COMM_FLASH_SendDataByUSB Error: COMM_HID_PACKET_CHECKSUM_ERROR");
			}
			g_usleep(1);
		}
	}

	return ret;

}

/**********************************************************
COMM_FLASH_Checksum_Upgrade: Get check sum for write done
***********************************************************/
static gboolean COMM_FLASH_Checksum_Upgrade(FuFocalfpHidDevice *self,guint32 *pData, GError **error)
{
	gboolean ret = TRUE;
	guint8 CmdPacket[REPORT_SIZE];
	guint8 RePacket[REPORT_SIZE];
	
	CmdPacket[0] = CMD_UPGRADE_CHECKSUM;	

	ret = fu_focalfp_hid_device_io(self,CmdPacket,1, RePacket, 7 + 3,error);
	
	if(ret)
	{	
		if(RePacket[RePacket[3]] == fu_focaltp_hid_device_checksum(RePacket+1, RePacket[3]-1))	//check crc
		{
			if(RePacket[4] == CMD_UPGRADE_CHECKSUM)				
				*pData = (RePacket[8] << 24) +  (RePacket[7] << 16) + (RePacket[6] << 8) + RePacket[5];				
			else				
				ret = FALSE;
				g_prefix_error(error, "Enter COMM_FLASH_Checksum_Upgrade Error: COMM_HID_PACKET_COMMAND_ERROR");
		}
		else
		{
			ret = FALSE;
			g_prefix_error(error, "Enter COMM_FLASH_Checksum_Upgrade Error: COMM_HID_PACKET_CHECKSUM_ERROR");
		}
	}
	
	return ret;

}

static gboolean
fu_focalfp_hid_device_ensure_iap_ctrl(FuFocalfpHidDevice *self, GError **error)
{
	return TRUE;
}

static gboolean
fu_focalfp_hid_device_setup(FuDevice *device, GError **error)
{
	FuFocalfpHidDevice *self = FU_FOCALFP_HID_DEVICE(device);
	FuUdevDevice *udev_device = FU_UDEV_DEVICE(device);
	guint16 fwver;
	guint16 tmp;
	guint8 buf[2] = {0x0};
	guint8 ic_type;
	g_autofree gchar *version_bl = NULL;
	g_autofree gchar *version = NULL;
        g_print("=forcepad:hid device setup ==================\n");
      
	/* get current firmware version */
	if (!fu_focalfp_hid_device_read_reg(self, 0xa6, buf,error)) {
		g_prefix_error(error, "failed to read fw version: ");
		g_print("=forcepad:hid device setup return false : failed to read fw version=====================\n");
		return FALSE;
	}
	
	if (!fu_focalfp_hid_device_read_reg(self, 0xad, buf+1,error)) {
		g_prefix_error(error, "failed to read fw version2: ");
		g_print("=forcepad:hid device setup return false : failed to read fw version 2=====================\n");
		return FALSE;
	}
	fwver = (guint16)(buf[0]<<8 | buf[1]);
	//g_print("!!!!!!!!!!!!!!!!!!!!!!!!!!!!get fw version=%02x!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n",fwver);
	//fwver = fu_memread_uint16(buf, G_BIG_ENDIAN);
	version = fu_common_version_from_uint16(fwver, FWUPD_VERSION_FORMAT_HEX);
	fu_device_set_version(device, version);


	/* define the extra instance IDs */
	g_print("=forcepad:hid device setup: VEN=%2x ==================\n",fu_udev_device_get_vendor(udev_device));
        g_print("=forcepad:hid device setup: DEV=%2x ==================\n",fu_udev_device_get_model(udev_device));
        //g_print("=forcepad:hid device setup: MOD=%2x ==================\n",self->module_id);
	fu_device_add_instance_u16(device, "VEN", fu_udev_device_get_vendor(udev_device));
	fu_device_add_instance_u16(device, "DEV", fu_udev_device_get_model(udev_device));
	//fu_device_add_instance_u16(device, "MOD", self->module_id);
	if (!fu_device_build_instance_id(device, error, "HIDRAW", "VEN", "DEV"/*, "MOD"*/, NULL)){
			return FALSE;
        }
	fu_device_set_firmware_size(device, ((guint64)122880));
    g_print("forcepad setup done\n");
	/* success */
	return TRUE;
}

static FuFirmware *
fu_focalfp_hid_device_prepare_firmware(FuDevice *device,
				      GBytes *fw,
				      FwupdInstallFlags flags,
				      GError **error)
{
	FuFocalfpHidDevice *self = FU_FOCALFP_HID_DEVICE(device);
	guint16 module_id;
	g_autoptr(FuFirmware) firmware = fu_focalfp_firmware_new();

	/* check is compatible with hardware */
	if (!fu_firmware_parse(firmware, fw, flags, error))
		return NULL;
        /*
	module_id = fu_focalfp_firmware_get_module_id(FU_FOCALFP_FIRMWARE(firmware));
	if (self->module_id != module_id) {
		g_set_error(error,
			    FWUPD_ERROR,
			    FWUPD_ERROR_INVALID_FILE,
			    "firmware incompatible, got 0x%04x, expected 0x%04x",
			    module_id,
			    self->module_id);
		g_print("=forcepad:hid device prepare_firmware return NULL===============================\n");
		return NULL;
	}
        */
	/* success */
	return g_steal_pointer(&firmware);
}

static gboolean
fu_focalfp_hid_device_write_firmware(FuDevice *device,
				    FuFirmware *firmware,
				    FuProgress *progress,
				    FwupdInstallFlags flags,
				    GError **error)
{
	FuFocalfpHidDevice *self = FU_FOCALFP_HID_DEVICE(device);
	FuFocalfpFirmware *firmware_focalfp = FU_FOCALFP_FIRMWARE(firmware);
	/////////////////////////////////////
	gsize bufsz = 0;
	guint32 Checksum = 0;
	guint32 DValue = 0;
	guint32 FWChecksum;
	guint8 retry;
	guint8 Step;
	gboolean bUpgrading=FALSE;
	guint8 ProgramCode = PROGRAM_CODE_OK;
	guint16 usIcID;
	guint8 ucPacketType = 0;		
	guint32 Max_Length;
	guint8 data[64];
	gboolean ret=TRUE;
	guint32 DataLen = 0;
	guint32 SentDataLen = 0;
	/////////////////////////////////////
	guint32 checksum_device = 0;
	guint32 i = 0;
	const guint8 *g_DataBuffer;
	guint8 csum_buf[2] = {0x0};
	g_autoptr(GBytes) fw = NULL;
	g_autoptr(GPtrArray) chunks = NULL;
	const guint32 UPGRADE_ID=0x582E;
	/* progress */
	fu_progress_set_id(progress, G_STRLOC);
	fu_progress_add_flag(progress, FU_PROGRESS_FLAG_GUESSED);
	fu_progress_add_step(progress, FWUPD_STATUS_DEVICE_BUSY, 1);
	fu_progress_add_step(progress, FWUPD_STATUS_DEVICE_WRITE, 97);
	fu_progress_add_step(progress, FWUPD_STATUS_DEVICE_VERIFY, 1);
	fu_progress_add_step(progress, FWUPD_STATUS_DEVICE_RESTART, 1);
      
        
	/* simple image */
	fw = fu_firmware_get_bytes(firmware, error);
	if (fw == NULL)
		return FALSE;

	/* detach */
	if (!fu_focalfp_hid_device_detach(device, fu_progress_get_child(progress), error))
		return FALSE;
	fu_progress_step_done(progress);
	/* write each block */
	g_DataBuffer = g_bytes_get_data(fw, &bufsz);
	Max_Length=bufsz;
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//Calculate FW checksum...
	for(i=0; i<bufsz ; i+=4)
	{
		DValue = (g_DataBuffer[i + 3] << 24) + (g_DataBuffer[i + 2] << 16) +(g_DataBuffer[i + 1] << 8) + g_DataBuffer[i];
		Checksum ^= DValue;
	}
	
	Checksum += 1;
	Step=0;
	retry=0;
	bUpgrading=TRUE;
	while(bUpgrading)
	{
		switch(Step)
		{
			case(USB_UPGRADE_ERASE_FLASH):				
				//Read chip id & Erase Flash
				ret = COMM_FLASH_CheckTPIsReadyForUpgrade(self,error);
				if(ret)
				{
					ret = COMM_FLASH_USB_ReadUpdateID(self,&usIcID,error);
					if(!ret)
					{
						g_print("Read FT3637 id error. \r\n");
						ProgramCode = PROGRAM_CODE_CHIP_ID_ERROR;
						Step=USB_UPGRADE_END;
						continue;
					}
					else
					{
						if(UPGRADE_ID!=usIcID)
						{
							g_print("FT3637 id error. \r\n");
							ProgramCode = PROGRAM_CODE_CHIP_ID_ERROR;
							Step=USB_UPGRADE_END;
							continue;
						}			
					}		
					COMM_FLASH_USB_EraseFlash(self,error);			
					g_usleep(1000*1000);
					g_print("Erase Time: 1S \r\n");
					Step=USB_UPGRADE_CHECK_ERASE_READY;
					retry=0;
				}
				else
				{	
					retry++;
					if(retry==3)
					{
						g_print("TP is not ready for upgrade \r\n");
						ProgramCode = PROGRAM_CODE_ENTER_UPGRADE_MODE_ERROR;
						Step=USB_UPGRADE_END;
					}
				}
				break;
			case(USB_UPGRADE_CHECK_ERASE_READY):				
				//Check Erase is Ready?
				ret = COMM_FLASH_CheckTPIsReadyForUpgrade(self,error);
				if(ret)
				{
					Step=USB_UPGRADE_SEND_DATA;
					retry=0;	
					DataLen = 0;
					SentDataLen=0;
				}
				else
				{					
					g_usleep(500*1000);
					retry++;
					g_print("Erase Time: %d.%dS \r\n", (1+retry/2),(5*(retry%2)));
					if(retry==20)
					{
						g_print("Erase Flash Error. \r\n"); 
						ProgramCode = PROGRAM_CODE_ERASE_FLASH_ERROR;
						Step=USB_UPGRADE_END;
					}
				}
				break;
			case(USB_UPGRADE_SEND_DATA):				
				//Send Packet Data 
				if(SentDataLen < Max_Length)
				{
					if(retry==0)
					{
						DataLen=0;
						//memset(data, 0xff, sizeof(data));			
						if(SentDataLen == 0)			
							ucPacketType = FIRST_PACKET;			
						else if(SentDataLen >= Max_Length - MAX_USB_PACKET_SIZE)			
							ucPacketType = END_PACKET;			
						else			
							ucPacketType = MID_PACKET;
						
						if(SentDataLen + MAX_USB_PACKET_SIZE > Max_Length)
						{
							//memcpy(data, g_DataBuffer + SentDataLen, Max_Length - SentDataLen);
							fu_memcpy_safe(data, REPORT_SIZE, 0x00, g_DataBuffer, bufsz, SentDataLen, Max_Length - SentDataLen, error);
							DataLen = Max_Length - SentDataLen;
						}
						else
						{				
							//memcpy(data, g_DataBuffer + SentDataLen, MAX_USB_PACKET_SIZE);
							fu_memcpy_safe(data, REPORT_SIZE, 0x00, g_DataBuffer, bufsz, SentDataLen, MAX_USB_PACKET_SIZE, error);
							DataLen = MAX_USB_PACKET_SIZE;
						}
						ret = COMM_FLASH_SendDataByUSB(self,ucPacketType, data, DataLen,error);						
						retry++;
					}

					if(retry>0)
					{	
						
						ret = COMM_FLASH_CheckTPIsReadyForUpgrade(self,error);
						if(ret)
						{
							SentDataLen += DataLen;
							//g_print("Updating %d bytes... \r\n", SentDataLen);
							fu_progress_set_percentage_full(fu_progress_get_child(progress),(gsize)SentDataLen,(gsize)Max_Length);
							retry=0;
						}
						else
						{
							g_usleep(1000);
							g_print("Updating %d \r\n", retry);
							if(++retry>100)
							{
								g_print("Upgrade failed! \r\n");
								ProgramCode = PROGRAM_CODE_WRITE_FLASH_ERROR;
								Step=USB_UPGRADE_END;
							}

						}						
					}
				}				
				else
				{
					//Write flash End and check ready (fw calculate checksum)
					g_usleep(50*1000); 
					ret = COMM_FLASH_CheckTPIsReadyForUpgrade(self,error);
					if(ret)	
					{			
						Step=USB_UPGRADE_CHECK_SUM;
						retry=0;
					}
					else
					{
						if(++retry>5)
						{
							printf("Upgrade failed!");
							ProgramCode = PROGRAM_CODE_WRITE_FLASH_ERROR;
							//COMM_FLASH_ExitUpgradeMode();		//Reset
							Step=USB_UPGRADE_END;
						}
						
					}
				}
				break;
			case(USB_UPGRADE_CHECK_SUM):
			        fu_progress_step_done(progress);
				ret = COMM_FLASH_Checksum_Upgrade(self,&FWChecksum,error);
				if(ret)
				{				
					if(Checksum == FWChecksum)
					{  						
						printf("Checksum Right, PC:0x%x, FW:0x%x!", Checksum, FWChecksum);
						Step=USB_UPGRADE_EXIT;
						//TODO modify USB UPGRADE EXIT to attach
						retry=0;
						COMM_FLASH_ExitUpgradeMode(self,error);		//Reset
					}
					else
					{	
						printf("Checksum error, PC:0x%x, FW:0x%x!", Checksum, FWChecksum);						
						g_usleep(500*1000);
						ProgramCode = PROGRAM_CODE_CHECKSUM_ERROR;
						Step=USB_UPGRADE_END;
					}
				}
				else
				{				
					ProgramCode = PROGRAM_CODE_CHECKSUM_ERROR;
					Step=USB_UPGRADE_END;
				}					
				break;
			case(USB_UPGRADE_EXIT):
			        fu_progress_step_done(progress);
				g_usleep(500*1000);				
				ret = fu_focalfp_hid_device_read_reg(self,0x9F,&data[0],error);
				ret = fu_focalfp_hid_device_read_reg(self,0xA3,&data[1],error);
				printf("Exit Upgrade Mode, Times: %d, id1=%x, id2=%x \r\n", retry,data[1],data[0]);				
				if((data[1]==0x58)&&(data[0]==0x22))
				{					
					Step=USB_UPGRADE_END;
					retry=0;			
					printf("Upgrade is successful! \r\n");
				}
				else
				{
					retry++;
					if(retry==4)
					{
						ProgramCode = PROGRAM_CODE_RESET_SYSTEM_ERROR;
						Step=USB_UPGRADE_END;
					}
				}				
				break;
			case(USB_UPGRADE_END):
			//TODO  add ProgramCode is ok ?
				bUpgrading=FALSE;
				break;
		}		
			
	}
	if(ProgramCode!=0x00){
                g_set_error(error,
				        FWUPD_ERROR,
				        FWUPD_ERROR_WRITE,
				        "Upgrade error code: %d",
				        ProgramCode);
                                        return FALSE;
                                }
	fu_progress_sleep(fu_progress_get_child(progress), FOCALFP_DELAY_COMPLETE);
        fu_progress_step_done(progress);
	
	return TRUE;  
}

static gboolean
fu_focalfp_hid_device_detach(FuDevice *device, FuProgress *progress, GError **error)
{
	FuFocalfpHidDevice *self = FU_FOCALFP_HID_DEVICE(device);
	guint8 retry;
	gboolean ret =TRUE;
	gboolean bSuccessed=FALSE;
	guint8 ucMode = 0;
        ret =COMM_FLASH_EnterUpgradeMode(self,error);
        g_usleep(200*1000);
        retry=0;
        bSuccessed=FALSE;
        while(retry<3&&!bSuccessed){
            ret = COMM_FLASH_EnterUpgradeMode(self,error);
	    if(ret)
	    {				
		    ret = COMM_FLASH_CheckCurrentState(self,&ucMode,error);
		    if(ucMode == 1) //1: Upgrade Mode; 2: FW Mode
		    {
			   bSuccessed=TRUE;
		    }
		    else
		    {
			    g_usleep(5000);
			    g_print("Get Current State, Times: %d...\r\n", retry++);
			    retry++;
			    if(retry==3)
			    {
				    g_print("Failed to Get Current State! \r\n");
				    //ProgramCode = PROGRAM_CODE_ENTER_UPGRADE_MODE_ERROR;
				    //Step=USB_UPGRADE_END;
				    bSuccessed=FALSE;
			    }			
		    }
	    }
	    else
	    {	
		    g_print("Enter Upgrade Mode..%dS.\r\n", retry+1);
		    retry++;
		    if(retry==3)
		    {
			    g_print("Failed to enter Upgrade Mode! \r\n");
			    //ProgramCode = PROGRAM_CODE_ENTER_UPGRADE_MODE_ERROR;
			    //Step=USB_UPGRADE_END;
			     bSuccessed=FALSE;
		    }
		    g_usleep(200*1000);
	    }     
        }
        g_usleep(200*1000);
        if(bSuccessed){
              return bSuccessed;
        }else{
              return bSuccessed;
        }
}

static gboolean
fu_focalfp_hid_device_attach(FuDevice *device, FuProgress *progress, GError **error)
{
        gboolean ret= TRUE;
	FuFocalfpHidDevice *self = FU_FOCALFP_HID_DEVICE(device);
	/* sanity check */
	/*
	if (!fu_device_has_flag(device, FWUPD_DEVICE_FLAG_IS_BOOTLOADER)) {
		g_debug("already in runtime mode, skipping");
		g_print("=forcepad:hid device attach return true :already in runtime mode, skipping==============================\n");
		return TRUE;
	}
	*/
        ret=COMM_FLASH_ExitUpgradeMode(self,error);
        g_usleep(500*1000);
	return TRUE;
}

static gboolean
fu_focalfp_hid_device_set_quirk_kv(FuDevice *device,
				  const gchar *key,
				  const gchar *value,
				  GError **error)
{
	FuFocalfpHidDevice *self = FU_FOCALFP_HID_DEVICE(device);
	guint64 tmp = 0;
	//g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, "quirk key not supported");
	return TRUE;
}

static void
fu_focalfp_hid_device_set_progress(FuDevice *self, FuProgress *progress)
{
	fu_progress_set_id(progress, G_STRLOC);
	fu_progress_add_flag(progress, FU_PROGRESS_FLAG_GUESSED);
	fu_progress_add_step(progress, FWUPD_STATUS_DEVICE_RESTART, 1);
	fu_progress_add_step(progress, FWUPD_STATUS_DEVICE_WRITE, 97);
	fu_progress_add_step(progress, FWUPD_STATUS_DEVICE_RESTART, 1);
	fu_progress_add_step(progress, FWUPD_STATUS_DEVICE_BUSY, 1);
}

static void
fu_focalfp_hid_device_init(FuFocalfpHidDevice *self)
{
	fu_device_add_flag(FU_DEVICE(self), FWUPD_DEVICE_FLAG_INTERNAL);
	fu_device_add_flag(FU_DEVICE(self), FWUPD_DEVICE_FLAG_UPDATABLE);
	fu_device_set_summary(FU_DEVICE(self), "Forcepad");
	fu_device_add_icon(FU_DEVICE(self), "input-forcepad");
	fu_device_add_protocol(FU_DEVICE(self), "tw.com.focalfp");
	fu_device_add_flag(FU_DEVICE(self), FWUPD_DEVICE_FLAG_UNSIGNED_PAYLOAD);
	fu_device_set_version_format(FU_DEVICE(self), FWUPD_VERSION_FORMAT_HEX);
	fu_device_set_priority(FU_DEVICE(self), 1); /* better than i2c */
	fu_udev_device_set_flags(FU_UDEV_DEVICE(self),
				 FU_UDEV_DEVICE_FLAG_OPEN_READ | FU_UDEV_DEVICE_FLAG_OPEN_WRITE |
				     FU_UDEV_DEVICE_FLAG_OPEN_NONBLOCK);
}

static void
fu_focalfp_hid_device_finalize(GObject *object)
{
	G_OBJECT_CLASS(fu_focalfp_hid_device_parent_class)->finalize(object);
}

static void
fu_focalfp_hid_device_class_init(FuFocalfpHidDeviceClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	FuDeviceClass *klass_device = FU_DEVICE_CLASS(klass);
	object_class->finalize = fu_focalfp_hid_device_finalize;
	klass_device->to_string = fu_focalfp_hid_device_to_string;
	klass_device->attach = fu_focalfp_hid_device_attach;
	klass_device->set_quirk_kv = fu_focalfp_hid_device_set_quirk_kv;
	klass_device->setup = fu_focalfp_hid_device_setup;
	klass_device->reload = fu_focalfp_hid_device_setup;
	klass_device->write_firmware = fu_focalfp_hid_device_write_firmware;
	klass_device->prepare_firmware = fu_focalfp_hid_device_prepare_firmware;
	klass_device->probe = fu_focalfp_hid_device_probe;
	klass_device->set_progress = fu_focalfp_hid_device_set_progress;
}
