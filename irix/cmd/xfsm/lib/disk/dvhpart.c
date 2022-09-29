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

#ident	"dvhpart.c: $Revision: 1.11 $"

/*
 * dvhpart.c -- tool for modifying disk partitions
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/debug.h>
#include <sys/signal.h>
#include <sys/dkio.h>
#include <sys/stat.h>
#include <mountinfo.h>
#include <errno.h>
#include <ctype.h>
#include <stdarg.h>
#include <locale.h>
#include <pfmt.h>

#include "xfs_utils.h"
#include "dvhpart.h"
#include "xfs_info_defs.h"

struct pt_keyword_list {
	char* word;
	int   value;
} keytab[] = {
	"diskname",	PARSER_DISK_NAME,
	"partitions",	PARSER_PT_COUNT,
	"size",		PARSER_PT_SIZE,
	"start",	PARSER_PT_START,
	"end",		PARSER_PT_END,
	"type",		PARSER_PT_TYPE
};

struct pt_type_list {
	int partition_no;
	char* partition_type_str;
} pt_type_tab[] = {
	PTYPE_VOLHDR,	"volhdr",
	-1,	"",
	-1,	"",
	PTYPE_RAW,	"raw",
	-1,	"",
	-1,	"",
	-1,	"",
	PTYPE_VOLUME,	"volume",
	PTYPE_EFS,	"efs",
	PTYPE_LVOL,	"lvol",
	PTYPE_RLVOL,	"rlvol",
	PTYPE_XFS,	"xfs",
	PTYPE_XFSLOG,	"xfslog",
	PTYPE_XLV,	"xlv"
};
 
/*
 *	The routine builds the list of partitions belonging to the disks
 *	specified in the dev_list. 
 */

partition_info_t *
list_partitions(int devc, char *dev_list[], char **msg)
{
	partition_info_t	*pinfo_table = NULL;
	partition_info_t	*pinfo_table_entry = NULL;
	partition_table_entry_t	*ptable = NULL;
	partition_table_entry_t	*pt_entry = NULL;
	struct volume_header	vh;
	int			dev_index = 0;
	char			*disk_addr;
	int			errValue = 0;
	struct stat		stat_buf;
	char			*rawname = NULL;
	int 			vfd = -1;
	int			pt_index;
	short			pt_volhdr_flag;
	char			str[BUFSIZ];

	memset((void *)&vh, 0, sizeof(vh));
	while (dev_index < devc) {
		disk_addr = dev_list[dev_index];
		if (stat(disk_addr, &stat_buf) < 0) {
			sprintf(str, gettxt(":1",
					"%s: Cannot access disk.\n%s\n"),
					disk_addr, strerror(errno));
			add_to_buffer(msg,str);
			errValue = -1;
			break;
		}
		else {
			switch(stat_buf.st_mode & S_IFMT) {
				case S_IFCHR:
					rawname = disk_addr;
					break;

				case S_IFBLK:
					rawname = findrawpath(disk_addr);
					if (!rawname) {
						sprintf(str, gettxt(":3",
						"%s: Cannot find equivalent char device.\n"),
							disk_addr);
						add_to_buffer(msg, str);
						errValue = -1;
					}
					break;

				default:
					sprintf(str, gettxt(":4",
						"%s: Not a device file.\n"),
						disk_addr);
					add_to_buffer(msg,str);
					errValue = -1;
					break;
			}

			if (errValue != -1) {
        			if((vfd = open(rawname, O_RDONLY)) < 0) {
			                sprintf(str, gettxt(":5",
					    "%s: Error opening volume.\n%s\n"),
						 rawname, strerror(errno));
					add_to_buffer(msg, str);
			                errValue = -1;
        			}
				else if (ioctl(vfd, DIOCGETVH, &vh) == -1) {
			                sprintf(str, gettxt(":6",
					    "%s: Error reading volume.\n%s\n"),
						rawname, strerror(errno));
					add_to_buffer(msg, str);
			                errValue = -1;
				}
				else if (vh.vh_magic != VHMAGIC) {
					sprintf(str, gettxt(":7",
					"%s: The magic number is incorrect.\n"),
					rawname);
					add_to_buffer(msg, str);
					errValue = -1;
				}
				else if (vhchksum(&vh)) {
					sprintf(str, gettxt(":8",
					"%s: The checksum of the volume header is incorrect.\n"),
					disk_addr);
					add_to_buffer(msg, str);
					errValue = -1;
				}
				else {
					/* Check for volume hdr partition*/
					pt_index = 0;
					pt_volhdr_flag = -1;
					while (pt_index < NPARTAB) {
						if (vh.vh_pt[pt_index].pt_type == PTYPE_VOLHDR)
						{
							pt_volhdr_flag = 0;
							break;
						}
						pt_index++;
					}

					if (pt_volhdr_flag == -1) {
						sprintf(str, gettxt(":9",
						"%s: Missing volume header partition.\n"),
						disk_addr);
						add_to_buffer(msg, str);
						errValue = -1;
					}
				}

				close(vfd);
			}

			if (errValue != -1)
			{
				/* Save the partition information */
				
				/* Print volume header info */
				for (pt_index=0;pt_index<NPARTAB;pt_index++)
				{
					if (vh.vh_pt[pt_index].pt_nblks)
					{
						pt_entry = (partition_table_entry_t*)
							malloc(sizeof(partition_table_entry_t));
						ASSERT(pt_entry!=NULL);

						pt_entry->pt_size = (float)vh.vh_pt[pt_index].pt_nblks;
						pt_entry->pt_no = pt_index;
						pt_entry->start_blk = vh.vh_pt[pt_index].pt_firstlbn;
						pt_entry->last_blk = vh.vh_pt[pt_index].pt_firstlbn +
									vh.vh_pt[pt_index].pt_nblks;
						pt_entry->type = vh.vh_pt[pt_index].pt_type;

						pt_entry->next = ptable;
						ptable = pt_entry;
					}
				}

				pinfo_table_entry = (partition_info_t*) malloc(sizeof(partition_info_t));
				ASSERT(pinfo_table_entry!=NULL);
				pinfo_table_entry->partition_data = ptable;
				pinfo_table_entry->disk_name = disk_addr;

				pinfo_table_entry->next = pinfo_table;
				pinfo_table = pinfo_table_entry;

				ptable = NULL;
			}
		}

		dev_index++;
	}
	return(pinfo_table);
}
		
