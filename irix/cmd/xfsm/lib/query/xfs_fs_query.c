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

#ident "xfs_fs_query.c: $Revision: 1.3 $"

/*
 *	Queries for xfs filesystem attributes implementation file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bstring.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/debug.h>
#include <sys/syssgi.h>

#include <sys/uuid.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <xlv_oref.h>
#include <xlv_utils.h>
#include <sys/xlv_vh.h>
#include <sys/xlv_attr.h>
#include <sys/dvh.h>
#include <xlv_lab.h>
#include <exportent.h>


#include "xfs_fs_defs.h"
#include "xfs_query.h"
#include "xfs_xlv_query.h"
#include "xfs_fs_query_defs.h"


/*
 *	add_fs_to_set()
 *	Adds a reference to the file system in the set.
 */

void
add_fs_to_set(query_set_t *element, query_set_t **set)
{
	query_set_t *new_element;
	struct mntent	*fs_mnt_info;
	struct exportent *fs_exp_info;

	new_element = (query_set_t*) malloc(sizeof(query_set_t));
	ASSERT(new_element!=NULL);
	new_element->type = element->type;

        new_element->value.fs_ref = (fs_info_entry_t*)
				malloc(sizeof(fs_info_entry_t));
	ASSERT(new_element->value.fs_ref!=NULL);
	new_element->value.fs_ref->info=element->value.fs_ref->info;
	new_element->value.fs_ref->fs_entry = NULL;
	new_element->value.fs_ref->mount_entry = NULL;
	new_element->value.fs_ref->export_entry = NULL;

	/* Copy the element->value.fs_ref */
	if (element->value.fs_ref->fs_entry!=NULL)
	{

		fs_mnt_info=new_element->value.fs_ref->fs_entry= 
			(struct mntent*) malloc(sizeof(struct mntent));
		ASSERT(fs_mnt_info!=NULL);

		fs_mnt_info->mnt_fsname = (char*) malloc(strlen(
			element->value.fs_ref->fs_entry->mnt_fsname)+1);
		ASSERT(fs_mnt_info->mnt_fsname!=NULL);
		strcpy(fs_mnt_info->mnt_fsname,
			element->value.fs_ref->fs_entry->mnt_fsname);

		fs_mnt_info->mnt_dir = (char*)malloc(strlen(
			element->value.fs_ref->fs_entry->mnt_dir)+1);
		ASSERT(fs_mnt_info->mnt_dir!=NULL);
		strcpy(fs_mnt_info->mnt_dir,
			element->value.fs_ref->fs_entry->mnt_dir);

		fs_mnt_info->mnt_type = (char*)malloc(strlen(
			element->value.fs_ref->fs_entry->mnt_type)+1);
		ASSERT(fs_mnt_info->mnt_type!=NULL);
		strcpy(fs_mnt_info->mnt_type,
			element->value.fs_ref->fs_entry->mnt_type);
			
		fs_mnt_info->mnt_opts = (char*)malloc(strlen(
			element->value.fs_ref->fs_entry->mnt_opts)+1);
		ASSERT(fs_mnt_info->mnt_opts!=NULL);
		strcpy(fs_mnt_info->mnt_opts,
			element->value.fs_ref->fs_entry->mnt_opts);
			
		fs_mnt_info->mnt_freq = 
			element->value.fs_ref->fs_entry->mnt_freq;
		fs_mnt_info->mnt_passno =
			element->value.fs_ref->fs_entry->mnt_passno;
	}

	if (element->value.fs_ref->mount_entry!=NULL)
	{

		fs_mnt_info=new_element->value.fs_ref->mount_entry= 
			(struct mntent*) malloc(sizeof(struct mntent));
		ASSERT(fs_mnt_info!=NULL);

		fs_mnt_info->mnt_fsname = (char*) malloc(strlen(
			element->value.fs_ref->mount_entry->mnt_fsname)+1);
		ASSERT(fs_mnt_info->mnt_fsname!=NULL);
		strcpy(fs_mnt_info->mnt_fsname,
			element->value.fs_ref->mount_entry->mnt_fsname);

		fs_mnt_info->mnt_dir = (char*)malloc(strlen(
			element->value.fs_ref->mount_entry->mnt_dir)+1);
		ASSERT(fs_mnt_info->mnt_dir!=NULL);
		strcpy(fs_mnt_info->mnt_dir,
			element->value.fs_ref->mount_entry->mnt_dir);

		fs_mnt_info->mnt_type = (char*)malloc(strlen(
			element->value.fs_ref->mount_entry->mnt_type)+1);
		ASSERT(fs_mnt_info->mnt_type!=NULL);
		strcpy(fs_mnt_info->mnt_type,
			element->value.fs_ref->mount_entry->mnt_type);
			
		fs_mnt_info->mnt_opts = (char*)malloc(strlen(
			element->value.fs_ref->mount_entry->mnt_opts)+1);
		ASSERT(fs_mnt_info->mnt_opts!=NULL);
		strcpy(fs_mnt_info->mnt_opts,
			element->value.fs_ref->mount_entry->mnt_opts);
			
		fs_mnt_info->mnt_freq = 
			element->value.fs_ref->mount_entry->mnt_freq;
		fs_mnt_info->mnt_passno =
			element->value.fs_ref->mount_entry->mnt_passno;
	}

	if (element->value.fs_ref->export_entry!=NULL)
	{
		fs_exp_info = new_element->value.fs_ref->export_entry =
		(struct exportent*) malloc(sizeof(struct exportent));
		ASSERT(fs_exp_info!=NULL);

		if(element->value.fs_ref->export_entry->xent_dirname) {
		    fs_exp_info->xent_dirname=(char*)malloc(strlen(
			element->value.fs_ref->export_entry->xent_dirname)+1);
		    strcpy(fs_exp_info->xent_dirname,
			element->value.fs_ref->export_entry->xent_dirname);
		}
		else {
		    fs_exp_info->xent_dirname = NULL;
		}

		if(element->value.fs_ref->export_entry->xent_options) {
		    fs_exp_info->xent_options=(char*)malloc(strlen(
			element->value.fs_ref->export_entry->xent_options)+1);
		    strcpy(fs_exp_info->xent_options,
			element->value.fs_ref->export_entry->xent_options);
		}
		else {
		    fs_exp_info->xent_options = NULL;
		}
	}
			

	new_element->next = *set;
	*set = new_element;
}



