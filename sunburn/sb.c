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
 * MERCHANTABIITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <endian.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <usb.h>

#include "sb.h"

int dl = 0;
int initdram = 0;

/* Flash config data for the Letcool device with Micron 29F32G08 flash */
char fc_29F32G08[sizeof(nandconf_t)] =
{
				0x80, 0x00, 0x80, 0x10, 0x00, 0x10, 0x80, 0x00,
				0x20, 0x00, 0x20, 0x00, 0x00, 0x20, 0x0D, 0x00,
				0x00, 0x00, 0x00, 0x00, 0x5d, 0xf0, 0x07, 0x00,
				0x80, 0x00, 0x05, 0x00, 0x05, 0x00, 0x00, 0x00,
				0x2c, 0xd7, 0x94, 0x3e, 0x84, 0x00, 0x00, 0x00,
				0x2c, 0xd7, 0x94, 0x3e, 0x84, 0x00, 0x00, 0x00,
				0x2c, 0xd7, 0x94, 0x3e, 0x84, 0x00, 0x00, 0x00,
				0x2c, 0xd7, 0x94, 0x3e, 0x84, 0x00, 0x00, 0x00
};

void hexdump(unsigned char *data, int length, int base)
{
	int i;

	for (i = 0; i < length; i++)
	{
		if ((i % 16) == 0)
		{
			if ((i / 16) > 0)
			{
				base += 16;
				printf("\n");
			}
			printf("%08X: ", base);
		}
		printf("%02X ", data[i]);
	}

	if (i % 15 != 0)
		printf("\n");
}

/**
 * Prints information about the NAND config
 * @param nc Pointer to struct nandconf_t containing the info
 */
void print_nand_info(nandconf_t *nc)
{
	int ppb = le16toh(nc->pagesperblock);
	int ps = le16toh(nc->pagesize);
	int pl = le16toh(nc->payloadlen);
	int tb = le16toh(nc->totalblocks);
	uint8_t *fid = nc->flashid1;
	
	DBG("- NAND Info:\n");
	DBG("Pages per block:        %d\n", ppb);
	DBG("Real Pagesize:          %d\n", ps);
	DBG("Pagesize:               %d\n", pl);
	DBG("Total number of blocks: %d\n", tb);
	DBG("ECC mode:               %d\n", nc->eccmode);
	DBG("Total size :            %u KB\n", tb * ppb * (ps / 1024));
	DBG("NAND ID:                %02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X\n",
		fid[0], fid[1], fid[2], fid[3], fid[4], fid[5], fid[6], fid[7]);
	DBG("Hedump:\n");
	hexdump((unsigned char*)nc, sizeof(nandconf_t), 0);
	printf("\n");
}

/**
 * Prints information about the device
 * @param di Device info struct of opened and inited device
 * @returns 0 if OK, <0 on error
 */
int print_device_infos(devinfo_t *di)
{
	int ret;
	nandconf_t nc;
	char devid[DEVICE_ID_LENGTH];

	DBG("-- Device information --\n");

	ret = cmd_read_devid(di, devid);
	if (ret)
	{
		DBG("Can't read device ID\n");
		return -1;
	}

	DBG("- ROMBOOT ID:\n");
	hexdump((unsigned char*)devid, DEVICE_ID_LENGTH, 0);

	ret = cmd_get_flash_info(di, &nc);
	if (ret)
		return -1;
	print_nand_info(&nc);

	if (initdram)
	{
		ret = cmd_init_dram(di);
		if (ret)
			return -1;
		ret = cmd_get_flash_info(di, &nc);
		if (ret)
			return -1;
		DBG("- DRAM Inited\n\n");
		print_nand_info(&nc);
		initdram = 0;
	}	
	ret = image_show_pats_usb(di);
	if (ret)
		return -1;

	return 0;
}

