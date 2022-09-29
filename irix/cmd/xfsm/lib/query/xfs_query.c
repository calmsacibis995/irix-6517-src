/**************************************************************************
 *                                                                        *
 *           Copyright (C) 1995, Silicon Graphics, Inc.                   *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "xfs_query.c: $Revision: 1.7 $"

/*
 *	XFS queries on disks, filesystems and xlv objects.
 */


#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <wctype.h>
#include <wsregexp.h>
#include <widec.h>
#include <string.h>
#include <bstring.h>
#include <sys/sysmacros.h>
#include <sys/debug.h>
#include <sys/syssgi.h>
#include <sys/types.h>
#include <sys/uuid.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <xlv_oref.h>
#include <xlv_utils.h>
#include <sys/xlv_vh.h>
#include <sys/xlv_attr.h>
#include <sys/dvh.h>
#include <pathnames.h>
#include <xlv_lab.h>
#include <tcl.h>
#include "xfs_fs_defs.h"
#include "xfs_query.h"
#include "xfs_xlv_query.h"
#include "xfs_disk_query_defs.h"
#include "xfs_fs_query_defs.h"
#include "xfs_info_defs.h"
#include "xfs_query_defs.h"

extern boolean_t	isTableInitialized(void);
extern void		init_admin(void);
extern fs_info_entry_t	*xfs_info();


/*
 * xfs_query_parse_regexp()
 *
 * This function compiles the given regular expression and parses the 
 * given value to check if it matches. The wide character based regular 
 * expression routines are used.
 * Return Value: 0 if there is a match 
 *		 1 otherwise
 */

short
xfs_query_parse_regexp(char* regexpn, char* value)
{
	struct rexdata rex;
	short result = 1;
	long expbuf[BUFSIZ];
	wchar_t *inputbuf;
	wchar_t wregexpn[BUFSIZ];
	wchar_t weof;
	char eof = '\0';

	if(value == NULL) {
		return(1);
	}

	/* Parse the regular expression to handle the situation where
	   the regular expression is "*". This should match any 
	   value. */
	if (strcmp(regexpn,"\*")==0)
		return(0);

	inputbuf = (wchar_t*)
			 calloc(strlen((char*)value)+1,sizeof(wchar_t));

	(void) mbstowcs(wregexpn,(char*)regexpn,strlen((char*)regexpn)+1);
	(void) mbtowc(&weof, &eof, 1);

	rex.sed = 0;
	rex.str = wregexpn;
	rex.sed = rex.err = 0;

	if (!wsrecompile(&rex,expbuf,&expbuf[BUFSIZ],weof))
	{
		fprintf(stderr,"%s\n",wsreerr(rex.err));
	}
	else
	{
		(void) mbstowcs(inputbuf,value,strlen(value));
		if (wsrematch(&rex,inputbuf,expbuf)) 
		{
			/* A reg expression like p* causes all strings to
			   be matched because zero or more instances
			   of p is a match! Hence, a check to see, if
			   the last character matching the regular
			   expression is different from the first
			   character of the regular expression
			   eliminates such spurious matches! */

			if (&inputbuf[0] != rex.loc2)
				result = 0;
		}
	}

	free(inputbuf);
	return(result);
}


/*
 *	create_fs_in_set()
 *	Adds references to all file system objects in the system.
 */