/*
 *	save_partition_info()
 *	The partition table is written to the specified disk.
 *	Returns 0 if successful and -1 in case of error.
 */

int
save_partition_info(partition_info_t *pinfo,char** msg)
{
	struct volume_header	vh;
	partition_table_entry_t	*pt_entry = NULL;
	struct stat		stat_buf;

	int	errValue = 0;
	char	*rawname = NULL;
	int 	vfd = -1;
	int	pt_index;
	int	pt_nblks;
	short	pt_volhdr_flag;
	char 	*disk_addr = NULL;
	int	dev_blk_size;
	int	major_device_no;
	int	minor_device_no;
	char	device_node_name[BUFSIZ];
	char	cmd_string[BUFSIZ];
	char 	*device_node_name_ptr = NULL;
	int	vol_hdr_pt_no;
	FILE	*pfd = NULL;
	char	str[BUFSIZ];
	
	if (pinfo == NULL) {
		return(-1);
	}

	disk_addr = pinfo->disk_name;
	if (stat(disk_addr, &stat_buf) < 0) {
		sprintf(str, gettxt(":1",
				"%s: Cannot access disk.\n%s\n"),
				disk_addr, strerror(errno));
		add_to_buffer(msg, str);
		return(-1);
	}

	switch(stat_buf.st_mode & S_IFMT) {
		case	S_IFCHR:
			rawname = disk_addr;
			break;

		case	S_IFBLK:
			rawname = findrawpath(disk_addr);
			if (!rawname) {
				sprintf(str, gettxt(":3",
				"%s: Cannot find equivalent char device.\n"),
				disk_addr);
				add_to_buffer(msg, str);
				errValue = -1;
			}
			break;

		default:
			sprintf(str,gettxt(":4",
					"%s: Not a device file.\n"),
					disk_addr);
			add_to_buffer(msg,str);
			errValue = -1;
			break;
	}

	if (errValue != -1) {
		/* Get the major & minor device numbers
		   of the disk */
		major_device_no = major(stat_buf.st_rdev);
		minor_device_no = minor(stat_buf.st_rdev);
		
		if((vfd = open(rawname, O_RDWR)) < 0) {
			sprintf(str, gettxt(":5",
					"%s: Error opening volume.\n%s\n"),
					 rawname, strerror(errno));
			add_to_buffer(msg, str);
			errValue = -1;
		}
		else if (ioctl(vfd, DIOCGETVH, &vh) == -1) {
			sprintf(str, gettxt(":6",
			    "%s: Error reading volume.\n%s\n"),
				rawname, strerror(errno));
			add_to_buffer(msg, str);
			errValue = -1;
		}
		else if (vh.vh_magic != VHMAGIC) {
			sprintf(str, gettxt(":7",
					"%s: The magic number is incorrect.\n"),
					rawname);
			add_to_buffer(msg, str);
			errValue = -1;
		}
		else if (vhchksum(&vh)) {
			sprintf(str, gettxt(":8",
			"%s: The checksum of the volume header is incorrect.\n"),
					disk_addr);
			add_to_buffer(msg, str);
			errValue = -1;
		}
		else {
			/* Check for volume hdr partition*/
			pt_index = 0;
			pt_volhdr_flag = -1;
			while (pt_index < NPARTAB) {
				if(vh.vh_pt[pt_index].pt_nblks != 0 &&
				   vh.vh_pt[pt_index].pt_type == PTYPE_VOLHDR) {
					/* Save the partition no of
					 * volume header partition
					 */
					vol_hdr_pt_no = pt_index;
					pt_volhdr_flag = 0;
					break;
				}
				pt_index++;
			}

			if (pt_volhdr_flag == -1) {
				sprintf(str, gettxt(":9",
				    "%s: Missing volume header partition.\n"),
				    disk_addr);
				add_to_buffer(msg, str);
				errValue = -1;
			}
		}
	}

	if (errValue != -1) {
		if((dev_blk_size = vh.vh_dp.dp_secbytes) == 0) {
			dev_blk_size = NBPSCTR;
		}

		/* Save the partition information */
		
		/* Initialize all partitions to 0 size */
		for (pt_index=0;pt_index<NPARTAB;pt_index++) {
			vh.vh_pt[pt_index].pt_nblks=0;
		}

		pt_entry = pinfo->partition_data;
		while (pt_entry != NULL) {
			/* The size of the partition maybe
			 * in KB/MB/GB/Blocks/Cylinders.
			 * Conversion to blocks is required
			 */
			switch(pt_entry->pt_size_type) {
				case	PT_KB_SIZE:
					pt_nblks = (int)
						pt_entry->pt_size*((1<<10) /
						dev_blk_size);
					break;

				case	PT_MB_SIZE:
					pt_nblks = (int)
						pt_entry->pt_size*((1<<20) /
						dev_blk_size);
					break;

				case	PT_GB_SIZE:
					pt_nblks = (int)
						pt_entry->pt_size*((1<<30) /
						dev_blk_size);
					break;

				case	PT_BLK_SIZE:
					pt_nblks = (int) pt_entry->pt_size;
					break;
			}

			vh.vh_pt[pt_entry->pt_no].pt_nblks = pt_nblks; 
			vh.vh_pt[pt_entry->pt_no].pt_firstlbn =
					 pt_entry->start_blk;
			vh.vh_pt[pt_entry->pt_no].pt_type = pt_entry->type;

			/* Create the nodes in the /dev/dsk and /dev/rdsk
			 * directories for this partition
			 */
			/* Transform the disk name to
			 * refer to the node to be created
			 */
			device_node_name_ptr = strstr(disk_addr,"vh");
			if (device_node_name_ptr != NULL) {
				memset(device_node_name,'\0',BUFSIZ);
				strncpy(device_node_name,disk_addr,
					(device_node_name_ptr-disk_addr));
				sprintf(device_node_name,"%ss%d",
					device_node_name, pt_entry->pt_no);

				strtok(device_node_name,"/");
				strtok(NULL,"/");
				device_node_name_ptr = strtok(NULL,"/");
			}
		
			/* Invoke mknod to create the nodes*/
			sprintf(cmd_string,
			    "/bin/sh -c '/sbin/mknod /dev/dsk/%s b %d %d 2>&1'",
			    device_node_name_ptr,
			    major_device_no,
			    (minor_device_no-vol_hdr_pt_no) + pt_entry->pt_no);

			pfd = xfs_popen(cmd_string, "r", 0);
			xfs_pclose(pfd);

			sprintf(cmd_string,
			    "/bin/sh -c '/sbin/mknod /dev/rdsk/%s c %d %d 2>&1'",
			    device_node_name_ptr,
			    major_device_no,
			    (minor_device_no-vol_hdr_pt_no) + pt_entry->pt_no);

			pfd = xfs_popen(cmd_string, "r", 0);
			xfs_pclose(pfd);

			pt_entry = pt_entry->next;
		}

		/* Calculate the checksum for vol header*/
		vh.vh_csum = 0;
		vh.vh_csum = -vhchksum(&vh);

		if (lseek(vfd, 0, 0) < 0) {
			sprintf(str, gettxt(":10",
					"%s: Seek to sector 0 failed.\n"),
					pinfo->disk_name);
			add_to_buffer(msg, str);
			errValue = -1;
		}
		else if (write(vfd, &vh, dev_blk_size) != dev_blk_size) {
			sprintf(str, gettxt(":11",
				"%s: Unable to write volume header.\n%s\n"),
					pinfo->disk_name,
					strerror(errno));
			add_to_buffer(msg, str);
			errValue = -1;
		}	
		else {
			/* Inform the driver */
			if (ioctl(vfd, DIOCSETVH, &vh) < 0) {
				sprintf(str, gettxt(":11",
				"%s: Unable to write volume header.\n%s\n"),
					pinfo->disk_name,
					strerror(errno));
				add_to_buffer(msg, str);
				errValue = -1;
			}
		}
	}

	if(vfd != -1) {
		close(vfd);
	}

	return(errValue);
}


