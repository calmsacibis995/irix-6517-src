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
#ident "$Revision: 1.1 $"

#define GRP_LINESIZE        128

#define GRP_INVALID_TYPE_ERR_FMT "Type not specified for element in group %s\n"

/* Structure to hold the group information */
typedef struct group_entry {
        char    *group_name;	/* The name of the group */
	char	*group_type;	/* The type of the group */
        int     group_count;	/* Number of elements in group */
        char    *elm_list;	/* The element list */
} group_entry_t;

/* Structure to hold the list of groups */
typedef struct group_entry_list {
        group_entry_t   *group;	
        struct group_entry_list *next;
} group_entry_list_t;

/* Function prototypes for group management */
extern int create_group(const char* filename,const char* group_name,const char* group_type,char** msg);

extern int delete_group(const char* filename,const char *name,const char *type,char** msg);

extern int add_element_to_group(const char* filename,const char* group_name,const char* group_type,const char* elm_name,const char* elm_type,char** msg);

extern int delete_element_from_group(const char* filename,const char *group_name,const char* group_type,const char *elm_name,const char *elm_type,char** msg);

extern int list_groups(char* filename,char** info,char** msg);