void
create_fs_in_set(query_set_t** in_set)
{
	fs_info_entry_t *fs_info_list_entry=NULL;
	fs_info_entry_t	*fs_info_elm;
	query_set_t	*in_set_elm;
	struct mntent	*fs_mnt_info;
	struct exportent *fs_exp_info;
	char*	msg=NULL;

	fs_info_list_entry = xfs_info(NULL, &msg);

	/* Clear any error/warning messages */
	if (msg != NULL) {
		free(msg);
		msg = NULL;
	}

	while (fs_info_list_entry != NULL)
	{
                in_set_elm = (query_set_t*) malloc(sizeof(query_set_t));
                ASSERT(in_set_elm!=NULL);
                in_set_elm->type = FS_TYPE_QUERY;

               	in_set_elm->value.fs_ref = (fs_info_entry_t*)
				malloc(sizeof(fs_info_entry_t));
		ASSERT(in_set_elm->value.fs_ref!=NULL);
		in_set_elm->value.fs_ref->info=fs_info_list_entry->info;
		in_set_elm->value.fs_ref->fs_entry = NULL;
		in_set_elm->value.fs_ref->mount_entry = NULL;
		in_set_elm->value.fs_ref->export_entry = NULL;

		/* Copy the fs_info_list_entry */
		if (fs_info_list_entry->fs_entry!=NULL)
		{
			fs_mnt_info=in_set_elm->value.fs_ref->fs_entry= 
				(struct mntent*) malloc(sizeof(struct mntent));
			ASSERT(fs_mnt_info!=NULL);

			fs_mnt_info->mnt_fsname = (char*) malloc(strlen(
				fs_info_list_entry->fs_entry->mnt_fsname)+1);
			ASSERT(fs_mnt_info->mnt_fsname!=NULL);
			strcpy(fs_mnt_info->mnt_fsname,
				fs_info_list_entry->fs_entry->mnt_fsname);

			fs_mnt_info->mnt_dir = (char*)malloc(strlen(
				fs_info_list_entry->fs_entry->mnt_dir)+1);
			ASSERT(fs_mnt_info->mnt_dir!=NULL);
			strcpy(fs_mnt_info->mnt_dir,
				fs_info_list_entry->fs_entry->mnt_dir);

			fs_mnt_info->mnt_type = (char*)malloc(strlen(
				fs_info_list_entry->fs_entry->mnt_type)+1);
			ASSERT(fs_mnt_info->mnt_type!=NULL);
			strcpy(fs_mnt_info->mnt_type,
				fs_info_list_entry->fs_entry->mnt_type);
			
			fs_mnt_info->mnt_opts = (char*)malloc(strlen(
				fs_info_list_entry->fs_entry->mnt_opts)+1);
			ASSERT(fs_mnt_info->mnt_opts!=NULL);
			strcpy(fs_mnt_info->mnt_opts,
				fs_info_list_entry->fs_entry->mnt_opts);
			
			fs_mnt_info->mnt_freq =
				fs_info_list_entry->fs_entry->mnt_freq;
			fs_mnt_info->mnt_passno =
				fs_info_list_entry->fs_entry->mnt_passno;
		}

		if (fs_info_list_entry->mount_entry!=NULL)
		{
			fs_mnt_info=in_set_elm->value.fs_ref->mount_entry= 
				(struct mntent*) malloc(sizeof(struct mntent));
			ASSERT(fs_mnt_info!=NULL);

			fs_mnt_info->mnt_fsname = (char*) malloc(strlen(
				fs_info_list_entry->mount_entry->mnt_fsname)+1);
			ASSERT(fs_mnt_info->mnt_fsname!=NULL);
			strcpy(fs_mnt_info->mnt_fsname,
				fs_info_list_entry->mount_entry->mnt_fsname);

			fs_mnt_info->mnt_dir = (char*)malloc(strlen(
				fs_info_list_entry->mount_entry->mnt_dir)+1);
			ASSERT(fs_mnt_info->mnt_dir!=NULL);
			strcpy(fs_mnt_info->mnt_dir,
				fs_info_list_entry->mount_entry->mnt_dir);

			fs_mnt_info->mnt_type = (char*)malloc(strlen(
				fs_info_list_entry->mount_entry->mnt_type)+1);
			ASSERT(fs_mnt_info->mnt_type!=NULL);
			strcpy(fs_mnt_info->mnt_type,
				fs_info_list_entry->mount_entry->mnt_type);
			
			fs_mnt_info->mnt_opts = (char*)malloc(strlen(
				fs_info_list_entry->mount_entry->mnt_opts)+1);
			ASSERT(fs_mnt_info->mnt_opts!=NULL);
			strcpy(fs_mnt_info->mnt_opts,
				fs_info_list_entry->mount_entry->mnt_opts);
			
			fs_mnt_info->mnt_freq =
				fs_info_list_entry->mount_entry->mnt_freq;
			fs_mnt_info->mnt_passno =
				fs_info_list_entry->mount_entry->mnt_passno;
		}

		if (fs_info_list_entry->export_entry!=NULL)
		{
			fs_exp_info =
			in_set_elm->value.fs_ref->export_entry =
			(struct exportent*) malloc(sizeof(struct exportent));
			ASSERT(fs_exp_info!=NULL);

			if(fs_info_list_entry->export_entry->xent_dirname) {
			    fs_exp_info->xent_dirname=(char*)malloc(strlen(
			    fs_info_list_entry->export_entry->xent_dirname)+1);
			    strcpy(fs_exp_info->xent_dirname,
				fs_info_list_entry->export_entry->xent_dirname);
			}
			else {
			    fs_exp_info->xent_dirname = NULL;
			}

			if(fs_info_list_entry->export_entry->xent_options) {
			    fs_exp_info->xent_options=(char*)malloc(strlen(
			    fs_info_list_entry->export_entry->xent_options)+1);
			    strcpy(fs_exp_info->xent_options,
				fs_info_list_entry->export_entry->xent_options);
			}
			else {
			    fs_exp_info->xent_options = NULL;
			}
		}
			
                in_set_elm->next = *in_set;
                *in_set = in_set_elm;

		fs_info_elm = fs_info_list_entry;
		fs_info_list_entry = fs_info_list_entry->next;
		delete_xfs_info_entry(fs_info_elm);
	}
}