/*
 *	delete_partition_entry()-routine to delete an element from the table
 */

void
delete_partition_entry(partition_table_entry_t **table, 
partition_table_entry_t *elm)
{
	partition_table_entry_t *prev_elm = NULL;
	partition_table_entry_t *table_entry=NULL;

	table_entry = *table;
	while (table_entry != NULL)
	{
		if (table_entry == elm)
		{
			if (prev_elm == NULL)
			{
				*table = table_entry->next;
			}
			else
			{
				prev_elm->next = table_entry->next;
			}
			table_entry->next = NULL;	
			break;
		}

		prev_elm = table_entry;
		table_entry = table_entry->next;
	}
}
				
	
/*
 *	Routine to order the partition table entries by sorting on
 *	the starting block number of each partition. This will
 *	enable easy detection of holes and overlaps.
 */

partition_table_entry_t*
sort_partition_table(partition_table_entry_t **ptable)
{
	partition_table_entry_t *pt_entry=NULL;
	partition_table_entry_t *best_entry=NULL;
	partition_table_entry_t *sorted_ptable=NULL;

	/* The sorting is performed by finding the partition
	   with the largest starting block no & propogating it
	   to the end of a list (sorted_ptable) */

	while (*ptable != NULL)
	{
		best_entry = *ptable;
		pt_entry = (*ptable)->next;

		while (pt_entry != NULL)
		{
			if (pt_entry->start_blk > best_entry->start_blk)
			{
				best_entry = pt_entry;
			}
			else if (pt_entry->start_blk == best_entry->start_blk)
			{
				if (pt_entry->last_blk > best_entry->last_blk)
				{
					best_entry = pt_entry;
				}
			}

			pt_entry = pt_entry->next;
		}

		if (best_entry != NULL)
		{
			/* Delete it from the ptable */
			delete_partition_entry(ptable,best_entry);
	
			/* Add the best entry to sorted list */
			if (sorted_ptable==NULL)
			{
				sorted_ptable = best_entry;
				sorted_ptable->next = NULL;
			}
			else
			{
				best_entry->next = sorted_ptable;
				sorted_ptable = best_entry;
			}
		}
	}
	return(sorted_ptable);
}		
		
