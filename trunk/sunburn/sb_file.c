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
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <usb.h>

#include "sb.h"

enum memtype {
	RAM = 0,
	FLASH
};

/**
 * Dumps a flash or mem region to a file
 * @param di Device info struct of opened and inited device
 * @param ramflash FILE_DUMP_RAM or FILE_DUMP_FLASH
 * @param addr Address to start dump from
 * @param len Length in bytes to dump
 * @param fname Path and filename to write to
 * @returns 0 if OK, <0 on error
 */
static int file_mem_dump(devinfo_t *di, enum memtype ramflash, int addr,
			 int len, char* fname)
{
	int fd, ret;
	int wl = 0, poi = 0, i = 0;
	char *pagebuf;
	flashoffsets_t fo;

	fd = open(fname, O_WRONLY | O_CREAT | O_TRUNC,
		  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (fd == -1)
	{
		DBGE("Can't open output file: %s\n", strerror(errno));
		return -1;
	}

	pagebuf = malloc(di->ps);
	if (pagebuf == NULL)
	{
		DBGE("Can't allocate space for page buffer\n");
		close(fd);
		return -1;
	}

	if (ramflash == FLASH)
	{
		flash_offset_calc(di, &fo, addr, len);
		poi = len;
	}
	else
		fo.np = 0;

	DBG2("addr: %08X, len.%08X, fp: %08X, np: %08X\n", addr, len, fo.fp,
	     fo.np);

	while ((poi < len) || i < fo.np)
	{
		if (ramflash == FLASH)
		{
			ret = cmd_read_flash_page(di, fo.fp + i, pagebuf);

			/* Whole page needs to be read, but only write part
			 * of the last page if len is not multiple pagesize */
			if (((i + 1) * di->ps) > len)
				wl = len - ((i - 1) * di->ps);
			else
				wl = di->ps;

			/* Next page */
			i++;
		}
		else
		{
			/* Only read part of mem at end if len is not multiple
			 * page size */
			if ((poi + di->ps) > len)
				wl = len - poi;
			else
				wl = di->ps;

			ret = cmd_read_mem(di, addr + poi, wl, pagebuf);

			poi += wl;
		}

		if (ret)
		{
			DBGE("Can't read %s\n", (ramflash == FLASH) ?
				"flash" : "memory");
			goto fail;
		}

		ret = write(fd, pagebuf, wl);
		if (ret < wl)
		{
			DBGE("Can't write to output file: %s\n",
			     strerror(errno));
			ret = -1;
			goto fail;
		}
	}

	ret = 0;

fail:
	free(pagebuf);
	close(fd);
	return ret;
}

/**
 * Dumps RAM content to a file
 * @param di Device info struct of opened and inited device
 * @param addr Address to start dump from
 * @param len Length in bytes to dump
 * @param fname Path and filename to write to
 * @returns 0 if OK, <0 on error
 */ 
inline int file_ram_dump(devinfo_t *di, int addr, int len, char* fname)
{
	return file_mem_dump(di, RAM, addr, len, fname);
}

/**
 * Dumps FLASH content to a file
 * @param di Device info struct of opened and inited device
 * @param addr Address to dump from
 * @param len Length in bytes to dump
 * @param fname Path and filename to write to
 * @returns 0 if OK, <0 on error
 */
inline int file_flash_dump(devinfo_t *di, int addr, int len, char* fname)
{
	return file_mem_dump(di, FLASH, addr, len, fname);
}

/**
 * Reads a bootfile from the device
 * Rounds output up to flash page size for simplicity
 * (cmd_read_flash_page can only read full pages)
 * @param di Device info struct of opened and inited device
 * @param bi Bootfile info struct of the bootfile you want
 * @param filename to dump to
 * @returns 0 if OK, <0 on error
 */
int file_bootfile_read(devinfo_t *di, bootfile_info_t *bi, char* fname)
{
	int fd, ret;
	char *file;
	int filesize = di->ps * (((bi->size - 1) / di->ps) + 1);

	fd = open(fname, O_WRONLY | O_CREAT | O_TRUNC,
		  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (fd == -1)
	{
		DBGE("Can't open output file: %s\n", strerror(errno));
		return -1;
	}

	file = malloc(filesize);
	if (file == NULL)
	{
		DBGE("Can't allocate space for bootfile\n");
		close(fd);
		return -1;
	}

	ret = image_get_bootfile_usb(di, bi->patpage, file);
	if (ret)
	{
		DBGE("Can't read bootfile\n");
		goto fail;
	}

	ret = write(fd, file, filesize);
	if (ret < filesize)
	{
		DBGE("Can't write to output file\n");
		goto fail;
	}

	ret = 0;

fail:
	free(file);
	close(fd);
	return ret;
}

/**
 * Dumps all bootfiles from the device with pre-defined names
 * @param di Device info struct of opened and inited device
 * @returns 0 if OK, <0 on error
 */
int file_bootfiles_dump(devinfo_t *di)
{
	int ret;
	int curpage = 0;
	char fname[128];
	bootfile_info_t binf;

	while (curpage < PAT_SEARCH_RANGE_PAGES)
	{
		ret = image_get_bootfile_info_usb(di, curpage, &binf);
		if (ret < 0)
			return -1;
		else if (ret == 0)
		{
			ret = sprintf(fname, "BF%04X.bin", curpage);
			if (ret < 0)
				return -1;
			ret = file_bootfile_read(di, &binf, fname);
			if (ret < 0)
				return -1;
		}
		curpage++;
	}

	return 0;
}

/**
 * Open a file in read-only mode and mmaps it
 * @param fname filename to open
 * @param fd Returns the file descriptor in this arg
 * @param length Returns the length of the file in this arg
 * @param data Returns the mmap pointer of the file in this arg
 * @return 0 if OK, <0 on error
 */
int file_open_mmap(char* fname, int *fd, int *length, char** data)
{
	*fd = open(fname, O_RDONLY);
	if (*fd == -1)
	{
		DBGE("Can't open input file: %s\n", strerror(errno));
		return -1;
	}

	/* Get file size and rewind */
	*length = (int)lseek(*fd, 0, SEEK_END);
	if (*length == -1)
	{
		DBGE("Can't determinate file size: %s\n", strerror(errno));
		goto fail;
	}
	
	*data = mmap(NULL, *length, PROT_READ, MAP_SHARED, *fd, 0);
	if (*data == MAP_FAILED)
	{
		DBGE("Can't mmap input file: %s\n", strerror(errno));
		goto fail;
	}

	return 0;

fail:
	close(*fd);
	return -1;
}

/**
 * Writes a file to a flash or mem region
 * @param di Device info struct of opened and inited device
 * @param addr Address to write to
 * @param fname Path and filename to write
 * @returns 0 if OK, <0 on error
 */
int file_flash_write(devinfo_t *di, int addr, char* fname)
{
	int fd, length;
	int ret;
	char *data;

	ret = file_open_mmap(fname, &fd, &length, &data);
	if (ret)
		return -1;

	ret = image_write_random_usb(di, addr, data, length);
	if (ret)
	{
		DBGE("Can't write file to flash\n");
		goto out;
	}

	ret = 0;

out:
	munmap(data, length);
	close(fd);
	return ret;
}

/**
 * Writes a bootfile to flash including its PAT table
 * @param di Device info struct of opened and inited device
 * @param id ID to use in PAT table
 * @param patpage Number of the page to write the PAT to
 * @param datapage Number of the page to write the data to
 * @param filename name of the file to write
 */
int file_bootfile_write(devinfo_t *di, uint32_t id, int patpage, int datapage,
			char* fname)
{
	int fd, length;
	int ret;
	char *data;

	ret = file_open_mmap(fname, &fd, &length, &data);
	if (ret)
		return -1;

	ret = image_write_bootfile_usb(di, id, patpage, datapage, data, length);
	if (ret)
	{
		DBGE("Can't write bootfile\n");
		goto out;
	}

	ret = 0;
out:
	munmap(data, length);
	close(fd);
	return ret;
}
