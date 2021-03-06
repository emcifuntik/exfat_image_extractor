// stdafx.h: ���������� ���� ��� ����������� ��������� ���������� ������
// ��� ���������� ������ ��� ����������� �������, ������� ����� ������������, ��
// �� ����� ����������
//

#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include <direct.h>
#include <locale>
#include <codecvt>
#include <algorithm> 
#include <cctype>

#ifdef _WIN32
#include <Windows.h>
#endif

#pragma pack(push, 1)
struct exfat_table {
	uint8_t magic[3];
	uint8_t oem_name[8];
	uint8_t	__unused1[53];
	uint64_t sector_start;
	uint64_t sector_count;
	uint32_t fat_sector_start;
	uint32_t fat_sector_count;
	uint32_t cluster_sector_start;
	uint32_t cluster_count;
	uint32_t rootdir_cluster;
	uint8_t volume_serial[4];
	struct {
		uint8_t vermin;
		uint8_t vermaj;
	} version;
	uint16_t volume_state;
	uint8_t sector_bits;
	uint8_t spc_bits;
	uint8_t fat_count;
	uint8_t drive_no;
	uint8_t allocated_percent;
	uint8_t reserved[7];
	uint8_t boot_image[390];
	uint16_t boot_signature;
};

struct volume_label {
	uint8_t entry;
	uint8_t label_length;
	char label[22];
	uint8_t reserved[8];
};

#define SECTOR_SIZE(sb) (1u << (sb)->sector_bits)
#define CLUSTER_SIZE(sb) (SECTOR_SIZE(sb) << (sb)->spc_bits)
#define EXFAT_FIRST_DATA_CLUSTER 2
#define EXFAT_LAST_DATA_CLUSTER 0xffffff6
#define EXFAT_ENTRY_SIZE 32

#define EXFAT_ENTRY_EOD		0x00

#define EXFAT_ENTRY_VALID     0x80
#define EXFAT_ENTRY_CONTINUED 0x40
#define EXFAT_ENTRY_OPTIONAL  0x20

#define EXFAT_ENTRY_BITMAP    (0x01 | EXFAT_ENTRY_VALID)
#define EXFAT_ENTRY_UPCASE    (0x02 | EXFAT_ENTRY_VALID)
#define EXFAT_ENTRY_LABEL     (0x03 | EXFAT_ENTRY_VALID)
#define EXFAT_ENTRY_FILE      (0x05 | EXFAT_ENTRY_VALID)
#define EXFAT_ENTRY_FILE_INFO (0x00 | EXFAT_ENTRY_VALID | EXFAT_ENTRY_CONTINUED)
#define EXFAT_ENTRY_FILE_NAME (0x01 | EXFAT_ENTRY_VALID | EXFAT_ENTRY_CONTINUED)
#define EXFAT_ENTRY_FILE_TAIL (0x00 | EXFAT_ENTRY_VALID \
                                    | EXFAT_ENTRY_CONTINUED \
                                    | EXFAT_ENTRY_OPTIONAL)


struct exfat_entry					/* common container for all entries */
{
	uint8_t type;					/* any of EXFAT_ENTRY_xxx */
	uint8_t data[31];
};

#define EXFAT_ENAME_MAX 15

struct exfat_entry_bitmap			/* allocated clusters bitmap */
{
	uint8_t type;					/* EXFAT_ENTRY_BITMAP */
	uint8_t flags;
	uint8_t __unknown1[18];
	uint32_t start_cluster;
	uint64_t size;					/* in bytes */
};

#define EXFAT_UPCASE_CHARS 0x10000

struct exfat_entry_upcase			/* upper case translation table */
{
	uint8_t type;					/* EXFAT_ENTRY_UPCASE */
	uint8_t __unknown1[3];
	uint32_t checksum;
	uint8_t __unknown2[12];
	uint32_t start_cluster;
	uint64_t size;					/* in bytes */
};

struct exfat_entry_label			/* volume label */
{
	uint8_t type;					/* EXFAT_ENTRY_LABEL */
	uint8_t length;					/* number of characters */
	wchar_t name[EXFAT_ENAME_MAX];	/* in UTF-16LE */
};

#define EXFAT_ATTRIB_RO     0x01
#define EXFAT_ATTRIB_HIDDEN 0x02
#define EXFAT_ATTRIB_SYSTEM 0x04
#define EXFAT_ATTRIB_VOLUME 0x08
#define EXFAT_ATTRIB_DIR    0x10
#define EXFAT_ATTRIB_ARCH   0x20

