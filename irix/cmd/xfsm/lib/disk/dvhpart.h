/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"dvhpart.h: $Revision: 1.3 $"

/*
 * dvhpart.h -- definitions for modifying disk partitions
 */

#define	PART_MAX_DISKS	100

#define	PT_BLK_SIZE	0
#define	PT_KB_SIZE 	1
#define	PT_MB_SIZE 	2
#define	PT_GB_SIZE 	3

#define	VOLHDR_PART_NO	8
#define	VOL_PART_NO	10

#define PARSER_PT_COUNT		0
#define	PARSER_PT_SIZE		1
#define	PARSER_PT_START		2
#define	PARSER_PT_END		3
#define	PARSER_PT_TYPE		4
#define	PARSER_DISK_NAME	5

#define NKEYS   (sizeof(keytab) / sizeof(keytab[0]))

#define N_PT_TYPE_TAB   (sizeof(pt_type_tab) / sizeof(pt_type_tab[0]))

#define gettxt(str_line_num,str_default)        str_default


typedef struct partition_table_entry_s {
	float	pt_size;
	int	pt_size_type;
	short	pt_no;
	int	start_blk;
	int	last_blk;
	int	type;
	struct partition_table_entry_s *next;
} partition_table_entry_t;

typedef struct partition_info_s {
	char	*disk_name;
	partition_table_entry_t	*partition_data;
	struct partition_info_s *next;
} partition_info_t;	

extern char *findrawpath(char*);
extern int errno, sys_nerr;
extern char* get_partition_info(char* buffer,char** msg);
extern int update_partition_table(char* buffer,char** msg);
extern partition_info_t* list_partitions(int devc, char* dev_list[],char** msg);

/*
 *	From libdisk.a
 */
extern int vhchksum(struct volume_header* vh);