/*
 *	create_disks_in_set()
 *	Adds references to all the disks in the system.
 */

void
create_disks_in_set(query_set_t** in_set)
{
	extern xlv_vh_entry_t	*xlv_vh_list,
				*nxlv_vh_list;
	xlv_vh_entry_t		*vh_entry;
	query_set_t		*in_set_elm;

	if(! isTableInitialized()) {
		init_admin();
	}

	for(vh_entry = xlv_vh_list;
	    vh_entry != NULL;
	    vh_entry = vh_entry->next) {
		/* Create an entry for each disk */
                in_set_elm = (query_set_t*) malloc(sizeof(query_set_t));
                ASSERT(in_set_elm!=NULL);
                in_set_elm->type = DISK_TYPE_QUERY;
                in_set_elm->value.disk = vh_entry;

                in_set_elm->next = *in_set;
                *in_set = in_set_elm;
	}
	for(vh_entry = nxlv_vh_list;
	    vh_entry != NULL;
	    vh_entry = vh_entry->next) {
		/* Create an entry for each disk */
                in_set_elm = (query_set_t*) malloc(sizeof(query_set_t));
                ASSERT(in_set_elm!=NULL);
                in_set_elm->type = DISK_TYPE_QUERY;
                in_set_elm->value.disk = vh_entry;

                in_set_elm->next = *in_set;
                *in_set = in_set_elm;
	}
}


/*
 *	create_xlv_in_set()
 *	Adds all instances of XLV objects known to the system to the
 *	in_set.
 */

void
create_xlv_in_set(query_set_t** in_set)
{
	extern Tcl_HashTable	xlv_obj_table;
	Tcl_HashEntry		*hash_entry;
	Tcl_HashSearch		search_ptr;
	xlv_oref_t		*oref;
	xlv_query_set_t		*xlv_query_set_elm;
	query_set_t		*in_set_elm;

	init_admin();

	hash_entry = Tcl_FirstHashEntry(&xlv_obj_table,&search_ptr);
	while (hash_entry != NULL)
	{
		oref = (xlv_oref_t*) Tcl_GetHashValue(hash_entry);
		ASSERT(oref!=NULL);

		xlv_query_set_elm = (xlv_query_set_t*)
					malloc(sizeof(xlv_query_set_t));
		ASSERT(xlv_query_set_elm!=NULL);
		xlv_query_set_elm->oref = oref;
		xlv_query_set_elm->next = NULL;

		in_set_elm = (query_set_t*) malloc(sizeof(query_set_t));
		ASSERT(in_set_elm!=NULL);
		in_set_elm->type = XLV_TYPE_QUERY;
		in_set_elm->value.xlv_obj_ref = xlv_query_set_elm;
		in_set_elm->value.xlv_obj_ref->creator = 0;

		in_set_elm->next = *in_set;
		*in_set = in_set_elm;

		do {
			oref = oref->next;
		} while (oref!=NULL);

		hash_entry = Tcl_NextHashEntry(&search_ptr);
	}
}

/*
 *	create_in_set()
 *	This routine creates a set of all instances of XLV objects/disks
 *	and file systems known to the system. 
 */

