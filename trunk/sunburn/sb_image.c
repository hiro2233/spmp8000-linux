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

#include <endian.h>
#include <usb.h>

#include "sb.h"

#define PATPAGE_MAGIC			0x55AACC33UL
#define PATPAGE_END			0xFFFFFFFFUL
#define PATPAGE_OFFSET_MAGIC		0
#define PATPAGE_OFFSET_ID		1
#define PATPAGE_OFFSET_SIZE		2
#define PATPAGE_OFFSET_LASTPAGEPLUS1	3
#define PATPAGE_OFFSET_FIRSTPAGE	4

#define PATPAGE_ID			0xFFFE0401

#define FLASH_WRITE_MAXRETRIES		128

void flash_offset_calc(devinfo_t *di, flashoffsets_t *fo,
			      int offset, int length)
{
	fo->fb = offset / di->bs;
	fo->lb = (offset + length - 1) / di->bs;
	fo->nb = fo->lb - fo->fb + 1;
	fo->fp = offset / di->ps;
	fo->lp = (offset + length - 1) / di->ps;
	fo->np = fo->lp - fo->fp + 1;
}

/**
 * Prints a bootfile_info_t structure
 * @param di Device info struct of inited device
 * @param binf The bootfile info struct to be printed
 */
void image_print_bootfile_info(devinfo_t *di, bootfile_info_t *binf)
{
	int numpages = binf->lastpage - binf->firstpage;

	DBG("- Bootfile info:\n");
	DBG(" Patpage:     0x%08X\n", binf->patpage);
	DBG(" ID:          0x%08X\n", binf->id);
	DBG(" Size:        %d\n", binf->size);
	DBG(" First page:  0x%08X\n", binf->firstpage);
	DBG(" Last page:   0x%08X\n", binf->lastpage);

	if (numpages > (binf->size / di->ps))
		DBG(" Pages non-continous!\n");
}

/**
 * Extracts the infos of a bootfile from a PAT page
 * @param patpageno Number of PAT page
 * @param patpage Pointer to buffer containing the PAT page
 * @param pagesize Size of the page
 * @param binf Pointer to the bootfile_info_t to fill
 * @returns 0 if OK, <0 on error
 */
int image_get_bootfile_info(int patpageno, uint32_t *patpage, uint32_t pagesize,
			    bootfile_info_t *binf)
{
	int i = PATPAGE_OFFSET_FIRSTPAGE;

	if (patpage[PATPAGE_OFFSET_MAGIC] != le32toh(PATPAGE_MAGIC))
	{
		DBG2("Not a PAT page - magic word not found\n");
		return -1;
	};

	binf->patpage = patpageno;

	binf->id = le32toh(patpage[PATPAGE_OFFSET_ID]);
	binf->size = le32toh(patpage[PATPAGE_OFFSET_SIZE]);
	binf->firstpage = le32toh(patpage[PATPAGE_OFFSET_FIRSTPAGE]);

	while ((patpage[i] != PATPAGE_END) && (i < pagesize / sizeof(uint32_t)))
		i++;

	binf->lastpage = le32toh(patpage[i - 1]);

	return 0;
}

/**
 * Extracts the infos of a bootfile from a PAT page, reading it through USB
 * @param di Device info struct of opened and inited device
 * @param patpagenum Number of the PAT page to analyze
 * @param binf Bootfile info struct fo fill
 * @returns 0 if OK and PAT parsed, <0 on error, 1 if not PAT
 */
int image_get_bootfile_info_usb(devinfo_t *di, uint32_t patpagenum,
				bootfile_info_t *binf)
{
	int ret;
	uint32_t *buf;
	
	buf = malloc(di->ps);
	if (buf == NULL)
	{
		DBGE("Can't alloc buffer for nand page\n");
		return -1;
	}

	ret = cmd_read_flash_page(di, patpagenum, (char*)buf);
	if (ret)
		goto out;

	ret = image_get_bootfile_info(patpagenum, buf, di->ps, binf);
	if (ret)
		ret = 1;

out:
	free(buf);
	return ret;
}

/**
 * Prints info about all PAT pages and bootfiles found in the device
 * @param di Device info struct of opened and inited device
 * @returns 0 if OK, <0 on error
 */
int image_show_pats_usb(devinfo_t *di)
{
	int ret;
	int curpage = 0;
	bootfile_info_t binf;

	while (curpage < PAT_SEARCH_RANGE_PAGES)
	{
		ret = image_get_bootfile_info_usb(di, curpage, &binf);
		if (ret < 0)
			return -1;
		else if (ret == 0)
			image_print_bootfile_info(di, &binf);

		curpage++;
	}

	return 0;
}

/**
 * Reads a bootfile through USB
 * 
 * @param usbdev Handle of USB device
 * @param nandconf NAND configuration structure containing FLASH info
 * @param patpagenum Page number of the PAT for the bootfile
 * @param data Output buffer for image, needs to be pre-allocated with pagesize
 *		 multiple length
 * @return 0 if OK, <0 on error
 */
int image_get_bootfile_usb(devinfo_t *di, uint32_t patpagenum, char* data)
{
	int ret, i;
	uint32_t *pat;
	char *bufpoi;

	pat = malloc(di->ps);
	if (pat == NULL)
	{
		DBGE("Can' alloc buffer for nand page\n");
		return -1;
	}

	ret = cmd_read_flash_page(di, patpagenum, (char*)pat);
	if (ret)
		goto fail;

	if (pat[PATPAGE_OFFSET_MAGIC] != le32toh(PATPAGE_MAGIC))
	{
		DBGE("Not a PAT page - magic word not found\n");
		goto fail;
	};

	i = PATPAGE_OFFSET_FIRSTPAGE;
	while ( (pat[i] != PATPAGE_END) && (i < (di->ps / sizeof(uint32_t)) ))
	{
		bufpoi = data + (i - PATPAGE_OFFSET_FIRSTPAGE) * di->ps;
		ret = cmd_read_flash_page(di, pat[i], bufpoi);
		if (ret)
			goto fail;
		i++;
	}

	free(pat);
	return 0;
fail:
	free(pat);
	return -1;
	
}

/**
 * Write data to flash pages and verify them
 * @param di Device info struct of opened and inited device
 * @param page Number of first page to write & verify (needs to be erased first)
 * @param num Number of pages to write to
 * @param databuf Buffer with data to write
 * @param veribuf Buffer for reading back page (page size length) 
 * @returns 0 if page OK, 1 if page not OK, <0 on error
 */
static int image_write_verify_pages_usb(devinfo_t *di, int page, int num,
					char* databuf, char* veribuf)
{
	int i, ret;
	char *bufpoi = databuf;

	for (i = page; i < page + num; i++)
	{
		ret = cmd_write_flash_page(di, i, bufpoi);
		if (ret)
			return -1;

		ret = cmd_read_flash_page(di, i, veribuf);
		if (ret)
			return -1;

		ret = memcmp(bufpoi, veribuf, di->ps);
		if (ret)
		{
			DBGE("Flash page error on page %08X at %04X\n",
			     i, ret);
			return -1;
		}
	
		bufpoi += di->ps;
	}

	return 0;
}

/**
 * Writes data at any offset into NAND flash taking care of erasing and
 * re-writing blocks
 * @param di Device info struct of opened and inited device
 * @param offset Offset to write to
 * @param data Pointer to data to be written
 * @param len Length of dtat to be written
 * @returns 0 if OK, <0 on error
 * 
 * TODO optimize to only read pages that don't get overwritten
 */
