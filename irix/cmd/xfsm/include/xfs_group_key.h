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

#ident "xfs_group_key.h: $Revision: 1.1 $"

/*
 *	The definition of keywords used for group operations are listed
 *	here.
 */

#define	XFS_GROUP_FILE_STR	"XFS_GROUP_FILE"
#define	XFS_GROUP_NAME_STR	"XFS_GROUP_NAME"	
#define	XFS_GROUP_TYPE_STR	"XFS_GROUP_TYPE"	
#define	XFS_GROUP_ELM_STR	"XFS_GROUP_ELM"	
#define	XFS_GROUP_OPER_STR	"XFS_GROUP_OPER"
#define	XFS_GROUP_BEGIN_STR	"XFS_GROUP_BEGIN"
#define	XFS_GROUP_END_STR	"XFS_GROUP_END"

/* Different group operation supported */
#define	XFS_GROUP_LIST_OPER_STR		"XFS_GROUP_LIST"
#define	XFS_GROUP_CREATE_OPER_STR	"XFS_GROUP_CREATE"
#define	XFS_GROUP_DELETE_OPER_STR	"XFS_GROUP_DELETE"
#define	XFS_GROUP_ADD_ELM_STR		"XFS_GROUP_ADD_ELM"
#define	XFS_GROUP_RM_ELM_STR		"XFS_GROUP_RM_ELM"

/* To temporarily bypass internationalization */
#define gettxt(str_line_num,str_default)        str_default

/* Function prototypes */
extern int create_group(const char* filename,const char* group_name,const char* group_type,char** msg);
extern int delete_group(const char* filename,const char *name,const char *type,char** msg);
extern int add_element_to_group(const char* filename,const char* group_name,const char* group_type,const char* elm_name,const char* elm_type,char** msg);
extern int delete_element_from_group(const char* filename,const char *group_name,const char* group_type,const char *elm_name,const char *elm_type,char** msg);
extern int list_groups(char* filename,char** info,char** msg);