#ifdef	VERIFY_LAYOUT

/*
 *	add_to_allocation_list()
 */
static void 
add_to_allocation_list(
	partition_table_entry_t **alloc_list,
	partition_table_entry_t *partition)
{
	partition_table_entry_t *pt_entry=NULL;
	int new_alloc_range = 0;

	pt_entry = *alloc_list;
	while (pt_entry != NULL)
	{
		if (partition->start_blk == pt_entry->start_blk)
		{
			if (pt_entry->last_blk < partition->last_blk)
			{
				new_alloc_range = -1;
				pt_entry->last_blk = partition->last_blk;
				pt_entry->pt_no = partition->pt_no;
				partition = pt_entry;
			}
			else
			{
				new_alloc_range = -1;
			}
		}
		else if (partition->start_blk > pt_entry->start_blk)
		{
			if (partition->start_blk > pt_entry->last_blk)
			{
				new_alloc_range = 0;
			}
			else if (partition->last_blk > pt_entry->last_blk)
			{
				new_alloc_range = -1;
				pt_entry->last_blk = partition->last_blk;
				pt_entry->pt_no = partition->pt_no;
				partition = pt_entry;
			}
			else
			{
				new_alloc_range = -1;
			}
		}
		else
		{
			if (partition->last_blk > pt_entry->start_blk)
			{
				new_alloc_range = 0;
			}
			else if (partition->last_blk > pt_entry->last_blk)
			{
				new_alloc_range = -1;
				pt_entry->last_blk = partition->last_blk;
				pt_entry->pt_no = partition->pt_no;
				partition = pt_entry;
			}
			else
			{
				new_alloc_range = -1;
			}
		}				

		pt_entry = pt_entry->next;
	}

	if (new_alloc_range == 0)
	{
		pt_entry = (partition_table_entry_t*) malloc(sizeof(partition_table_entry_t));
		ASSERT(pt_entry!=NULL);
		pt_entry->start_blk = partition->start_blk;
		pt_entry->last_blk = partition->last_blk;
		pt_entry->pt_no = partition->pt_no;

		pt_entry->next = *alloc_list;
		*alloc_list = pt_entry;

		*alloc_list = sort_partition_table(alloc_list);
	}

	/* Print the allocation list for debug purposes */
#ifdef	DEBUG
	printf("Allocation list \n");
	pt_entry = *alloc_list;
	while (pt_entry != NULL)
	{
		printf("Block %d (%d-%d)\n", pt_entry->pt_no,
			pt_entry->start_blk,pt_entry->last_blk);

		pt_entry = pt_entry->next;
	}
#endif	/* DEBUG */
}
		