query_set_t*
create_in_set(void)
{
	query_set_t* in_set = NULL;

	/* Add the XLV objects to the in_set */
	create_xlv_in_set(&in_set);

	/* Add the Disks to the in_set */
	create_disks_in_set(&in_set);

	/* Add the file systems to the in_set */
	create_fs_in_set(&in_set);

	return(in_set);
}


/*
 *	set_xlv_obj_name()
 *	Converts the oref structure into a human readable name for the
 *	XLV object it represents.
 */

void
set_xlv_obj_name(query_set_t **element)
{ 
	xlv_oref_t		*oref;
	xlv_tab_subvol_t	*subvolume;
	char			*name_str;

	if ((element == NULL) || (*element == NULL)) {
		return;
	}

	oref = (*element)->value.xlv_obj_ref->oref;
	name_str = (*element)->value.xlv_obj_ref->name;
	
	if ((oref == NULL) || (name_str == NULL)) {
		return;
	}

	switch(oref->obj_type) {
		case	XLV_OBJ_TYPE_VOL:
			sprintf(name_str,XLV_OREF_VOL(oref)->name);
			break;

		case	XLV_OBJ_TYPE_SUBVOL:
			subvolume = XLV_OREF_SUBVOL(oref);

			if(subvolume->subvol_type == XLV_SUBVOL_TYPE_LOG) {
				strcpy(name_str, "log");
			}
			else if(subvolume->subvol_type==XLV_SUBVOL_TYPE_DATA) {
				strcpy(name_str, "data");
			}
			else if(subvolume->subvol_type == XLV_SUBVOL_TYPE_RT) {
				strcpy(name_str, "rt");
			}
			break;

		case	XLV_OBJ_TYPE_PLEX:
			sprintf(name_str, "%s", XLV_OREF_PLEX(oref)->name);
			break;

		case	XLV_OBJ_TYPE_VOL_ELMNT:
			sprintf(name_str, "%s",
				XLV_OREF_VOL_ELMNT(oref)->veu_name);
			break;
	}
}


/*
 *	delete_fs_set_elm()
 *	Releases the memory allocated to save reference to file system objects.
 */

void
delete_fs_set_elm(query_set_t* elm)
{
	if ((elm->type == FS_TYPE_QUERY) && (elm->value.fs_ref != NULL)) {
		delete_xfs_info_entry(elm->value.fs_ref);
		elm->value.fs_ref = NULL;
	}
}

/*
 *	delete_disk_set_elm()
 *	Releases the memory allocated to save the disk reference.
 */

void
delete_disk_set_elm(query_set_t* elm)
{
	if ((elm->type == DISK_TYPE_QUERY) && (elm->value.disk != NULL)) {
		free(elm->value.disk);
		elm->value.disk = NULL;
	}
}

/*
 *	delete_xlv_set_elm()
 *	Releases the memory allocated to save the xlv object reference. 
 */

void
delete_xlv_set_elm(query_set_t* elm)
{
	if (elm->type == XLV_TYPE_QUERY) {
		if (elm->value.xlv_obj_ref != NULL) {
			if ((elm->value.xlv_obj_ref->oref != NULL) && 
			    (elm->value.xlv_obj_ref->creator == 1)) {
				free(elm->value.xlv_obj_ref->oref);
				elm->value.xlv_obj_ref->oref = NULL;
			}

			free(elm->value.xlv_obj_ref);
			elm->value.xlv_obj_ref = NULL;
		}
	}
}

/*
 *      delete_query_set()
 *      Deletes the query set element at a time by invoking the
 *      delete function of the appropriate element type.
 */
void
delete_query_set(query_set_t **set)
{
	query_set_t	*iter = *set;

	while (iter != NULL) {
		if (iter->type == XLV_TYPE_QUERY) {
			delete_xlv_set_elm(iter);
		}
		else if (iter->type == FS_TYPE_QUERY) {
			delete_fs_set_elm(iter);
		}

		*set = iter->next;
		free(iter);
		iter = *set;
	}
}

		
/*
 *	delete_in_set()
 *	Deletes the input set element at a time by invoking the 
 *	delete function of the appropriate element type. All the
 *	resources allocated in building the input set are also
 *	released.
 */
