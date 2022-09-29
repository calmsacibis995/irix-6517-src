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

/*
 *	Generates list of all the disks, XLV objects and file systems
 *	on current host to the GUI.
 */

#ident  "xfs_get_obj.c: $Revision: 1.4 $"

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <bstring.h>
#include <wctype.h>
#include <wsregexp.h>
#include <widec.h>
#include <unistd.h>
#include <invent.h>

#include <sys/types.h>
#include <sys/uuid.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <xlv_oref.h>
#include <xlv_utils.h>
#include <sys/dvh.h>
#include <sys/xlv_vh.h>
#include <sys/xlv_attr.h>
#include <xlv_lab.h>
#include <xlv_error.h>
#include <sys/debug.h>


#include "xfs_fs_defs.h"
#include "xfs_query.h"
#include "xfs_xlv_query.h"
#include "xfs_disk_query_defs.h"
#include "xfs_fs_query_defs.h"
#include "xfs_info_defs.h"

static boolean_t xfsmGetXlvObjects(char *, char *, char **, char **);
static boolean_t xfsmGetFsObjects(char *, char *, char **, char **);
static boolean_t xfsmGetDiskObjects(char *, char *, char **, char **);

/*
 *	add_obj_to_buffer()
 *	Adds the specified object into the buffer in the format
 *		{hostname obj_type obj_name subtype}
 *	The memory management is performed for the buffer dynamically.
 */

void
add_obj_to_buffer(char** buffer,char* hostname,char* obj_type,char* obj_name,
char* obj_subtype)
{
	char str[BUFSIZ];

	sprintf(str,"{ %s %s %s %s }\n",hostname,obj_type,
					obj_name,obj_subtype);

	if (*buffer == NULL)
	{
		*buffer = malloc(strlen(str)+1);
		ASSERT(*buffer!=NULL);
		strcpy(*buffer,str);
	}
	else
	{
		*buffer = realloc(*buffer,strlen(*buffer)+strlen(str)+1);
		ASSERT(*buffer!=NULL);
		strcat(*buffer,str);
	}
}

/*
 *	getObjectsInternal(criteria, objs, msg)
 *
 *	This routine takes the criteria for finding the file systems/disks/
 *	XLV objects on the host & stores the matching objects in objs string.
 *	Any warnings/error messages are stored in msg.
 *	
 *	The format of criteria is a series of keyword value pairs with
 *	newline as seperator.
 *		HOST_PATTERN:hostname
 *		OBJ_PATTERN:reg_expression
 *		OBJ_TYPE:fs/disk/vol
 *		QUERY_DEFN:{attribute_id:operator:value:query_clause_operator}
 *
 *	The objects are stored in the objs string in the format below.
 *		{hostname obj_type obj_name subtype}
 *
 *	The warning/error messages in msg string are of the format
 *		{message1}{message2}.....
 *
 *	Return Value:
 *		0	success
 *		1	failure
 *		2	partial success
 *
 */

int
getObjectsInternal(const char *criteria, char **objs, char **msg)
{
	int 	returnVal = 0;
	char	*line;
	char	*key,
		*value;
	char	*token;
	char	str[GET_OBJ_BUF_SIZE];
	char	host_pattern[GET_OBJ_BUF_SIZE];
	char	obj_pattern[GET_OBJ_BUF_SIZE];
	char	obj_type[GET_OBJ_BUF_SIZE];
	char	hostname[256];
	char	query_defn[BUFSIZ];
	query_set_t	*in_set = NULL;
	query_set_t	*elm;

	/* Initialize the buffers */
	host_pattern[0] = '\0';
	obj_pattern[0] = '\0';
	obj_type[0] = '\0';
	query_defn[0] = '\0';

	gethostname(hostname, 256);

	/* Check the input parameters */
	if ((criteria == NULL) || (objs == NULL) || (msg == NULL)) {
		return(1);
	}

	/* Initialize the objs and msg buffers */
	*objs = NULL;
	*msg = NULL;

	/* Parse the criteria to extract: hostname, obj_pattern and obj_type */
	for(line = strtok((char*)criteria,"\n");
	    line != NULL;
	    line = strtok(NULL, "\n")) {

		if(xfsmGetKeyValue(line, &key, &value)) {
			if (strcmp(key, HOST_PATTERN_KEY) == 0) {
				strcpy(host_pattern, value); 
			}
			else if (strcmp(key, OBJ_PATTERN_KEY) == 0) {
				strcpy(obj_pattern, value); 
			}
			else if (strcmp(key, OBJTYP_PATTERN_KEY) == 0) {
				strcpy(obj_type, value); 
			}
			else if (strcmp(key, QUERY_DEFN_KEY) == 0) {
				strcpy(query_defn, value); 
			}
		}
	}

	/* Check if the object type and pattern are known */
	if ((obj_type[0] != '\0') && (obj_pattern[0] != '\0'))
	{
		/* Match all objects for regular expression in obj_pattern */
		/* Build the set of all the possible objects of given type */
		if (strcmp(obj_type,FS_TYPE)==0)
		{
			xfsmGetFsObjects(obj_pattern, hostname, objs, msg);
		}
		else if (strcmp(obj_type,DISK_TYPE)==0)
		{
			xfsmGetDiskObjects(obj_pattern, hostname, objs, msg);
		}
		else if (strcmp(obj_type,VOL_TYPE)==0)
		{
			xfsmGetXlvObjects(obj_pattern, hostname, objs, msg);
		}
	}
	/* Perform a xfs query to find all matching objects */
	else if ((obj_type[0] != '\0') && (query_defn[0] != '\0'))
	{
		returnVal = xfs_query(hostname,obj_type,query_defn,objs,msg);
	}

	return(returnVal);
}

