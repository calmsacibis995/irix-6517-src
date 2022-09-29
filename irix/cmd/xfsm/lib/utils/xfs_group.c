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
 *	The interface function for the RPC call to manage groups. It
 *	uses the services of the group library.
 */

#ident "xfs_group.c: $Revision: 1.2 $"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/debug.h>
#include <pfmt.h>
#include "xfs_utils.h"
#include "xfs_group_key.h"

/*
 *	xfsGroupInternal()
 *	This function interfaces with the group management functions
 *	on the local host. The various operations supported are 
 *	creation,deletion,listing a group. In addition a new member can
 *	added to the group or existing member deleted from the group.
 *	The parameters indicate the file associated with the group,
 *	the information associated with each individual operation
 *	through defined keyword value pairs.
 *
 *	The warning/error messages in msg string are of the format
 *		{message1}{message2}.....
 *
 *	Return Value:
 *		0       Success
 *		1       Failure
 *		2       Partial success
 *
 */

int
xfsGroupInternal(const char* parameters, char **info, char **msg)
{
	int	returnVal = 0;
	char	*line;
	char	*key;
	char	*value;
	char	str[BUFSIZ];

	char	*group_file=NULL;
	char	*group_name=NULL;
	char	*group_type=NULL;
	char	*elm=NULL;
	char	*dup_elm=NULL;
	char	*oper=NULL;

	char	*elm_name=NULL;
	char	*elm_type=NULL;

	/* Check the input parameters */
	if ((parameters == NULL) || (info == NULL) || (msg == NULL))
	{
		sprintf(str,gettxt(":138",
			"Invalid parameters passed to xfsGroupInternal()\n"));
		add_to_buffer(msg,str);
		returnVal = 1;
	}
	else
	{
		*msg = NULL;
		*info = NULL;

		/* Parse the parameters string */
		for(line = strtok((char *)parameters, "\n");
		    line != NULL;
		    line = strtok(NULL, "\n")) {

			/* Extract the keyword and value pair */
			if(xfsmGetKeyValue(line, &key, &value)) {
				if (strcmp(key, XFS_GROUP_FILE_STR) == 0) {
					group_file = value;
				}
				else if(strcmp(key, XFS_GROUP_NAME_STR) == 0) {
					group_name = value;
				}
				else if(strcmp(key, XFS_GROUP_TYPE_STR) == 0) {
					group_type = value;
				}
				else if(strcmp(key, XFS_GROUP_ELM_STR) == 0) {
					elm = value;
				}
				else if(strcmp(key, XFS_GROUP_OPER_STR) == 0) {
					oper = value;
				}
			}
		}

		if (group_file == NULL) 
		{
			sprintf(str,gettxt(":139",
					"Group filename not specified\n"));
			add_to_buffer(msg,str);
			returnVal = 1;
		}
		else if (oper == NULL)
		{
			sprintf(str,gettxt(":140",
					"Group operation not specified\n"));
			add_to_buffer(msg,str);
			returnVal = 1;
		}
		else if (strcmp(oper,XFS_GROUP_LIST_OPER_STR)==0)
		{
			/* List all groups in file */
			returnVal = list_groups(group_file,info,msg);
		}
		else if (strcmp(oper,XFS_GROUP_CREATE_OPER_STR)==0)
		{
			/* Create a group */
			returnVal = create_group(group_file,group_name,
							group_type,msg);

		}
		else if (strcmp(oper,XFS_GROUP_DELETE_OPER_STR)==0)
		{
			/* Delete a group */
			returnVal = delete_group(group_file,group_name,
							group_type,msg);

		}
		else if (strcmp(oper,XFS_GROUP_ADD_ELM_STR)==0)
		{
			/* Add element to group */
			if (elm != NULL)
			{
				dup_elm = strdup(elm);
				ASSERT(dup_elm!=NULL);
				elm_name = strtok(dup_elm,",");
				elm_type = strtok(NULL,"\n");
			}
			returnVal = add_element_to_group(group_file,group_name,
					group_type,elm_name,elm_type,msg);

			if (dup_elm != NULL)
				free(dup_elm);

		}
		else if (strcmp(oper,XFS_GROUP_RM_ELM_STR)==0)
		{
			/* Remove element from group */
			if (elm != NULL)
			{
				dup_elm = strdup(elm);
				ASSERT(dup_elm!=NULL);
				elm_name = strtok(dup_elm,",");
				elm_type = strtok(NULL,"\n");
			}

			returnVal = delete_element_from_group(group_file,
				group_name,group_type,elm_name,elm_type,msg);

			if (dup_elm != NULL)
				free(dup_elm);
		}
		else
		{
			sprintf(str,gettxt(":141",
					"Unknown Group operation %s\n"),oper);
			add_to_buffer(msg,str);
			returnVal = 1;
		}
	}

	return(returnVal);
}