int main(int argc, char **argv)
{
	int ret, opt;
	char options[] = "ia:r:f:FB:GdDlc";

	devinfo_t di;
	nandconf_t nc;

	char function = 0;
	char *filename = NULL;
	unsigned int addr, functarg;
	int flashconfig = 1;

	opterr = 0;

	DBG("Sunburn - Sunplus SPMP8000 firmware flashing tool " SB_VERSION
		"\n\n");

	while ((opt = getopt(argc, argv, options)) != -1)
	switch (opt)
	{
	case '?':
		switch (optopt)
		{
		case 'a':
		case 'f':
		case 'B':
			DBGE("Option -%c requires an argument.\n", optopt);
			return 1;
		default:
			DBGE("Unknown option specified\n");
			return 1;
		}
		break;
	case 'a':
		ret = sscanf(optarg, "0x%8X", &addr);
		if (ret < 1)
		{
			DBGE("Invalid address specified\n");
			return 1;
		}
		break;
	case 'b':
	case 'i':
	case 'F':
	case 'l':
		function = opt;
		break;
	case 'f':
	case 'r':
	case 'B':
		function = opt;
		ret = sscanf(optarg, "0x%8X", &functarg);
		if (ret < 1)
		{
			DBGE("Invalid parameter\n");
			return 1;
		}
		break;
	case 'c':
		flashconfig = 0;
		break;
	case 'd':
		if (dl < 3)
			dl++;
		break;
	case 'D':
		initdram = 1;
		break;
	default:
		return 1;
	}

	if (optind < argc)
		filename = argv[optind];

	if (((function == 'r') || (function == 'f') || (function == 'F') ||
		(function == 'B') || (function == 'l')) && filename == NULL)
	{
		DBGE("No filename specified\n");
		return 1;
	}

	if (function == 0)
	{
		DBG(
		"Copyright (C) 2011, Zoltan Devai - zdevai@gmail.com - "
		"http://sunnap.blogspot.com\n"
		"Credits to Alemaxx and openschemes.com\n\n"
		"Usage:\n"
		"\tsunburn <options> [suboption] [filename]\n"
		"Options:\n"
		" -i\t\tPrint information about the device\n"
		" -a <address>\t\tAddress in 0x hex format (for options that need it)\n"
		" -r <length>\tRead RAM from -a address and <length>\n"
		" -f <length>\tRead FLASH from -a address and <length>\n"
		" -F\t\tWrite file to flash at -a address\n"
		" -b\t\tDump all bootfiles to BP<patpageno>.bin files\n"
		" -B <address>\tWrite bootfile to flash with -a PAT address,"
		" and <adress> data address\n"
		" -l\t\tDump ROM bootloader to file\n"
		" -D\t\tRun the DRAM init code in FLASH\n\n"
		);
		return 1;
	}
	
	ret = usb_spmp8000_init(&di.ud);
	if (ret)
		return 1;

	DBG("- SPMP8000 device found\n");

	/* Need to do this before the others as it may init the DRAM itself */
	if (function == 'i')
	{
		ret = print_device_infos(&di);
		if (ret)
			goto out;
		goto end;
	}

	if (initdram)
	{
		ret = cmd_init_dram(&di);
		DBGE("Can't init DRAM\n");
		if (ret)
			goto out;
		DBG("- DRAM Init called\n");
	}

	if (flashconfig)
	{
		ret = cmd_write_flash_config(&di, (nandconf_t *)&fc_29F32G08);
		if (ret)
		{
			DBGE("Can't send flash configuration\n");
			goto out;
		}
		DBG("- FLASH config sent\n");
	}

	ret = cmd_get_flash_info(&di, &nc);
	if (ret)
	{
		DBGE("Can't read flash config\n");
		goto out;
	}

	switch (function)
	{
		case 'l':
			DBG("- Dumping the romboot code to %s\n", filename);
			ret = file_ram_dump(&di, ROMBOOT_LOCATION,
					    ROMBOOT_LENGTH, filename);
			break;
		case 'b':
			DBG("- Dumping the bootfiles to BF<pat>.bin files\n");
			ret = file_bootfiles_dump(&di);
			break;
		case 'r':
			DBG("- Dumping RAM from %08X, length %08X to %s\n",
			    addr, functarg, filename);
			ret = file_ram_dump(&di, addr, functarg, filename);
			break;
		case 'f':
			DBG("- Dumping FLASH from %08X, length %08X to %s\n",
			    addr, functarg, filename);
			ret = file_flash_dump(&di, addr, functarg, filename);
			break;
		case 'F':
			DBG("- Writing %s to flash addr %08X\n", filename,
			    addr);
			ret = file_flash_write(&di, addr, filename);
			break;
		case 'B':
			DBG("- Writing bootfile %s to %08X PAT addr and %08X"
			    "PAT addr\n", filename, functarg, addr);
			ret = file_bootfile_write(&di, 0x1984BABE,
						  addr / di.ps,
						  functarg / di.ps, filename);
			break;
		default:
			DBG("Should not happen\n");
			goto out;
			break;
	}

	if (ret)
		DBGE("Operation failed\n");
	else
		DBG("Done\n");

end:
	usb_close(di.ud);
	return 0;

out:
	usb_close(di.ud);
	return 1;

}