/*
 *	verify_partition_layout()
 *	By traversing the sorted list of partition table entries, the 
 *	holes and overlaps in partitions are located. If there are 
 *	overlapping partitions then a value of 1 is returned or else
 *	0 is returned.
 */

int
verify_partition_layout(partition_table_entry_t *ptable)
{
	partition_table_entry_t *pt_entry=NULL;
	partition_table_entry_t *vol_entry=NULL;
	partition_table_entry_t *pt_next_entry=NULL;
	partition_table_entry_t *pt_last_block=NULL;
	partition_table_entry_t *alloc_list=NULL;
	int	overlap_flag = 0;

	pt_entry = ptable;
	while (pt_entry != NULL)
	{
		if (pt_entry->type != PTYPE_VOLUME)
		{
			add_to_allocation_list(&alloc_list,pt_entry);
		}
		pt_entry = pt_entry->next;
	}

	pt_entry = ptable;
	while (pt_entry != NULL)
	{
		if (pt_entry->type == PTYPE_VOLUME)
		{
			vol_entry = pt_entry;
		}
		else if (pt_entry->next != NULL)
		{
			if (pt_entry->next->type == PTYPE_VOLUME)
			{
				if (pt_entry->next->next != NULL)
					pt_next_entry=pt_entry->next->next;
				else
					break;
			}
			else
			{
				pt_next_entry = pt_entry->next;
			}

			/* Check for any overlaps */
			while (pt_next_entry != NULL)
			{
			if (pt_next_entry->type == PTYPE_VOLUME)
			{
				if (pt_next_entry->next != NULL)
					pt_next_entry=pt_next_entry->next;
				else
					break;
			}

			if (pt_entry->last_blk>pt_next_entry->start_blk)
			{
				if ((pt_entry->last_blk
					-pt_next_entry->start_blk) < 
					(pt_next_entry->last_blk - 
					pt_next_entry->start_blk))
				{
					overlap_flag = 1;
					printf("Overlap %9d blocks in partitions %d-%d\n",
						(pt_entry->last_blk-
						pt_next_entry->start_blk),
						pt_entry->pt_no,
						pt_next_entry->pt_no);
				}
				else
				{
					overlap_flag = 1;
					printf("Overlap %9d blocks in partitions %d-%d\n",
						(pt_next_entry->last_blk-
						pt_next_entry->start_blk),
						pt_entry->pt_no,
						pt_next_entry->pt_no);
				}	
			}

			pt_next_entry = pt_next_entry->next;
			}
		}

		pt_entry = pt_entry->next;
	}

/*
	printf("\n");
*/

	/* Check for holes at the beginning of volume */
	pt_entry = alloc_list;
	if (pt_entry == vol_entry)
	{
		pt_entry = pt_entry->next;
	}
	if (pt_entry != NULL)
	{
		if ((pt_entry->start_blk-vol_entry->start_blk) > 0)
		{
/*
			printf("Hole %12d blocks at the beginning of partition %d\n",
				(pt_entry->start_blk-vol_entry->start_blk),
					pt_entry->pt_no);
*/
		}
		pt_next_entry = pt_entry->next;
	}

	while ((pt_entry != NULL) && (pt_next_entry != NULL))
	{
		if (pt_entry->last_blk < pt_next_entry->start_blk)
		{
/*
			printf("Hole %12d blocks between partitions %d-%d\n",
			(pt_next_entry->start_blk-pt_entry->last_blk),
				pt_entry->pt_no,
				pt_next_entry->pt_no);
*/
		}

		pt_entry = pt_next_entry;
		pt_next_entry = pt_next_entry->next;
	}

	/* Check for holes at the end of volume */
	pt_last_block = NULL;
	if (alloc_list != NULL)
	{
		if (alloc_list->type == PTYPE_VOLUME)
			pt_last_block = alloc_list->next;
		else
			pt_last_block = alloc_list;
		pt_entry = alloc_list->next;
	}

	/* locate the last partition */
	while (pt_entry != NULL)
	{
		if ((pt_entry->type != PTYPE_VOLUME) &&
			(pt_entry->last_blk > pt_last_block->last_blk))
		{	
			pt_last_block = pt_entry;
		}
		pt_entry = pt_entry->next;
	}

	if (pt_last_block != NULL)
	{
		if ((vol_entry->last_blk-pt_last_block->last_blk) > 0)
		{
/*
			printf("Hole %12d blocks at the end of partition %d\n",
			(vol_entry->last_blk-pt_last_block->last_blk),
					pt_last_block->pt_no);
*/
		}					
	}		

	/* Release memory used for allocation list */
	pt_entry = alloc_list;
	while (pt_entry != NULL)
	{
		alloc_list = pt_entry->next;
		free(pt_entry);
		pt_entry = alloc_list;
	}

	return(overlap_flag);	
}	
#endif	/* VERIFY_LAYOUT */


/*
 *	lookup_pt_keyword()
 *	The list of known keywords is traversed to find the given entry.
 */

int
lookup_pt_keyword(char* keyword)
{
	int count=0;

	while (count < NKEYS)
	{
		if (strcmp(keytab[count].word,keyword)==0)
			return(keytab[count].value);
		count++;
	}
	return(-1);
}

/*
 *	lookup_pt_type_str()
 *	Given a string identifying a partition, it returns the corresponding
 *	partition number by looking up the list of known partition_types,
 *	pt_type_tab.
 *	If partition type not found, -1 is returned.
 */

int
lookup_pt_type_str(char* pt_type_str)
{
	int count = 0;
	while (count < N_PT_TYPE_TAB)
	{
		if (strcmp(pt_type_tab[count].partition_type_str,
			pt_type_str)==0)
		{
			return(pt_type_tab[count].partition_no);
		}
		count++;
	}
	return(-1);
}

/*
 *	lookup_pt_type()
 *	Given a partition number, it returns the corresponding
 *	string identifying the partition type by looking up the 
 *	list of known partition_types, pt_type_tab.
 *	If partition number not found, NULL is returned.
 */

char*
lookup_pt_type(int pt_type)
{
	int count = 0;
	while (count < N_PT_TYPE_TAB)
	{
		if (pt_type_tab[count].partition_no == pt_type)
		{
			return(pt_type_tab[count].partition_type_str);
		}
		count++;
	}
	return(NULL);
}

/*
 *	get_pt_entry()
 *	Return the partition table entry corresponding to the partition 
 *	number. If the partition does not exist create a new entry.
 */

partition_table_entry_t*
get_pt_entry(partition_info_t **ptable,int partition_no)
{
	partition_table_entry_t *pt_entry=NULL;

	pt_entry = (*ptable)->partition_data;
	while (pt_entry != NULL)
	{
		if (pt_entry->pt_no == partition_no)
			break;

		pt_entry = pt_entry->next;
	}

	if (pt_entry == NULL)
	{
		/* Create a new entry for the partition */
		pt_entry = (partition_table_entry_t*) malloc(
					sizeof(partition_table_entry_t));
		ASSERT(pt_entry!=NULL);
		pt_entry->pt_no = partition_no;
		pt_entry->next = (*ptable)->partition_data;
		(*ptable)->partition_data = pt_entry;
	}

	return(pt_entry);
}

/*
 *	update_ptable_disk_name()
 *	Update the disk name of the partition table being constructed.
 */

void
update_ptable_disk_name(partition_info_t *ptable,char* disk_name)
{
	ptable->disk_name = (char*) malloc(strlen(disk_name)+1);
	ASSERT(ptable->disk_name!=NULL);
	strncpy(ptable->disk_name,disk_name,strlen(disk_name)+1);
}


/*
 *	parse_line()
 *	Retrieves the keyword, value pair from the line and updates
 *	the partition table.
 */

void
parse_line(partition_info_t* partition_info,char* line)
{
	char 	str[BUFSIZ];
	char* 	keyword=NULL;
	char*	token=NULL;
	char*	disk_name=NULL;
	int 	partition_no;
	float	partition_size;
	int	partition_start;
	int	partition_end;
	int	partition_type;
	partition_table_entry_t* pt_entry=NULL;

	/* Make a copy of the line to parse internally */
	strncpy(str,line,BUFSIZ);
	
	/* Get the keyword */
	keyword = strtok(str,".:");
	if (keyword!=NULL)
	{
		switch(lookup_pt_keyword(keyword))
		{
			case PARSER_PT_COUNT	: /* Ignore it for now */
				break;

			case PARSER_DISK_NAME	:
				/* Fetch the name of the disk */
				if ((token=strtok(NULL,":"))!=NULL)
				{
					disk_name = token;
					update_ptable_disk_name(
						partition_info,
						disk_name);
				}
				break;

			case PARSER_PT_SIZE	:
				/* Fetch the partition no & size */
				if ((token=strtok(NULL,":"))!=NULL)
				{
					partition_no = atoi(token);

					if ((token=strtok(NULL,": "))
						!=NULL)
					{
						pt_entry = get_pt_entry(
							&partition_info,
							partition_no);
						if (pt_entry!=NULL)
						  pt_entry->pt_size=atof(token);

						/* Check how the size is
						   specified in MB/GB/Cyl ?
						   Default is blocks */
						pt_entry->pt_size_type =
							PT_BLK_SIZE;
						if ((token=strtok(NULL,": "))
							!=NULL)
						{
							if (strcmp(token,"KB")
								==0)
							 pt_entry->pt_size_type=
								 PT_KB_SIZE;
							else if (strcmp(
								token,"MB")==0)
							 pt_entry->pt_size_type=
								 PT_MB_SIZE;
							else if (strcmp(
								token,"GB")==0)
							 pt_entry->pt_size_type=
								 PT_GB_SIZE;
							else if (strcmp(
								token,"BLK")==0)
							 pt_entry->pt_size_type=
								 PT_BLK_SIZE;
						}
					}
				}
				break;

			case PARSER_PT_START	:
				/* Fetch the partition starting blk#*/
				if ((token=strtok(NULL,":"))!=NULL)
				{
					partition_no = atoi(token);

					if ((token=strtok(NULL,": "))
						!=NULL)
					{
						pt_entry = get_pt_entry(
							&partition_info,
							partition_no);
						if (pt_entry!=NULL)
						   pt_entry->start_blk=
						   (int) atoi(token);
					}
				}
				break;

			case PARSER_PT_END	:
				/* Fetch the partition ending blk#*/
				if ((token=strtok(NULL,":"))!=NULL)
				{
					partition_no = atoi(token);

					if ((token=strtok(NULL,": "))
						!=NULL)
					{
						pt_entry = get_pt_entry(
							&partition_info,
							partition_no);
						if (pt_entry!=NULL)
						   pt_entry->last_blk=
						   (int) atoi(token);
					}
				}
				break;

			case PARSER_PT_TYPE	:
				/* Fetch the partition type */
				if ((token=strtok(NULL,":"))!=NULL)
				{
					partition_no = atoi(token);

					if ((token=strtok(NULL,": "))
						!=NULL)
					{
						pt_entry = get_pt_entry(
							&partition_info,
							partition_no);
						if (pt_entry!=NULL)
						   pt_entry->type=
						   lookup_pt_type_str(token);
					}
				}
				break;
		}
	}
}


/*
 *	update_partition_table()
 *	Reads the character stream from the GUI and parses a line at
 *	a time buiding the structures for the partition information.
 *	Returns 0 if successful and -1 in case of error.
 */

int
update_partition_table(char* buffer,char** msg)
{
	char* line=NULL;/* Each line in the buffer corresponds to a
			   keyword value pair */
	char* ptr_next_line=NULL;
	partition_info_t partition_info = {NULL, NULL};
	partition_table_entry_t *pt_entry=NULL;
	partition_table_entry_t *prev_pt_entry=NULL;
	int return_value = 0;

	line = strtok(buffer,"\n");
	while (line != NULL)
	{
		ptr_next_line = (line + strlen(line) + 1);
		parse_line(&partition_info,line);
		line = strtok(ptr_next_line,"\n");
	}

	/* Save the partition data on disk */

	/* Remove empty partition entries in the partition table */
	prev_pt_entry = NULL;
	pt_entry = partition_info.partition_data;
	while (pt_entry != NULL)
	{
		if (((int)pt_entry->pt_size) == 0)
		{
			if (prev_pt_entry == NULL)
			{
				prev_pt_entry = 
				partition_info.partition_data =
						pt_entry->next;
				free(pt_entry);
			}
			else
			{
				prev_pt_entry->next=pt_entry->next;
				prev_pt_entry = pt_entry->next;
				free(pt_entry);
			}
		}
		else
		{
			/* Check partition 8 it must be PTYPE_VOLHDR
			   and partition 10 must be PTYPE_VOLUME */
			if (pt_entry->pt_no == VOLHDR_PART_NO) 
			{
				if (pt_entry->type != PTYPE_VOLHDR)
					return_value = -1;
			}
			else if (pt_entry->pt_no == VOL_PART_NO) 
			{
				if (pt_entry->type != PTYPE_VOLUME)
					return_value = -1;
			}
			prev_pt_entry = pt_entry;
		}

		/* Move to next partition table entry */
		if (prev_pt_entry != NULL)
		{
			pt_entry = prev_pt_entry->next;
		}
		else
		{
			pt_entry = NULL;
		}
	}
			
	/* Check for overlapping partitions */
	if (return_value == 0)
	{
		partition_info.partition_data = sort_partition_table(
				&(partition_info.partition_data));
		return_value = save_partition_info(&partition_info, msg);
#ifdef	VERIFY_LAYOUT
		if (verify_partition_layout(
				partition_info.partition_data)==0)
		{
			save_partition_info(&partition_info,msg);
		}
		else
		{
			return_value = -1;
		}
#endif	/* VERIFY_LAYOUT */
	}

	return(return_value);
}

/*
 *	dump_partition_info()
 *	Saves the partition information into the character stream as a
 *	series of keyword value pairs.
 */

