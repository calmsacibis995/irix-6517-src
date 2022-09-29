/*
 * fat.h -- fat boot record information 
 */

#ifndef _ARCS_FAT_H
#define _ARCS_FAT_H

/*
 * Format for  boot record information
 *
 * The master boot record is a block located at the beginning of all disk
 * media.  It contains information pertaining to partition table which 
 * contains information about the fix disk's partitions and master 
 * bootstrap loader code which starts the operating system's boot code.
 *
 * A copy of the master boot record is located on sector 1 of track 0 
 * of head 0.  The master boot record is constrained to be 512
 * bytes long. A particular copy is assumed valid if no drive errors
 * are detected and the signature word is correct.
 *
 * Boot record/sector is located on the first sector(sector 1 of track 0
 * of head 0) of an active partition of a fixed disk. It contains 
 * information regarding the disk's characteristics and bootstrap code 
 * required to looad the operating system.
 */

#include <arcs/types.h>

typedef ULONG WORD;			/* 32 bit WORD			      */
typedef USHORT HWORD;			/* 16 bit Half Word		      */
typedef UCHAR BYTE;			/*  8 bit Byte			      */
typedef unsigned int	ADDRESS;	/* Port address 		      */

	/*
	 * Structure of Master Boot Record found at sector 1,
	 * cylinder 0, head 0.
	 */

struct	partition_type {
	BYTE	boot_indicator;	     /* 0=no partition, 80h=active partition */
	BYTE	starting_head;	     /* Not used. Too small for large disks  */
	BYTE	starting_sector;     /* Not used. Too small for large disks  */
	BYTE	starting_cylinder;   /* Not used. Too small for large disks  */
	BYTE	system_indicator;    /* operating system type */
	BYTE	end_head;	     /* Not used. Too small for large disks  */
	BYTE	end_sector;	     /* Not used. Too small for large disks  */
	BYTE	end_cylinder;	     /* Not used. Too small for large disks  */
	BYTE	rel_sectors_byt0;
	BYTE	rel_sectors_byt1;
	BYTE	rel_sectors_byt2;
	BYTE	rel_sectors_byt3;
	BYTE	tot_sectors_byt0;
	BYTE	tot_sectors_byt1;
	BYTE	tot_sectors_byt2;
	BYTE	tot_sectors_byt3;
};

#define BOOT_SIGNATURE	0xAA55
#define MAX_FAT_PAR     4       /* 4 FAT partitions                     */

struct master_boot_rec_type {
	BYTE	filler[0x1BE];
	struct	partition_type partition[MAX_FAT_PAR];
	HWORD	 signature;
};


struct bios_param_block_type {
	HWORD	bytes_per_sector;
	BYTE	sectors_per_cluster;
	HWORD	reserved_sectors;
	BYTE	num_fats;
	HWORD	num_rootdir_entries;
	HWORD	sects_in_volume;
	BYTE	media_desc_byte;
	HWORD	sects_per_fat;
	HWORD	sects_per_track;
	HWORD	num_heads;
	HWORD	num_hidden_sectors;
};

struct	boot_sector_type {
	BYTE	jmp[3];
	BYTE	oem_name[8];
	struct	bios_param_block_type bpb;
};

#endif
