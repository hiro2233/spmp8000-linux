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
extern int dl;	/**< Debug level */
void hexdump(unsigned char *data, int length, int base);

#define SB_VERSION	"v1.0"

#define DBG(format, ...) do { printf(format,  ## __VA_ARGS__); } \
			 while (0)
#define DBGE(format, ...) do { printf("- Error: " format,  ## __VA_ARGS__); } \
			 while (0)
#define DBG1(format, ...) do { if (dl > 0) printf("-1-:" format, \
				## __VA_ARGS__); }  while (0)
#define DBG2(format, ...) do { if (dl > 1) printf("-2-:" format, \
				## __VA_ARGS__); }  while (0)

#define SCSI_FLAG_READ		0x80
#define SCSI_FLAG_WRITE		0x0

#define PAT_SEARCH_RANGE_PAGES		512

#define DEVICE_ID_LOCATION	0x9D800010
#define DEVICE_ID_LENGTH	0x8

#define ROMBOOT_LOCATION	0x98000000
#define ROMBOOT_LENGTH		64*1024

typedef struct
{
	uint32_t patpage;
	uint32_t id;
	uint32_t size;
	uint32_t firstpage;
	uint32_t lastpage;
} bootfile_info_t;

typedef struct
{
	unsigned int ppb;	/**< Pages per block */
	unsigned int rps;	/**< Real page size (with OOB) */
	unsigned int ps;	/**< Page size (for data) */
	unsigned int bs;	/**< Block size (bytes) */
	unsigned int tb;	/**< Total num of blocks */
	usb_dev_handle *ud;	/**< USB device handle */
} devinfo_t;

/**
 * Nand config info layout
 * Depending on romboot version, this is found at memory locations
 * 0x9D805300 (v4) (found in my letool device)
 * 0x9D805820 (v3) (alemaxx)
 * 
 * However, this is better obtained with the dedicated USB command that
 * should work for both versions.
 */
typedef struct
{
	uint16_t pagesperblock;
	uint16_t pagesize;
	uint16_t payloadlen;
	uint16_t unknown1;
	uint16_t unknown2;
	uint16_t unknown3;
	uint16_t totalblocks;
	uint8_t  unknown4;
	uint8_t eccmode;
	uint8_t unkown5[16];
	uint8_t flashid1[16];
	uint8_t flashid2[16];
} __attribute__((packed)) nandconf_t;

typedef struct
{
	int fb;		/**< First block */
	int lb;		/**< Last block */
	int nb;		/**< Number of blocks */
	int fp;		/**< First page */
	int lp;		/**< Last page */
	int np;		/**< Number of pages */
} flashoffsets_t;

/* from fu_cmds.c */
inline int cmd_read_flash_config(devinfo_t *di, nandconf_t *nc);
int cmd_get_flash_info(devinfo_t *di, nandconf_t *nc);
inline int cmd_write_flash_config(devinfo_t *di, nandconf_t *nc);
int cmd_init_dram(devinfo_t *di);
inline int cmd_read_mem(devinfo_t *di, int addr, int len, char* buf);
inline int cmd_write_mem(devinfo_t *di, int addr, int len, char* buf);
inline int cmd_read_flash_page(devinfo_t *di, uint32_t pageno, char *data);
int cmd_read_flash_pages(devinfo_t *di, uint32_t fp, int num, char *data);
inline int cmd_write_flash_page(devinfo_t *di, uint32_t pageno, char *data);
int cmd_write_flash_pages(devinfo_t *di, uint32_t fp, int num, char *data);
inline int cmd_erase_block(devinfo_t *di, uint32_t pageno);
int cmd_erase_blocks(devinfo_t *di, uint32_t firstpage, int numblocks);
inline int cmd_read_devid(devinfo_t *di, char *devid);

/* from fu_image.c */
void flash_offset_calc(devinfo_t *di, flashoffsets_t *fo, int offset,
		       int length);
void image_print_bootfile_info(devinfo_t *di, bootfile_info_t *binf);
int image_get_bootfile_info(int patpage, uint32_t *patbuf, uint32_t pagesize,
			    bootfile_info_t *binf);
int image_get_bootfile_info_usb(devinfo_t *di, uint32_t patpagenum,
				bootfile_info_t *binf);
int image_get_bootfile_usb(devinfo_t *di, uint32_t patpagenum, char* data);
int image_write_random_usb(devinfo_t *di, int offset, char* data, int len);
int image_write_bootfile_usb(devinfo_t *di, uint32_t id, int patpage,
			     int datapage, char* data, int len);
int image_show_pats_usb(devinfo_t *di);

/* from fu_usb.c */
int usb_spmp8000_init(usb_dev_handle **udevh);
int usb_txn(devinfo_t *di, uint32_t cmd, uint32_t addr, uint32_t len,
	    char *data, uint8_t flag);

/* from fu_file.c */
inline int file_ram_dump(devinfo_t *di, int addr, int len, char* fname);
inline int file_flash_dump(devinfo_t *di, int addr, int len, char* fname);
int file_bootfile_read(devinfo_t *di, bootfile_info_t *bi, char* fname);
int file_flash_write(devinfo_t *di, int addr, char* fname);
int file_bootfiles_dump(devinfo_t *di);
int file_bootfile_write(devinfo_t *di, uint32_t id, int patpage, int datapage,
			char* fname);