int image_write_random_usb(devinfo_t *di, int offset, char* data, int len)
{
	flashoffsets_t fo;
	char *blockbuf, *veribuf;
	int ret;

	flash_offset_calc(di, &fo, offset, len);
	
	DBG2("offset: %08X, len: %08X |", offset, len);
	DBG2("FB: %08X, LB: %08X, NB: %d, FP: %08X, LP: %08X, NP: %d\n", fo.fb,
	     fo.lb, fo.nb, fo.fp, fo.lp, fo.np);

	/* Allocate space for all to be erased data plus a page for verifying */
	blockbuf = malloc(fo.nb * di->bs + di->ps);
	if (blockbuf == NULL)
	{
		DBGE("Can't allocate block buffer\n");
		return -1;
	}

	veribuf = blockbuf + fo.nb * di->bs;

	/* Read current blocks content */
	ret = cmd_read_flash_pages(di, fo.fb * di->ppb, fo.nb * di->ppb,
				   blockbuf);
	if (ret)
	{
		DBGE("Can't read block content\n");
		goto fail;
	}
	
	/* Erase the data blocks needed */
	ret = cmd_erase_blocks(di, fo.fb * di->ppb, fo.nb);
	if (ret)
	{
		DBGE("Can't erase blocks\n");
		goto fail;
	}

	/* Copy data into the block buffer, overwriting previous content */
	memcpy(blockbuf + (offset - (fo.fb * di->bs)), data, len);

	/* Write back all pages and verify them */
	ret = image_write_verify_pages_usb(di, fo.fb * di->ppb, fo.nb * di->ppb,
					   blockbuf, veribuf);
	if (ret)
	{
		DBGE("Error writing flash pages back\n");
		goto fail;
	}

	free(blockbuf);
	return 0;

fail:
	free(blockbuf);
	return -1;
}

/**
 * Fills header and page data into a PAT page
 * @param di Device info struct of inited device
 * @param datapage Number of the first data page
 * @param patbuf Buffer holding the PAT data
 * @param size Size of the file this PAT belongs to
 * @param id ID field of PAT
 */
static void image_fill_pat(devinfo_t *di, int datapage, char *patbuf,
			   uint32_t size, uint32_t id)
{
	uint32_t *pat=(uint32_t*)patbuf;
	int numpages = ((size - 1) / di->ps) + 1;
	int i;
	int j = 0;

	pat[PATPAGE_OFFSET_MAGIC] = htole32(PATPAGE_MAGIC);
	pat[PATPAGE_OFFSET_SIZE] = htole32(size);
	pat[PATPAGE_OFFSET_ID] = htole32(id);
	pat[PATPAGE_OFFSET_LASTPAGEPLUS1] = htole32(datapage + numpages);
	pat[PATPAGE_OFFSET_FIRSTPAGE + numpages] = htole32(PATPAGE_END);

	for (i = datapage; i < datapage + numpages; i++)
	{
		pat[PATPAGE_OFFSET_FIRSTPAGE + j++] = i;
	}
}

/**
 * Writes a bootfile to the flash, including its patpage
 * @param di Device info struct of opened and inited device
 * @param patpage Number of page to write PAT to
 * @param datapage Number of first data page
 * @param data Buffer containing the data to be written
 * @param len Length of data to be written
 * @returns 0 if OK, <0 on error
 * NOTE: PAT can't be in the same block as the data, as the function will erase
 *       the block after writing data
 * NOTE: This currently only supports file sizes which require a PAT
 * 	 of maximum 1 page length
 * 	 2K pages: 4.161.536 bytes
 * 	 4K pages: 16.711.680 bytes
 * TODO: Implement multi-page PAT support. Test if romboot supports it at all
 */
int image_write_bootfile_usb(devinfo_t *di, uint32_t id, int patpage,
			     int datapage, char* data, int len)
{
	int ret;
	int ps = di->ps;
	flashoffsets_t fo;
	char *patbuf;

	flash_offset_calc(di, &fo, datapage * di->ps, len);

	if (len > (ps - 4 * sizeof(uint32_t)) * ps)
	{
		DBGE("Data length bigger than what fits to 1 PAT page\n");
		return -1;
	}
	
	patbuf = malloc(ps);
	if (patbuf == NULL)
	{
		DBGE("Can't allocate buffer for pat\n");
		return -1;
	}
	memset(patbuf, 0xFF, ps);

	/* Write data */
	ret = image_write_random_usb(di, datapage * di->ps, data, len);
	if (ret)
	{
		DBGE("Can't write bootfile data\n");
		goto fail;
	}

	/* Create PAT */
	image_fill_pat(di, datapage, patbuf, len, id);

	/* Write PAT */
	ret = image_write_random_usb(di, patpage * di->ps, patbuf, ps);
	if (ret)
	{
		DBGE("Can't write PAT page - 2nd round\n");
		goto fail;
	}

	free(patbuf);
	return 0;

fail:
	free(patbuf);
	return -1;
}
