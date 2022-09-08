/*
 * Copyright (C) 2022 Shihwei Huang <shihwei.huang@focaltech-electronics.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib.h>

#define FOCALFP_DELAY_COMPLETE	     1200 /* ms */
#define FOCALFP_DELAY_RESET	     30	  /* ms */
#define FOCALFP_DELAY_UNLOCK	     100  /* ms */
#define FOCALFP_DELAY_WRITE_BLOCK     35	  /* ms */
#define FOCALFP_DELAY_WRITE_BLOCK_512 50	  /* ms */

/*********************************************************
Define Program Code
**********************************************************/
#define PROGRAM_CODE_OK					0x00
#define	PROGRAM_CODE_INVALID_PARAM			0x01
#define	PROGRAM_CODE_ALLOCATE_BUFFER_ERROR		0x02
#define	PROGRAM_CODE_ERASE_FLASH_ERROR			0x03
#define	PROGRAM_CODE_WRITE_FLASH_ERROR			0x04
#define	PROGRAM_CODE_READ_FLASH_ERROR			0x05
#define	PROGRAM_CODE_CHECK_DATA_ERROR			0x06//Downloadʹ��
#define	PROGRAM_CODE_CHECKSUM_ERROR			0x07//Upgradeʹ��
#define	PROGRAM_CODE_CHIP_ID_ERROR			0x08
#define	PROGRAM_CODE_ENTER_DEBUG_MODE_ERROR		0x09
#define	PROGRAM_CODE_WRITE_ENABLE_ERROR			0x0a
#define	PROGRAM_CODE_RESET_SYSTEM_ERROR			0x0b
#define	PROGRAM_CODE_IIC_BYTE_DELAY_ERROR		0x0c
#define	PROGRAM_CODE_CHECK_BLANK_ERROR			0x0d
#define	PROGRAM_CODE_ENTER_UPGRADE_MODE_ERROR	        0x0e
#define	PROGRAM_CODE_COMM_ERROR				0x0f

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
/*********************************************************
Define HID CMD
**********************************************************/
#define	CMD_ENTER_UPGRADE_MODE			0x40
#define	CMD_CHECK_CURRENT_STATE			0x41
#define CMD_READY_FOR_UPGRADE			0x42
#define CMD_SEND_DATA				0x43
#define CMD_UPGRADE_CHECKSUM			0x44
#define CMD_EXIT_UPRADE_MODE			0x45
#define CMD_USB_READ_UPGRADE_ID			0x46
#define CMD_USB_ERASE_FLASH 			0x47
#define CMD_USB_BOOT_READ 		    	0x48
#define CMD_USB_BOOT_BOOTLOADERVERSION          0x49
#define CMD_READ_REGISTER			0x50
#define CMD_WRITE_REGISTER			0x51
#define CMD_ACK					0xf0
#define CMD_NACK				0xff
#define FIRST_PACKET				0x00
#define SERIAL_PACKET				0x01
#define	MID_PACKET				0x01
#define END_PACKET				0x02

#define REPORT_SIZE				64
#define MAX_USB_PACKET_SIZE			56
#define FTS_READ_PACKET_OFFSET 		        6
#define FTS_HID_PACKET_MAX	                56