void
dump_partition_info(partition_info_t* partition_info,char** buffer)
{
	char str[BUFSIZ];
	partition_table_entry_t* pt_entry=NULL;
	int partition_count = 0;

	if ((partition_info != NULL) && (buffer != NULL) 
		&& (*buffer !=NULL))
	{
		sprintf(*buffer,"diskname:%s\n",partition_info->disk_name);
		/* Dump the partition table entries one at a time */
		pt_entry = partition_info->partition_data;
		while (pt_entry != NULL)
		{
			sprintf(str,"size.%hd:%d\n",pt_entry->pt_no,
							(int)pt_entry->pt_size);
			strcat(*buffer,str);
			sprintf(str,"start.%hd:%d\n",pt_entry->pt_no,
							pt_entry->start_blk);
			strcat(*buffer,str);
			sprintf(str,"end.%hd:%d\n",pt_entry->pt_no,
							pt_entry->last_blk);
			strcat(*buffer,str);
			sprintf(str,"type.%hd:%s\n",pt_entry->pt_no,
						lookup_pt_type(pt_entry->type));
			strcat(*buffer,str);

			partition_count++;
			pt_entry = pt_entry->next;
		}

		sprintf(str,"partitions:%d\n",partition_count);
		strcat(*buffer,str);
	}
}


/*
 *	get_partition_info()
 *	The partition table for the given disk is retrieved 
 *	and dumped into a character stream for the GUI.
 */

char*
get_partition_info(char* buffer,char** msg)
{
	char* 	line=NULL;/* Each line in the buffer corresponds to a
			   keyword value pair */
	char* 	ptr_next_line=NULL;
	char 	str[BUFSIZ];
	char* 	keyword=NULL;
	char* 	token=NULL;
	char*	disk_name=NULL;
	partition_info_t *partition_info=NULL;
	char*	dev_list[1];
	char* 	output_buffer=NULL;
	
	/* Get the disk name */
	line = strtok(buffer,"\n");
	while (line != NULL)
	{
		/* Make a copy of the line to parse internally */
		strncpy(str,line,BUFSIZ);
	
		/* Get the keyword */
		keyword = strtok(str,".:");
		if ((keyword!=NULL) && 
		(lookup_pt_keyword(keyword)==PARSER_DISK_NAME)) 
		{
			if ((token=strtok(NULL,":"))!=NULL)
			{
				disk_name = token;
				break;
			}
		}
		ptr_next_line = (line + strlen(line) + 1);
		line = strtok(ptr_next_line,"\n");
	}

	/* Get the partition data from disk */
	if (disk_name != NULL)
	{
		dev_list[0] = disk_name;
		partition_info = list_partitions(1,dev_list,msg);
		if (partition_info != NULL)
		{
			output_buffer = (char*) malloc(BUFSIZ);
			ASSERT(output_buffer!=NULL);
			dump_partition_info(partition_info,&output_buffer);
/****			free(partition_info);		****/
		}	
	}

	return(output_buffer);
}	

int
xfsChkMountsInternal(const char *objectId, char **info, char **msg)
{
	mnt_check_state_t	*check_state;

	char	str[BUFSIZ];
	char	o_name [256];
	char	o_class[32];

	*msg = NULL; *info = NULL;
	
	if ((objectId == NULL) || (info == NULL) || (msg == NULL)) {
		sprintf(str, gettxt(":146",
			"Invalid parameters for xfsChkMountsInternal()\n"));
		add_to_buffer(msg, str);
		return 1;
	}

        if(xfsParseObjectSignature(DISK_TYPE, objectId, NULL, o_name,
                                        o_class, NULL, msg) == B_FALSE) {
		sprintf(str, gettxt(":146",
		"Invalid object signature passed to xfsChkMountsInternal()\n"));
		add_to_buffer(msg, str);
                return 1;
        }


	if (mnt_check_init(&check_state) == -1) {
		sprintf(str, gettxt(":146",
			"%s: mnt_check_init() failed\n"), o_name);
		add_to_buffer(msg, str);
		return 1;
	}

	if(mnt_find_mounted_partitions(check_state, o_name) > 0) {
		char	tname[L_tmpnam+1];
		FILE	*fp;

		if(tmpnam(tname) == NULL) {
			sprintf(str, gettxt(":146",
				"%s: conflicts exist.\n"), o_name);
			add_to_buffer(msg, str);
			sprintf(str, gettxt(":146",
				"Cannot generate conflict results.\n"));
			add_to_buffer(msg, str);
			return 1;
		}

		if((fp = fopen(tname, "a+")) == NULL) {
			sprintf(str, gettxt(":146",
				"%s: conflicts exist.\n"), o_name);
			add_to_buffer(msg, str);
			sprintf(str, gettxt(":146",
				"Cannot generate conflict results.\n"));
			add_to_buffer(msg, str);
			return 1;
		}

		mnt_causes_show(check_state, fp, "");
		mnt_plist_show(check_state, fp, "");

		fflush(fp);
		rewind(fp);
		
		while(fgets(str, sizeof(str), fp) != NULL) {
			add_to_buffer(info, str);
		}
		fclose(fp);
		unlink(tname);
	}
	mnt_check_end(check_state);

	return 0;
}
