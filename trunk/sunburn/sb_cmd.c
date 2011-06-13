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

#define CMD_USB_SCSI_C2(x)	((x << 8) | 0xC2)

#define CMD_USB_BYPASSBR	CMD_USB_SCSI_C2(0x00)
#define CMD_USB_RAMWRITE	CMD_USB_SCSI_C2(0x01)
#define CMD_USB_RAMREAD		CMD_USB_SCSI_C2(0x02)

#define CMD_USB_EXECUTE		CMD_USB_SCSI_C2(0x05)
#define CMD_USB_BRVERINFO	CMD_USB_SCSI_C2(0x06)
#define CMD_USB_DRAMINIT	CMD_USB_SCSI_C2(0x07)

#define CMD_USB_FLASHCONFREAD	CMD_USB_SCSI_C2(0x10)
#define CMD_USB_FLASHBLKERASE	CMD_USB_SCSI_C2(0x11)
#define CMD_USB_FLASHWRITE	CMD_USB_SCSI_C2(0x12)
#define CMD_USB_FLASHREAD	CMD_USB_SCSI_C2(0x13)

#define CMD_USB_FLASHCONFSEND	CMD_USB_SCSI_C2(0x20)

#define CMD_USB_FLASHWRITEALT	CMD_USB_SCSI_C2(0x30)
#define CMD_USB_FLASHREADALT	CMD_USB_SCSI_C2(0x31)

#define FLASHINFO_LENGTH	0x40

/**
 * Get the NAND config information from the device
 * @param di Device info struct of opened and inited device
 * @param nc Nand config struct to be filled with data
 * @returns 0 if OK, <0 on error
 */
inline int cmd_read_flash_config(devinfo_t *di, nandconf_t *nc)
{
	return usb_txn(di, CMD_USB_FLASHCONFREAD, 0, sizeof(nandconf_t),
		(char*)nc, SCSI_FLAG_READ);
}

/**
 * Get the NAND config information from the device and also fill it into
 * the device info struct
 * @param di Device info struct of opened and inited device
 * @param nc Nand config struct to be filled with data
 * @returns 0 if OK, <0 on error
 */
int cmd_get_flash_info(devinfo_t *di, nandconf_t *nc)
{
	int ret;

	ret = cmd_read_flash_config(di, nc);
	if (ret < 0)
	{
		DBGE("Unable to get flash info\n");
		return ret;
	}
	
	di->ppb = le16toh(nc->pagesperblock);
	di->rps = le16toh(nc->pagesize);
	di->ps = le16toh(nc->payloadlen);
	di->tb = le16toh(nc->totalblocks);
	di->bs = di->ppb * di->ps;

	return 0;
}

/**
 * Sends the NAND config to the device, which gets applied immediately
 * This is needed to have proper flash handling in ISP mode
 * (at least on the Letcool device)
 * @param di Device info struct of opened and inited device
 * @param nc Nand config struct to be sent
 * @returns 0 if OK, <0 on error
 */
inline int cmd_write_flash_config(devinfo_t *di, nandconf_t *nc)
{
	return usb_txn(di, CMD_USB_FLASHCONFSEND, 0, sizeof(nandconf_t),
		       (char*)nc, SCSI_FLAG_WRITE);
}

/**
 * Sends the init DRAM command. Bootloader will extract and run the DRAM init
 * code from flash. Also inits the NAND timings and configuration.
 * @param di Device info struct of opened and inited device
 * @returns 0 if OK, <0 on error
 */
inline int cmd_init_dram(devinfo_t *di)
{
	return usb_txn(di, CMD_USB_DRAMINIT, 0, 0, NULL, SCSI_FLAG_READ);
}

/**
 * Reads from device memory
 * @param di Device info struct of opened and inited device
 * @param addr Address to read from
 * @param len How much to read
 * @param buf Buffer to write the data to
 * @returns 0 if OK, <0 on error
 */
inline int cmd_read_mem(devinfo_t *di, int addr, int len, char* buf)
{
	return usb_txn(di, CMD_USB_RAMREAD, addr, len, buf, SCSI_FLAG_READ);
}