void
delete_in_set(query_set_t **set)
{
	query_set_t 	*iter = *set;

	while (iter != NULL) {
		if (iter->type == XLV_TYPE_QUERY) {
			delete_xlv_set_elm(iter);
		}
		else if (iter->type == FS_TYPE_QUERY) {
			delete_fs_set_elm(iter);
		}
	
		*set = iter->next;
		free(iter);
		iter = *set;
	}
}

/*
 *	IsElementInSet() - Checks if the element belongs to the set.
 *	Returns	0 if it exists in the set or else 1.
 */

int
IsElementInSet(query_set_t *element, query_set_t *set)
{
	query_set_t *iter;
	int elm_type;
	int result = 1;

	elm_type = element->type;
	iter = set;
	while (iter != NULL)
	{
		/* Compare similar types of objects! */
		if (iter->type == elm_type)
		{
			if (elm_type == XLV_TYPE_QUERY)
			{
				if (strcmp(iter->value.xlv_obj_ref->name,
					element->value.xlv_obj_ref->name)==0)
				{
					result = 0;
					break;
				}
			}
			else if (elm_type == DISK_TYPE_QUERY)
			{
				if (strcmp(iter->value.disk->vh_pathname,
					element->value.disk->vh_pathname)==0)
				{
					result = 0;
					break;
				}
			}
			else if (elm_type == FS_TYPE_QUERY)
			{
				if ((iter->value.fs_ref->info & FS_FLAG)
			       &&(element->value.fs_ref->info & FS_FLAG)
			&& (strcmp(iter->value.fs_ref->fs_entry->mnt_fsname,
			element->value.fs_ref->fs_entry->mnt_fsname)==0))
				{
					result = 0;
					break;
				}
				else if ((iter->value.fs_ref->info & MNT_FLAG)
				&&(element->value.fs_ref->info & MNT_FLAG)
			&& (strcmp(iter->value.fs_ref->mount_entry->mnt_fsname,
			element->value.fs_ref->mount_entry->mnt_fsname)==0))
				{
					result = 0;
					break;
				}
			}
		}

		iter = iter->next;
	}
	
	return(result);
}
		
/*
 *	logical_or_sets(setA,setB) 
 *	The logical OR operation is performed on the sets A and B 
 *	by checking if every element of setA is in setB. The result
 *	of the operation is stored in setA.
 *	setA = setA OR setB
 */

void
logical_or_sets(query_set_t** setA, query_set_t** setB)
{
	query_set_t *element, *prev_element;

	/* Traverse the setB adding the elements which don't belong to setA */
	element = *setB;
	prev_element = NULL;
	while (element != NULL) {
		if (IsElementInSet(element,*setA) != 0) {
			/* Add this element to the setA */
			if (prev_element == NULL) {
				*setB = element->next;
			}
			else {
				prev_element->next = element->next;
			}
			
			element->next = *setA;
			*setA = element;
		}
		else {
			prev_element = element;
		}

		if (prev_element == NULL) {
			element = *setB;
		}
		else {
			element = prev_element->next;
		}
	}
}


/*
 *	logical_and_sets(setA,setB) 
 *	The logical AND operation is performed on the sets A and B 
 *	by checking if every element of setA is in setB. The result
 *	of the operation is stored in setA.
 *	setA = setA AND setB
 */

void
logical_and_sets(query_set_t** setA, query_set_t** setB)
{
	query_set_t* element, *prev_element;

	/* Traverse the setA deleting the elements which don't belong to setB */
	element = *setA;
	prev_element = NULL;

	while (element != NULL) {
		if (IsElementInSet(element,*setB) != 0) {
			/* Remove this element from setA */
			if (prev_element == NULL) {
				*setA = element->next;
			}
			else {
				prev_element->next = element->next;
			}

			free(element);
		}
		else {
			prev_element = element;
		}

		if (prev_element == NULL) {
			element = *setA;
		}
		else {
			element = prev_element->next;
		}
	}
}


/*
 *	query()
 *	This function resolves queries on the file system, disks and 
 *	XLV objects in the system. The list of query clauses must be
 *	specified in the query_defn. A set of matching objects is returned
 *	on successful resolution of the query. If there is an error,
 *	NULL is returned.
 */

