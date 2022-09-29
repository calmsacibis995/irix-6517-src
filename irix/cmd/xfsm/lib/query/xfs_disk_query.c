/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "xfs_disk_query.c: $Revision: 1.4 $"

/*
 *	Queries on xfs disk attributes implementation file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bstring.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/syssgi.h>
#include <assert.h>

#include <sys/uuid.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <xlv_oref.h>
#include <xlv_utils.h>
#include <sys/xlv_vh.h>
#include <sys/xlv_attr.h>
#include <sys/dvh.h>
#include <xlv_lab.h>


#include "xfs_fs_defs.h"
#include "xfs_query.h"
#include "xfs_xlv_query.h"
#include "xfs_disk_query_defs.h"

/*
 *	add_disk_to_set()
 *	Adds a reference to the disk in the set.
 */

void
add_disk_to_set(query_set_t *element, query_set_t **set)
{
	query_set_t *new_element;

	new_element = (query_set_t*) malloc(sizeof(query_set_t));
	assert(new_element!=NULL);
	new_element->type = element->type;
	new_element->value = element->value; 
	new_element->next = *set;
	*set = new_element;
}

/*
 *	xfs_disk_query_eval()
 *	Evaluate the query for the given disk. 
 *	Return Value:	0 if condition is TRUE
 *			1 if condition is FALSE
 */

int
xfs_disk_query_eval(xlv_vh_entry_t* disk_info,short attr_id, char* value,
short oper)
{
	int result=1;
	int pt_exists_flag, pt_index;
	unsigned short value_ushort;
	int value_int;
	unsigned int	value_uint;
	

	switch(attr_id) {
		case DISK_DRIVECAP:
			sscanf(value, "%u", &value_uint);
			if(((oper == QUERY_EQUAL) &&
			    (disk_info->vh.vh_dp.dp_drivecap == value_uint)) ||
			   ((oper == QUERY_NE) &&
			    (disk_info->vh.vh_dp.dp_drivecap != value_uint))) {
				result = 0;
			}
				
			break;

		case DISK_SECTOR_LENGTH:
			sscanf(value,"%hu",&value_ushort);
			if ((oper == QUERY_EQUAL) &&
			(disk_info->vh.vh_dp.dp_secbytes==value_ushort))
			{
				result = 0;
			}
			else if ((oper == QUERY_NE) &&
			(disk_info->vh.vh_dp.dp_secbytes!=value_ushort))
			{
				result = 0;
			}
			break;

		case DISK_CTLR_FLAG:
			sscanf(value,"%d",&value_int);
			if ((oper == QUERY_EQUAL) &&
				(disk_info->vh.vh_dp.dp_flags==value_int))
			{
				result = 0;
			}
			else if ((oper == QUERY_NE) &&
				(disk_info->vh.vh_dp.dp_flags!=value_int))
			{
				result = 0;
			}
			break;

		case DISK_PARTITION_TYPE:

			sscanf(value,"%d",&value_int);
			/* Look for the disk which has the partition of 
			   the given type*/
			pt_exists_flag = 1;
			for (pt_index=0;pt_index<NPARTAB;pt_index++)
			{
				if ((disk_info->vh.vh_pt[pt_index].pt_nblks)
		 && (disk_info->vh.vh_pt[pt_index].pt_type==value_int)) 
				{
					pt_exists_flag = 0;
				}
			}

			if ((oper == QUERY_EQUAL) && (pt_exists_flag))
			{
				result = 0;
			}
			else if ((oper != QUERY_NE) && (!pt_exists_flag))
			{
				result = 0;
			}
			break;

		case DISK_NAME: 
			if ((oper == QUERY_STRCMP) &&
	       (xfs_query_parse_regexp((char*)value,disk_info->vh_pathname)==0))
			{
				result = 0;
			}
			break;
	
		default:
			result = 1;
			break;
	}

	return(result);
}


/*
 *	xfs_disk_query()
 *	Performs a query on the attributes of the disk. The out_set
 * 	contains references to the disks satisfying the query. 
 *	Return Value:	0 if query is successful
 *			1 in case of error
 */

int
xfs_disk_query(query_clause_t* query_clause,
query_set_t* in_set, query_set_t** out_set)
{
	query_set_t *set_element;
	int returnVal;
	
	/* Query all the disks in the in_set */
	set_element = in_set;
	while (set_element != NULL) 
	{
		if (set_element->type==DISK_TYPE_QUERY)
		{
			/* Evaluate the query for this disk */
			if ((returnVal = xfs_disk_query_eval(
						set_element->value.disk,
						query_clause->attribute_id,
						query_clause->value,
						query_clause->operator))==0)
			{
				add_disk_to_set(set_element, out_set);
			}
		}

		set_element = set_element->next;
	}
	return(returnVal);
}