/*
 *	xfs_fs_query_eval()
 *	Evaluate the query for the file system.
 *	Return Value:	0 if condition is TRUE
 *			1 if condition is FALSE
 */

int
xfs_fs_query_eval(fs_info_entry_t* fs_info,short attr_id,char* value,
short oper)
{
	int result=1;
	int value_int;

	switch(attr_id) {
	
		case FS_MOUNTED_STATE:
			if (fs_info->info & MNT_FLAG)
			{
				/* FS is mounted */
				if (oper==QUERY_TRUE)
					result = 0;
			}
			else
			{
				/* FS is not mounted */
				if (oper==QUERY_FALSE)
					result = 0;
			}
			break;

		case FS_EXPORTED_STATE:
			if (fs_info->info & EXP_FLAG)
			{
				/* FS is exported */
				if (oper==QUERY_TRUE)
					result = 0;
			}
			else
			{
				/* FS is not exported */
				if (oper==QUERY_FALSE)
					result = 0;
			}
			break;

		case FS_TYPE_INFO:
			if ((oper == QUERY_STRCMP) &&
				(fs_info->fs_entry != NULL) &&
				(xfs_query_parse_regexp((char*)value,
					fs_info->fs_entry->mnt_type)==0))
			{
				result = 0;
			}
			break;

		case FS_MOUNT_OPTIONS:
			if ((oper == QUERY_STRCMP) &&
				(fs_info->fs_entry != NULL) &&
				(xfs_query_parse_regexp((char*)value,
					fs_info->fs_entry->mnt_opts)==0))
			{
				result = 0;
			}
			break;

		case FS_EXPORT_OPTIONS:
			if ((oper == QUERY_STRCMP) &&
				(fs_info->export_entry != NULL) &&
				(xfs_query_parse_regexp((char*)value,
				fs_info->export_entry->xent_options)==0))
			{
				result = 0;
			}
			break;
			
		case FS_MOUNT_FREQ:
			sscanf(value,"%d",&value_int);
			if ((oper == QUERY_EQUAL) && 
				(fs_info->fs_entry != NULL) &&
				(fs_info->fs_entry->mnt_freq == value_int))
			{
				result = 0;
			}
			else if ((oper == QUERY_NE) && 
				(fs_info->fs_entry != NULL) &&
				(fs_info->fs_entry->mnt_freq != value_int))
			{
				result = 0;
			}
			else if ((oper == QUERY_GT) && 
				(fs_info->fs_entry != NULL) &&
				(fs_info->fs_entry->mnt_freq > value_int))
			{
				result = 0;
			}
			else if ((oper == QUERY_GE) && 
				(fs_info->fs_entry != NULL) &&
				(fs_info->fs_entry->mnt_freq >= value_int))
			{
				result = 0;
			}
			else if ((oper == QUERY_LT) && 
				(fs_info->fs_entry != NULL) &&
				(fs_info->fs_entry->mnt_freq < value_int))
			{
				result = 0;
			}
			else if ((oper == QUERY_LE) && 
				(fs_info->fs_entry != NULL) &&
				(fs_info->fs_entry->mnt_freq <= value_int))
			{
				result = 0;
			}
			break;

		case FS_NAME:
			if ((oper == QUERY_STRCMP) &&
				(fs_info->fs_entry != NULL) &&
				(xfs_query_parse_regexp((char*)value,
					fs_info->fs_entry->mnt_fsname)==0))
			{
				result = 0;
			}
			break;

		case FS_MNT_DIR_NAME:
			if ((oper == QUERY_STRCMP) &&
				(fs_info->fs_entry != NULL) &&
				(xfs_query_parse_regexp((char*)value,
					fs_info->fs_entry->mnt_dir))==0)
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
 *	xfs_fs_query()
 *	Performs a query on the attributes of the file system. The out_set
 *	contains references to the file systems satisfying the query.
 *	Return Value:	0 if query is successful
 *			1 in case of error
 */

int
xfs_fs_query(query_clause_t* query_clause,
query_set_t* in_set, query_set_t** out_set)
{
	query_set_t *set_element;
	int returnVal;

	/* Query all the file systems in the in_set */
	set_element = in_set;
	while (set_element != NULL)
	{
		if (set_element->type == FS_TYPE_QUERY)
		{
			/* Evaluate the query for this file system */
			if ((returnVal = xfs_fs_query_eval(
						set_element->value.fs_ref,
						query_clause->attribute_id,
						query_clause->value,
						query_clause->operator)) == 0)
			{
				add_fs_to_set(set_element, out_set);
			}
		}

		set_element = set_element->next;
	}
	return(returnVal);
}