query_set_t *
query(query_clause_t *query_defn)
{
	query_clause_t	*query_clause = query_defn;
	short		query_clause_operator = QUERY_CLAUSE_OR_OPER;
	query_set_t	*in_set = NULL,
			*out_set = NULL,
			*result_set = NULL;
	boolean_t	need_xlv = B_TRUE,
			need_disk = B_TRUE,
			need_fs = B_TRUE;

	while (query_clause != NULL) {
		out_set = NULL;

		if (query_clause->query_type == XLV_TYPE_QUERY) {
			if(need_xlv) {
				/* Add the XLV objects to the in_set */
				create_xlv_in_set(&in_set);
				need_xlv = B_FALSE;
			}
			xlv_query(query_clause, in_set, &out_set);
		}
		else if (query_clause->query_type == DISK_TYPE_QUERY) {
			if(need_disk) {
				/* Add the Disks to the in_set */
				create_disks_in_set(&in_set);
				need_disk = B_FALSE;
			}
			xfs_disk_query(query_clause, in_set, &out_set);
		}
		else if (query_clause->query_type == FS_TYPE_QUERY) {
			if(need_fs) {
				/* Add the file systems to the in_set */
				create_fs_in_set(&in_set);
				need_fs = B_FALSE;
			}
			xfs_fs_query(query_clause, in_set, &out_set);
		}

		if (query_clause_operator == QUERY_CLAUSE_OR_OPER) {
			logical_or_sets(&result_set, &out_set);
			delete_query_set(&out_set);
		}
		else if (query_clause_operator == QUERY_CLAUSE_AND_OPER) {
			logical_and_sets(&result_set, &out_set);
			delete_query_set(&out_set);
		}
		
		query_clause_operator = query_clause->query_clause_operator;
		query_clause = query_clause->next;
	}

	delete_in_set(&in_set);
	return(result_set);
}


/*
 *	lookup_attribute()
 *	Searches the list of known attributes for the specified attribute 
 *	and returns the attribute id.
 */

int
lookup_attribute(char* attr_str)
{
	int count=0;
	
	while (count < XFS_QUERY_ATTR_LEN)
	{
		if (strcmp(xfs_query_attr_tab[count].attribute_name,
				attr_str)==0)
			return(xfs_query_attr_tab[count].attribute_id);
		count++;
	}
	return(-1);
}


/*
 *	build_query_clause()
 *	Constructs the query clause for disk attributes.
 */

query_clause_t*
build_query_clause(const char* query_type, const char* query_clause_str)
{
	query_clause_t* query_clause=NULL;
	char* token;

#ifdef DEBUG
	printf("Constructing query clause %s\n", query_clause_str);
#endif

	query_clause = (query_clause_t*) malloc(sizeof(query_clause_t));
	ASSERT(query_clause!=NULL);

	if (strcmp(query_type,FS_TYPE)==0)
		query_clause->query_type = FS_TYPE_QUERY;
	else if (strcmp(query_type,DISK_TYPE)==0)
		query_clause->query_type = DISK_TYPE_QUERY;
	else if (strcmp(query_type,VOL_TYPE)==0)
		query_clause->query_type = XLV_TYPE_QUERY;

	if ((token = strtok((char*)query_clause_str,"{:")) != NULL)
	{
		query_clause->attribute_id = lookup_attribute(token);
	}

	/* Fetch the operator */
	if ((token = strtok(NULL,":")) != NULL) {
		if (strcmp(token,"EQ")==0) {
			query_clause->operator = QUERY_EQUAL;
		}
		else if (strcmp(token,"NEQ")==0) {
			query_clause->operator = QUERY_NE;
		}
		else if (strcmp(token,"GT")==0) {
			query_clause->operator = QUERY_GT;
		}
		else if (strcmp(token,"GE")==0) {
			query_clause->operator = QUERY_GE;
		}
		else if (strcmp(token,"LT")==0) {
			query_clause->operator = QUERY_LT;
		}
		else if (strcmp(token,"LE")==0) {
			query_clause->operator = QUERY_LE;
		}
		else if (strcmp(token,"STRCMP")==0) {
			query_clause->operator = QUERY_STRCMP;
		}
		else if (strcmp(token,"TRUE")==0) {
			query_clause->operator = QUERY_TRUE;
		}
		else if (strcmp(token,"FALSE")==0) {
			query_clause->operator = QUERY_FALSE;
		}
	}
		
	/* Fetch the value to be compared */	
	if ((token = strtok(NULL,":")) != NULL)
	{
		query_clause->value = (char*)malloc(strlen(token)+1);
		strcpy(query_clause->value,token);
	}

	/* Fetch the query clause operator */	
	if ((token = strtok(NULL,":")) != NULL)
	{
		if (strcmp(token,"OR")==0)
		{
			query_clause->query_clause_operator = 
						QUERY_CLAUSE_OR_OPER;
		}
		else if (strcmp(token,"AND")==0)
		{
			query_clause->query_clause_operator = 
						QUERY_CLAUSE_AND_OPER;
		}
		else if (strcmp(token,"EOQ")==0)
		{
			query_clause->query_clause_operator = EOQ;
		}
	}
	
	query_clause->next = NULL;

	return(query_clause);
}


/*
 *	build_query()
 *	Constructs the query by interpreting query_defn_str, query clause 
 *	at a time. 
 */

query_clause_t*
build_query(const char* query_type,const char* query_defn_str)
{
	query_clause_t *query_defn=NULL;
	query_clause_t *query_clause=NULL;
	query_clause_t *last_query_clause=NULL;
	char	dup_query_defn_str[BUFSIZ];
	char	query_clause_str[BUFSIZ];
	char	*token;
	char	*ptr_next_token;

	/* Make a copy of the query_defn_str as it mangled during strtok()*/
	strcpy(dup_query_defn_str,query_defn_str);

	token = strtok(dup_query_defn_str,"}");
	while (token != NULL)
	{
		/* Save pointer to next token */
		ptr_next_token = (token + strlen(token) + 1);

		strcpy(query_clause_str,token);
		query_clause = build_query_clause(query_type,query_clause_str);

		/* Add the query clause to query_defn */
		if (query_clause != NULL)
		{
			if (last_query_clause != NULL)
			{
				last_query_clause->next = query_clause;
				query_clause->next = NULL;
				last_query_clause = query_clause;
			}
			else
			{
				last_query_clause = query_defn = query_clause;
				query_clause->next = NULL;
			}
		}

		token = strtok(ptr_next_token,"}");
	}

	return(query_defn);
}

/*
 *	delete_query()
 *	Release the memory allocated for the query definition and results set.
 */

void
delete_query(query_clause_t* query_defn, query_set_t* set)
{
	query_clause_t *query_clause, *next_query_clause;
	query_set_t	*elm, *next_elm;

	/* Delete the query definition */
	query_clause = query_defn;
	while (query_clause != NULL)
	{
		if (query_clause->value != NULL)
		{
			free(query_clause->value);
		}

		next_query_clause = query_clause->next;
		free(query_clause);
		query_clause = next_query_clause;
	}
	
	/* Delete the results set */
	elm = set;
	while (elm != NULL)
	{
		if (elm->type == XLV_TYPE_QUERY)
		{
			if (elm->value.xlv_obj_ref != NULL) 
			{ 
				if ((elm->value.xlv_obj_ref->oref != NULL) &&
					(elm->value.xlv_obj_ref->creator == 1))
				{
					free(elm->value.xlv_obj_ref->oref);
				}

				free(elm->value.xlv_obj_ref);
			}
		}
		else if (elm->type == DISK_TYPE_QUERY)
		{
			if (elm->value.disk != NULL)
			{
				free(elm->value.disk);
			}
		}
		else if (elm->type == FS_TYPE_QUERY)
		{
			if (elm->value.fs_ref != NULL)
			{
				if (elm->value.fs_ref->fs_entry != NULL)
				{
					free(elm->value.fs_ref->fs_entry);
				}

				if (elm->value.fs_ref->export_entry != NULL)
				{
					free(elm->value.fs_ref->export_entry);
				}

				free(elm->value.fs_ref);
			}
		}

		next_elm = elm->next;
		free(elm);
		elm = next_elm;
	}
}


#ifdef DEBUG

/*
 *	display_results()
 *	Prints out the objects which satisfied the query.
 */