static boolean_t
xfsmGetXlvObjects(char *pattern, char *hostname, char **objs, char **msg)
{
	query_set_t	*in_set = NULL;
	query_set_t	*elm;

	create_xlv_in_set(&in_set);

	for(elm = in_set; elm != NULL; elm = elm->next) {

		set_xlv_obj_name(&elm);
		if(xfs_query_parse_regexp(pattern,
			elm->value.xlv_obj_ref->name) == 0) {

			/* Add the object type from the oref */
			switch(elm->value.xlv_obj_ref->oref->obj_type) {
				case	XLV_OBJ_TYPE_VOL:
					add_obj_to_buffer(objs,
						hostname,
						VOL_TYPE,
						elm->value.xlv_obj_ref->name,
						VOL_TYPE);
					break;

				case	XLV_OBJ_TYPE_PLEX:
					add_obj_to_buffer(objs,
						hostname,
						VOL_TYPE,
						elm->value.xlv_obj_ref->name,
						PLEX_TYPE);
					break;

				case	XLV_OBJ_TYPE_VOL_ELMNT:
					add_obj_to_buffer(objs,
						hostname,
						VOL_TYPE,
						elm->value.xlv_obj_ref->name,
						VE_TYPE);
					break;

				case	XLV_OBJ_TYPE_LOG_SUBVOL:
					add_obj_to_buffer(objs,
						hostname,
						VOL_TYPE,
						elm->value.xlv_obj_ref->name,
						LOG_SUBVOL_TYPE);
					break;

				case	XLV_OBJ_TYPE_DATA_SUBVOL:
					add_obj_to_buffer(objs,
						hostname,
						VOL_TYPE,
						elm->value.xlv_obj_ref->name,
						DATA_SUBVOL_TYPE);
					break;

				case	XLV_OBJ_TYPE_RT_SUBVOL:
					add_obj_to_buffer(objs,
						hostname,
						VOL_TYPE,
						elm->value.xlv_obj_ref->name,
						RT_SUBVOL_TYPE);
					break;

				default:
					add_obj_to_buffer(objs,
						hostname,
						VOL_TYPE,
						elm->value.xlv_obj_ref->name,
						UNKNOWN_TYPE);
					break;
			}
		}
	}

	delete_in_set(&in_set);

	return B_TRUE;
}

static boolean_t
xfsmGetFsObjects(char *pattern, char *hostname, char **objs, char **msg)
{
	query_set_t	*in_set = NULL;
	query_set_t	*elm;

	create_fs_in_set(&in_set);

	for(elm = in_set; elm != NULL; elm = elm->next) {
		if((elm->value.fs_ref->fs_entry != NULL) &&
		   (xfs_query_parse_regexp(pattern,
			    elm->value.fs_ref->fs_entry->mnt_fsname) == 0)) {
			if(elm->value.fs_ref->fs_entry->mnt_type != NULL) {
				add_obj_to_buffer(objs,
					hostname,
					FS_TYPE,
					elm->value.fs_ref->fs_entry->mnt_fsname,
					elm->value.fs_ref->fs_entry->mnt_type);
			}
			else {
				add_obj_to_buffer(objs,
					hostname,
					FS_TYPE,
					elm->value.fs_ref->fs_entry->mnt_fsname,
					"0");
			}
		}
		else if((elm->value.fs_ref->mount_entry != NULL) &&
			(xfs_query_parse_regexp(pattern,
			    elm->value.fs_ref->mount_entry->mnt_fsname) == 0)) {
			if(elm->value.fs_ref->mount_entry->mnt_type != NULL) {
				add_obj_to_buffer(objs,
				    hostname,
				    FS_TYPE,
				    elm->value.fs_ref->mount_entry->mnt_fsname,
				    elm->value.fs_ref->mount_entry->mnt_type);
			}
			else {
				add_obj_to_buffer(objs,
				    hostname,
				    FS_TYPE,
				    elm->value.fs_ref->mount_entry->mnt_fsname,
				    "0");
			}
		}
	}

	delete_in_set(&in_set);

	return B_TRUE;
}

static boolean_t
xfsmGetDiskObjects(char *pattern, char *hostname, char **objs, char **msg)
{
	inventory_t	*inv;
	char		vhpathname[128];

	if(setinvent() == -1) {
		return B_FALSE;
	}

	while((inv = getinvent()) != NULL) {
		if(inv->inv_class != INV_DISK) {
			continue;
		}

		switch(inv->inv_type) {
			case	INV_SCSIDRIVE:
				if(inv->inv_state) {
					sprintf(vhpathname,
						"/dev/rdsk/dks%dd%dl%dvh",
						inv->inv_controller,
						inv->inv_unit,
						inv->inv_state);
				}
				else {
					sprintf(vhpathname,
						"/dev/rdsk/dks%dd%dvh",
						inv->inv_controller,
						inv->inv_unit);
				}
				break;

			case	INV_VSCSIDRIVE:
				sprintf(vhpathname,
					"/dev/rdsk/jag%dd%dvh",
					inv->inv_controller,
					inv->inv_unit);
				break;

			case	INV_SCSIRAID:
				sprintf(vhpathname,
					"/dev/rdsk/rad%dd%dvh",
					inv->inv_controller,
					inv->inv_unit);
				break;

			default:
				continue;
		}

		if(strlen(pattern) == 0 ||
		   strcmp(pattern, "*") == 0 ||
		   xfs_query_parse_regexp(pattern, vhpathname) == 0) {
			add_obj_to_buffer(objs,
					hostname,
					DISK_TYPE,
					vhpathname,
					"0");
		}
	}

	endinvent();

	return B_TRUE;
}