/**
 * Writes to  device memory
 * @param di Device info struct of opened and inited device
 * @param addr Address to write to
 * @param len How much to write
 * @param buf Buffer containing data to write
 * @returns 0 if OK, <0 on error
 */
inline int cmd_write_mem(devinfo_t *di, int addr, int len, char* buf)
{
	return usb_txn(di, CMD_USB_RAMWRITE, addr, len, buf, SCSI_FLAG_WRITE);
}

/**
 * Reads one flash page
 * @param di Device info struct of opened and inited device
 * @param pageno Number of the page
 * @param data Buffer to be filled, page size length
 * @returns 0 if OK, <0 on error
 */
inline int cmd_read_flash_page(devinfo_t *di, uint32_t pageno, char *data)
{
	return usb_txn(di, CMD_USB_FLASHREAD, pageno, di->ps, data,
		       SCSI_FLAG_READ);
}

/**
 * Reads multiple flash pages
 * @param di Device info struct of opened and inited device
 * @param fp Number of the first page
 * @param num Number of pages to read
 * @param data Buffer to be filled
 * @returns 0 if OK, <0 on error
 */
int cmd_read_flash_pages(devinfo_t *di, uint32_t fp, int num, char *data)
{
	int i, ret;

	for (i = fp; i < fp + num; i++)
	{
		ret = cmd_read_flash_page(di, i, data);
		if (ret)
			return -1;

		data += di->ps;
	}

	return 0;
}

/**
 * Writes one flash page
 * @param di Device info struct of opened and inited device
 * @param pageno Number of the page
 * @param data Buffer to be written, page size length
 * @returns 0 if OK, <0 on error
 */
inline int cmd_write_flash_page(devinfo_t *di, uint32_t pageno, char *data)
{
	return usb_txn(di, CMD_USB_FLASHWRITE, pageno, di->ps, data,
		       SCSI_FLAG_WRITE);
}

/**
 * Writes multiple flash pages
 * @param di Device info struct of opened and inited device
 * @param fp Number of the first page
 * @param num Number of pages to write
 * @param data Buffer to be written
 * @returns 0 if OK, <0 on error
 */
int cmd_write_flash_pages(devinfo_t *di, uint32_t fp, int num, char *data)
{
	int i, ret;

	for (i = fp; i < fp + num; i++)
	{
		ret = cmd_write_flash_page(di, i, data);
		if (ret)
			return -1;

		data += di->ps;
	}

	return 0;
}

/**
 * Erases one flash block
 * The block is addressed by any of its pages
 * @param di Device info struct of opened and inited device
 * @param pageno Number of one of the pages in the block
 * @returns 0 if OK, <0 on error
 */
inline int cmd_erase_block(devinfo_t *di, uint32_t pageno)
{
	return usb_txn(di, CMD_USB_FLASHBLKERASE, pageno, 0,
		       NULL, SCSI_FLAG_WRITE);
}

/**
 * Erases multiple flash blocks
 * @param di Device info struct of opened and inited device
 * @param firstpage The first page of the first block to erase
 * @param numblock The number of blocks to erase
 * @returns 0 if OK, <0 on error
 */
int cmd_erase_blocks(devinfo_t *di, uint32_t firstpage, int numblocks)
{
	int ret, i;

	for (i = 0; i < numblocks; i++)
	{
		ret = cmd_erase_block(di, firstpage + i * di->ppb);
		if (ret)
			return -1;
	}

	return 0;
}

/**
 * Reads the device id from memory, filled by the romboot code
 * @param di Device info struct of opened and inited device
 * @param devid Buffer of DEVICE_ID_LENGTH bytes length
 * @returns 0 if OK, <0 on error
 */
inline int cmd_read_devid(devinfo_t *di, char *devid)
{
	return usb_txn(di, CMD_USB_RAMREAD, DEVICE_ID_LOCATION,
		       DEVICE_ID_LENGTH, devid, SCSI_FLAG_READ);
}