void
display_results(query_set_t* result_set)
{
	query_set_t* elm;

	if (result_set == NULL)
	{
		printf("\nNo matches to query\n");
	}
	else
	{
		printf("\nThe following objects matched the query\n");
	}

	elm = result_set;
	while (elm != NULL)
	{
		if (elm->type == XLV_TYPE_QUERY)
		{
			printf("XLV Object %s\n",elm->value.xlv_obj_ref->name);
		}
		else if (elm->type == DISK_TYPE_QUERY)
		{
			printf("Disk %s\n",elm->value.disk->vh_pathname);
		}
		else if (elm->type == FS_TYPE_QUERY)
		{
			if (elm->value.fs_ref->fs_entry != NULL)
			{
				printf("File System %s\n",
				elm->value.fs_ref->fs_entry->mnt_fsname);
			}
			else if (elm->value.fs_ref->mount_entry != NULL)
			{
				printf("File System %s\n",
				elm->value.fs_ref->mount_entry->mnt_fsname);
			}
		}

		elm = elm->next;
	}
}
#endif	/* DEBUG */


/*
 *	dump_results_to_buffer()
 *	Saves the object id's of the matching objects of the query
 *	in the buffer.
 */

void
dump_results_to_buffer(char *hostname, query_set_t *result_set, char **objs)
{
	query_set_t	*elm;
	char		*subtype;

	for(elm = result_set; elm != NULL; elm = elm->next) {
		if (elm->type == XLV_TYPE_QUERY)
		{
			/* Add the object type from the oref */
			switch (elm->value.xlv_obj_ref->oref->obj_type)
			{
				case XLV_OBJ_TYPE_VOL:
					subtype = VOL_TYPE;
					break;

				case XLV_OBJ_TYPE_PLEX:
					subtype = PLEX_TYPE;
					break;

				case XLV_OBJ_TYPE_VOL_ELMNT:
					subtype = VE_TYPE;
					break;

				case XLV_OBJ_TYPE_LOG_SUBVOL:
					subtype = LOG_SUBVOL_TYPE;
					break;

				case XLV_OBJ_TYPE_DATA_SUBVOL:
					subtype = DATA_SUBVOL_TYPE;
					break;

				case XLV_OBJ_TYPE_RT_SUBVOL:
					subtype = RT_SUBVOL_TYPE;
					break;

				default:
					subtype = UNKNOWN_TYPE;
					break;
			}
			add_obj_to_buffer(objs, hostname, VOL_TYPE,
					elm->value.xlv_obj_ref->name, subtype);

		}
		else if (elm->type == DISK_TYPE_QUERY)
		{
			add_obj_to_buffer(objs,hostname,DISK_TYPE,
					elm->value.disk->vh_pathname,"0");
		}
		else if (elm->type == FS_TYPE_QUERY)
		{
			if (elm->value.fs_ref->fs_entry != NULL)
			{
				add_obj_to_buffer(objs,hostname,FS_TYPE,
				elm->value.fs_ref->fs_entry->mnt_fsname,
				elm->value.fs_ref->fs_entry->mnt_type);
			}
			else if (elm->value.fs_ref->mount_entry != NULL)
			{
				add_obj_to_buffer(objs,hostname,FS_TYPE,
				elm->value.fs_ref->mount_entry->mnt_fsname,
				elm->value.fs_ref->mount_entry->mnt_type);
			}
		}
	}
}


/*
 *	xfs_query()
 *	Resolves a query on disks, filesystems or xlv objects in xfs.
 *	The query_type indicates the type of objects being queried.
 *	The query definition string, query_defn defines the query clauses
 *	(conditions). The matching object id's are stored in the string, objs. 
 *	All error messages are logged in the buffer, msg.
 */

int
xfs_query(char *hostname,
	  const char *query_type,
	  const char *query_defn_str,
	  char **objs,
	  char **msg)
{
	query_clause_t	*query_defn;
	query_set_t	*results;
	int		returnVal = 0;

	/* Construct the query definition */
	if ((query_defn = build_query(query_type, query_defn_str)) == NULL)
	{
		returnVal = 1;
	}
	else
	{
		/* Perform the query */
		results = query(query_defn);
		dump_results_to_buffer(hostname, results, objs);

#ifdef DEBUG
		/* Display the results */
		display_results(results);
#endif

		/* Release all resources before quiting */
		delete_query(query_defn, results);
	}

	return(returnVal);
}