struct exfat_entry_meta1			/* file or directory info (part 1) */
{
	uint8_t type;					/* EXFAT_ENTRY_FILE */
	uint8_t continuations;
	uint16_t checksum;
	bool read_only : 1;
	bool hidden : 1;
	bool system : 1;
	bool reserved : 1;
	bool directory : 1;
	bool archive : 1;
	bool __reserved1 : 1;
	bool __reserved2 : 1;
	bool __reserved3 : 1;
	bool __reserved4 : 1;
	bool __reserved5 : 1;
	bool __reserved6 : 1;
	bool __reserved7 : 1;
	bool __reserved8 : 1;
	bool __reserved9 : 1;
	bool __reserved10 : 1;
	uint16_t __unknown1;
	uint16_t crtime, crdate;			/* creation date and time */
	uint16_t mtime, mdate;			/* latest modification date and time */
	uint16_t atime, adate;			/* latest access date and time */
	uint8_t crtime_cs;				/* creation time in cs (centiseconds) */
	uint8_t mtime_cs;				/* latest modification time in cs */
	uint8_t atime_cs;				/* latest access time in cs */
	uint8_t __unknown2[9];
};

#define EXFAT_FLAG_ALWAYS1		(1u << 0)
#define EXFAT_FLAG_CONTIGUOUS	(1u << 1)

struct exfat_entry_meta2			/* file or directory info (part 2) */
{
	uint8_t type;					/* EXFAT_ENTRY_FILE_INFO */
	uint8_t flags;					/* combination of EXFAT_FLAG_xxx */
	uint8_t __unknown1;
	uint8_t name_length;
	uint16_t name_hash;
	uint16_t __unknown2;
	uint64_t valid_size;				/* in bytes, less or equal to size */
	uint8_t __unknown3[4];
	uint32_t start_cluster;
	uint64_t size;					/* in bytes */
};

struct exfat_entry_name				/* file or directory name */
{
	uint8_t type;					/* EXFAT_ENTRY_FILE_NAME */
	uint8_t __unknown;
	wchar_t name[EXFAT_ENAME_MAX];	/* in UTF-16LE */
};


struct fat32_table {
	uint8_t magic[3]; //3
	uint8_t oem_name[8]; //8
	uint16_t sector_bytes; //2
	uint8_t sector_per_cluster; //1
	uint16_t reserved_sectors; //2
	uint8_t fat_count; //1
	uint16_t directory_entries; //2
	uint16_t total_sectors; //2
	uint8_t media; //1
	uint16_t _fat_size_in_sectors; //2
	uint16_t sectors_per_track; //2
	uint16_t num_heads; //2
	uint32_t hidden_sectors; //4
	uint32_t sectors_on_volume; //4
	uint32_t fat_size_in_sectors; //4
	uint16_t ext_flags; //2
	uint16_t fat_version; //2 
	uint32_t root_cluster; //4
	uint16_t fs_info; //2
	uint16_t backup_boot_sector; //2
	uint8_t __unused1[12]; //12
	uint8_t drive_number; //1
	uint8_t win_nt_flags; //1
	uint8_t boot_sig; //1
	uint32_t volume_id; //4
	char volume_label[11]; //11
	char file_system_time[8]; //8
	uint8_t boot_image[420]; //420
	uint16_t boot_signature; //2
};

#define FAT32_SECTOR(x, y) (((cluster - 2) * x.sectors_per_cluster) + first_data_sector)

#define FAT32_FLAG_READ_OLNY 0x01
#define FAT32_FLAG_HIDDEN 0x02
#define FAT32_FLAG_SYSTEM 0x04
#define FAT32_FLAG_VOLUME_ID 0x08
#define FAT32_FLAG_DIRECTORY 0x10
#define FAT32_FLAG_ARCHIVE 0x20
#define FAT32_FLAG_LFN (FAT32_FLAG_READ_OLNY | FAT32_FLAG_HIDDEN | FAT32_FLAG_SYSTEM | FAT32_FLAG_VOLUME_ID)

struct fat32_file {
	char file_name[11];
	uint8_t flags;
	uint8_t nt_reserved;
	uint8_t creating_time_ms;
	uint16_t creating_time;
	uint16_t creating_date;
	uint16_t last_access_date;
	uint16_t hi_first_cluster;
	uint16_t last_mod_time;
	uint16_t last_mod_date;
	uint16_t lo_first_cluster;
	uint32_t file_size;
};

struct fat32_long_directory {
	uint8_t order;
	wchar_t file_name[5];
	uint8_t flags;
	uint8_t dir_type;
	uint8_t chksum;
	wchar_t file_name2[6];
	uint16_t first_cluster;
	wchar_t file_name3[2];
};

#pragma pack(pop)

// TODO: ���������� ����� ������ �� �������������� ���������, ����������� ��� ���������
