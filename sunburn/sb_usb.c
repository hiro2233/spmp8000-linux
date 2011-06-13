/*
 * Sunburn - Firmware flashing tool for Sunplus SPMP8000 SoC
 * 
 * Copyright (C) 2011  Zoltan Devai <zdevai@gmail.com>
 * Credits to Alemaxx and openschemes.com
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <usb.h>

#include "sb.h"

#define	USB_TIMEOUT		10*1000 /* msecs */

#define	SPMP8000_VENDORID	0x04FC
#define	SPMP8000_PRODUCTID	0x7201

#define SPMP8000_USB_CONFIG	1
#define SPMP8000_USB_IF		0

#define USB_CBW_SIG		0x43425355UL	/* Little endian "USBC" */
#define USB_CSW_SIG		0x53425355UL	/* Little endian "USBS" */
//#define USB_CBW_TAG		0xDEADBEEFUL
#define USB_CBW_TAG		0x12345678UL
#define USB_CBW_TAG_FRM		0xC0522682UL	/* CBW tag used by FRM PRO */

typedef struct {
	uint32_t   sig;
	uint32_t   tag;
	uint32_t   xlen;
	uint8_t    flag;
	uint8_t    lun;
	uint8_t    blen;
	uint32_t   cmd;
	uint32_t   adr;
	uint32_t   unk[2];
}__attribute__((packed)) cbw_t;

typedef struct {
	uint32_t   sig;
	uint32_t   tag;
	uint32_t   residue;
	uint8_t    status;
}__attribute__((packed)) csw_t;

/**
 * Find and open a usb device with libusb based on its VID and PID
 * @param vid Vendor ID of device
 * @param pid Product ID of device
 * @returns NULL if not found, usb_dev_handle if OK
 */
static usb_dev_handle *usb_open_device(uint16_t vid, uint16_t pid)
{
	struct usb_bus *pbus;
	struct usb_device *pdev;

	usb_find_busses();
	usb_find_devices();

	for(pbus = usb_get_busses(); pbus; pbus = pbus->next) {
		for(pdev = pbus->devices; pdev; pdev = pdev->next) {
			if ((pdev->descriptor.idVendor == vid)
				&& (pdev->descriptor.idProduct == pid)) {
				return usb_open(pdev);
			}
		}
	}

	return NULL;
}

/**
 * Finds and configures an attached SPMP8000 device in ISP mode
 * @param usbdev Pointer to usb_dev_handle pointer for future reference
 * @returns 0 if OK, <0 on error
 */
int usb_spmp8000_init(usb_dev_handle **usbdev)
{
	int ret;

	/* libusb init */
	usb_init();

	/* Configure device */
	DBG1("Looking for SPMP8000 device with VID: 0x%04X, PID: 0x%04X\n",
		SPMP8000_VENDORID, SPMP8000_PRODUCTID);
	*usbdev = usb_open_device(SPMP8000_VENDORID, SPMP8000_PRODUCTID);
	if (*usbdev == NULL) {
		DBGE("Could not find SPMP8000 device\n");
		return -1;
	}

	usb_reset(*usbdev);

	/* Detach the mass storage driver */
	ret = usb_detach_kernel_driver_np(*usbdev, 0);
	if (ret)
		DBG1("Detached kernel driver from device\n");

	ret = usb_set_configuration(*usbdev, SPMP8000_USB_CONFIG);
	if (ret < 0) {
		DBGE("Could not select configuration #%d. "
			"Try with root user\n", SPMP8000_USB_CONFIG);
		return -1;
	}

	ret = usb_claim_interface(*usbdev, SPMP8000_USB_IF);
	if (ret < 0) {
		DBGE("Could not claim interface #%d. "
			"Try with root user\n", SPMP8000_USB_IF);
		return -1;
	}

	DBG1("SPMP8000 USB device initialized\n");

	return 0;
}

/**
 * Fills a CBW struct with the given params
 * @param cbw Pointer to cbw struct to be filled
 * @param cmd Command in the cbw
 * @param addr Address in the cbw
 * @param len Length in the cbw
 * @param flag Flag in the cbw
 */
static void fill_cbw(cbw_t *cbw, uint32_t cmd, uint32_t addr, uint32_t len, uint8_t flag)
{
	memset(cbw, 0, sizeof(cbw_t));

	cbw->sig	= htole32(USB_CBW_SIG);
	cbw->tag	= htole32(USB_CBW_TAG);
	cbw->xlen	= htole32(len);
	cbw->flag	= flag;
	cbw->blen	= 0xA;
	cbw->cmd	= htole32(cmd);
	cbw->adr	= htobe32(addr);	/* Address is in big endian */
}

/**
 * Perform a USB transaction.
 * Transactions can be:
 * 	write command: wdata and rdata NULL
 * 	write command + write data: rdata NULL
 * 	write command + read data : wdata NULL
 * 	write command + write data + read data: not valid, don't use
 * @param di Device info struct of opened and inited device
 * @param cmd Command to be sent
 * @param addr Address to be sent
 * @param wlen Length of data to be written in a write+write txn
 * @param wdata Pointer to data to be written in a write+write txn, wlen size
 * @param rlen Length of data to be read in a write+read txn
 * @param rdata Pointer to data buf to fill in write+read txn, rlen size
 * @param flag Flaf to be sent in CBW
 * @returns 0 if OK, <0 on error
 */
int usb_txn(devinfo_t *di, uint32_t cmd, uint32_t addr, uint32_t len,
	    char *data, uint8_t flag)
{
	cbw_t cbw;
	csw_t csw;
	int ret;
	usb_dev_handle *ud = di->ud;

	DBG2("USB txn: Cmd:0x%08X, addr:0x%08X, len:0x%08X, data:%s\n",
		cmd, addr, len, (data ? "yes" : "NULL"));

	/* Transaction stage 1, Send CBW */
	fill_cbw(&cbw, cmd, addr, len, flag);
	ret = usb_bulk_write(ud, 0x02, (char*)&cbw, sizeof(cbw_t), USB_TIMEOUT);
	if (ret < 0)
	{
		DBGE("USB I/O: Can't send CBW\n");
		return -1;
	}

	/* Transaction stage 2, write or read data */
	if ((flag == SCSI_FLAG_WRITE) && data)		/* Write + write txn */
		ret = usb_bulk_write(ud, 0x02, data, len, USB_TIMEOUT);
	else if ((flag == SCSI_FLAG_READ) && data)	/* Write + read txn */
		ret = usb_bulk_read(ud, 0x01, data, len, USB_TIMEOUT);

	if (ret < 0)
	{
		DBGE("USB I/O: Can't %s data: %d\n",
		     (flag == SCSI_FLAG_WRITE) ? "write" : "read", ret);
		return -1;
	}
	
	/* Transaction stage 3, Get CSW */
	ret = usb_bulk_read(ud, 0x81, (char*)&csw, sizeof(csw_t), USB_TIMEOUT);
	if ((ret < sizeof(csw_t)) || (csw.sig != le32toh(USB_CSW_SIG))) {
		DBGE("USB I/O: CSW invalid\n");
		return -1;
	}

	return 0;
}
