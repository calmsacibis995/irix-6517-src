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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pfmt.h>
#include <assert.h>
#include <sys/debug.h>
#include "xfs_utils.h"
#include "group.h"
#include "xfs_group_key.h"

/*
 *	delete_group_list()
 *	The memory allocated for the group list is released by traversing
 *	the list.
 */

void
delete_group_list(group_entry_list_t **grp_list)
{
	group_entry_list_t *group_list_elm;
	group_entry_t *group;

	/* Traverse the group list */
	group_list_elm = *grp_list;
	while (group_list_elm != NULL)
	{
		/* Delete the group */
		group = group_list_elm->group;
		if (group != NULL)
		{
			if (group->group_name != NULL)
				free(group->group_name);
			if (group->group_type != NULL)
				free(group->group_type);
			if (group->elm_list != NULL)
				free(group->elm_list);

			free(group);
		}

		/* Delete the list element */
		*grp_list = group_list_elm->next;
		free(group_list_elm);
		group_list_elm = *grp_list;
	}
}


/*
 *	xlv_read_group_file()
 *	This routine reads the group information from the specified
 *	file into a list. This list could then be processed for 
 *	finding if a specific group exists, adding new groups and
 *	similar operations. The file must have entries in the format
 *	group_name:group_type{element1:type,element2:type}
 *	Return value :	0 success
 *			1 failure
 */

int
xlv_read_group_file(const char* filename, group_entry_list_t **grp_list,char** msg)
{
	FILE	*fp;
	int 	errValue = 0;
	char	linebuf[GRP_LINESIZE];
	char	*token;
	group_entry_t *group;
	group_entry_list_t *group_list_elm;
	short 	group_complete_flag;
	char	*group_name=NULL;
	char	*group_type=NULL;
	char	str[BUFSIZ];
	
	/* Open the group file */
	if ((fp = fopen(filename,"r")) == NULL)
	{
		/* File does not exist, return error */
		sprintf(str,gettxt(":127","Error opening file %s\n"),filename);
		add_to_buffer(msg,str);
		errValue = 1;
	}
	else
	{
		while ((fgets(linebuf,GRP_LINESIZE,fp)!=NULL) && (!errValue))
		{
			if ((strchr(linebuf,'{') == NULL) ||
				((group_name=strtok(linebuf,":")) == NULL) ||
				((group_type=strtok(NULL,"{")) == NULL))
			{
				sprintf(str,gettxt(":129",
					"Incorrect group file format\n"));
				add_to_buffer(msg,str);
				errValue = 1;
			}
			else
			{
				/* Allocate an entry for the group and 
				   add it to the group list */
				group = (group_entry_t*) 
					malloc(sizeof(group_entry_t));
				ASSERT(group!=NULL);

				group->group_name = NULL;
				group->group_type = NULL;
				group->group_count = 0;
				group->elm_list = NULL;

				group_list_elm = (group_entry_list_t*) 
					malloc(sizeof(group_entry_list_t));
				ASSERT(group_list_elm!=NULL);

				group_list_elm->group = group;
				group_list_elm->next = NULL;
				
				if (*grp_list == NULL)
				{
					*grp_list = group_list_elm;
				}
				else
				{
					group_list_elm->next = *grp_list;
					*grp_list = group_list_elm;
				} 

				/* Store the group name,type and element list */
				group->group_name = strdup(group_name);
				ASSERT(group->group_name!=NULL);
				
				group->group_type = strdup(group_type);
				ASSERT(group->group_type!=NULL);

				/* Check if there are elements */
				token = strtok(NULL,":");
				if ((token!=NULL)&&(strncmp(token,"}",1)==0))
				{
					group->group_count = 0;
					group->elm_list = strdup("{}");
					ASSERT(group->elm_list!=NULL);
				}
				else
				{
					group->elm_list = strdup("{");
					ASSERT(group->elm_list!=NULL);

					group_complete_flag = 0;

					while (!group_complete_flag)
					{
						while (token!=NULL) 
						{

							add_to_buffer(
								&group->elm_list,
								token);
							token=strtok(NULL,",\n");
							if (token == NULL)
							{
								sprintf(str,gettxt(":128","Type not specified for element in group %s\n"),group->group_name);
								add_to_buffer(msg,str);
								errValue = 1;
							}
							else
							{
								/* Add the type*/
								sprintf(str,
									":%s,",
									token);

								add_to_buffer(
								&group->elm_list,
									str);
								group->group_count++;

								if (strchr(token,'}')!=NULL)
								{
									group_complete_flag = 1;
									break;
								}
								else
								{
									/* Get the next token */
									token = strtok(NULL,":\n");
								}
							}
						}
						
						/* If group is not complete */
						/* Read the elements in the next line */
						if (!group_complete_flag)
						{
							memset(linebuf,'\0',
								GRP_LINESIZE);
							fgets(linebuf,
								GRP_LINESIZE,fp);
							token=strtok(
								linebuf,":");
						}
					}
					strncpy(strrchr(group->elm_list,','),"\0",1);
				}
			}
			memset(linebuf,'\0',GRP_LINESIZE);
		}
		fclose(fp);
	}	
	
	return(errValue);
}

/*
 *	xlv_update_group_file()
 *	Writes the group list to the specified file in the format
 *	group_name:group_type{element1:type,element2:type}
 *	There is a seperate line for each group. 
 *	Return value :	0 success
 *			1 failure
 */

int
xlv_update_group_file(const char* filename,group_entry_list_t *grp_list,char** msg)
{
	FILE	*fp;
	int	retValue = 0;
	group_entry_list_t *group_list_elm=NULL;
	group_entry_t	*group=NULL;
	char	str[BUFSIZ];

	/* Open the group file */
	if ((fp = fopen(filename,"w")) == NULL)
	{
		/* Unable to open file */
		sprintf(str,gettxt(":127","Error opening file %s\n"),filename);
		add_to_buffer(msg,str);
		retValue = 1;
	}
	else if (grp_list != NULL)
	{
		group_list_elm = grp_list;
		while (group_list_elm != NULL)
		{
			group = group_list_elm->group;
			if ((group != NULL) && 
				(group->group_name != NULL) &&
				(group->group_type != NULL) && 
				(group->elm_list != NULL))
			{
				fprintf(fp,"%s:%s%s\n", group->group_name,
							group->group_type,
							group->elm_list);
			}
			group_list_elm = group_list_elm->next;
		}
		fclose(fp);
	}
	return(retValue);
}
		

/*
 *	create_group()
 *	A new group is created, given the group name and its type.
 *	The list of known groups is checked to avoid duplicate entry. 
 *	ReturnValue :	0 success
 *			1 failure
 */

int
create_group(const char* filename,const char* group_name,const char* group_type,char** msg)
{
	group_entry_list_t *group_list=NULL; 
	group_entry_list_t *group_list_elm;
	group_entry_t	*group;
	int	returnVal = 0;
	char	str[BUFSIZ];

       	if ((returnVal = xlv_read_group_file(filename,&group_list,msg))!=0)
	{
		returnVal = 1;
	}
	/* Check for invalid group name & type */
	else if ((group_name == NULL) || (group_type == NULL))
	{
		sprintf(str,gettxt(":130",
		"Invalid group %s specified for creation type %s\n"),
			group_name,group_type);
		add_to_buffer(msg,str);
		returnVal = 1;
	}
	else
	{
		/* Check for a group of same name & type in list */
		group_list_elm = group_list;
		while (group_list_elm != NULL)
		{
			if ((group_list_elm->group->group_name!=NULL) &&
		(strcmp(group_list_elm->group->group_name,group_name)==0) &&
			(group_list_elm->group->group_type!=NULL) &&
		(strcmp(group_list_elm->group->group_type,group_type)==0)) 
			{
				/* Group already exists */
				sprintf(str,
				gettxt(":137","Group %s Type %s exists\n"),
						group_name, group_type);
				add_to_buffer(msg,str);
				returnVal = 1;
				break;
			}
			group_list_elm = group_list_elm->next;
		}

		if (returnVal == 0)
		{
			/* Create the empty group */
			group = (group_entry_t*) malloc(sizeof(group_entry_t));
			ASSERT(group!=NULL);

			group->group_name = strdup(group_name);
			ASSERT(group->group_name!=NULL);
			group->group_type = strdup(group_type);
			ASSERT(group->group_type!=NULL);
			group->elm_list = strdup("{}");
			ASSERT(group->elm_list!=NULL);
			group->group_count = 0;

			/* Add the group to the list */
			group_list_elm = (group_entry_list_t*) 
					malloc(sizeof(group_entry_list_t));
			ASSERT(group_list_elm!=NULL);

			group_list_elm->group = group;
			group_list_elm->next = group_list;
			group_list = group_list_elm;

			returnVal = xlv_update_group_file(filename,
							group_list,msg);
		}
	}

	/* Release the memory allocated for the group list */
	if (group_list != NULL)
	{
		delete_group_list(&group_list);
	}

	return(returnVal);
}

/*
 *	delete_group()
 *	The group is located in the list of groups and deleted. 
 *	A value of 0 is returned on success and -1 on failure to delete.
 */

int
delete_group(const char* filename,const char *name,const char *type,char** msg)
{
	group_entry_list_t *group_list=NULL; 
	group_entry_list_t *group_list_elm, *prev_group_list_elm;
	int returnVal = 1;
	int found=1;
	char	str[BUFSIZ];

       	if ((returnVal = xlv_read_group_file(filename,&group_list,msg))!=0)
	{
		returnVal = 1;
	}
	else if ((name != NULL) && (type != NULL))
	{
		found = 1;
		group_list_elm = group_list;
		prev_group_list_elm = NULL;
		while (group_list_elm != NULL)
		{
			if ((group_list_elm->group->group_name!=NULL) &&
			(strcmp(group_list_elm->group->group_name,name)==0)&&
				(group_list_elm->group->group_type!=NULL) &&
			(strcmp(group_list_elm->group->group_type,type)==0)) 
			{
				/* Group exists */
				found = 0;

				if (prev_group_list_elm == NULL)
				{
					group_list = group_list_elm->next;
				}
				else
				{
					prev_group_list_elm->next =
							group_list_elm->next;
				}				

				/* Release the memory allocated for the group*/
				if (group_list_elm->group->group_name!=NULL)
				{
					free(group_list_elm->group->group_name);
				}

				if (group_list_elm->group->group_type!=NULL)
				{
					free(group_list_elm->group->group_type);
				}

				if (group_list_elm->group->elm_list!=NULL)
				{
					free(group_list_elm->group->elm_list);
				}

				free(group_list_elm->group);
				free(group_list_elm);

				returnVal = xlv_update_group_file(filename,
							group_list,msg);

				break;
			}
			prev_group_list_elm = group_list_elm;
			group_list_elm = group_list_elm->next;
		}

		if (found == 1)
		{
			sprintf(str,gettxt(":132",
				"Group %s of type %s does not exist\n"),
				name,type);
			add_to_buffer(msg,str);
			returnVal = 1;
		}
	}
	else
	{
		sprintf(str,gettxt(":131",
			"Invalid group %s specified for deletion type %s\n"),
					name,type);
		add_to_buffer(msg,str);
		returnVal = 1;
	}	

	/* Release the memory allocated for the group list */
	if (group_list != NULL)
	{
		delete_group_list(&group_list);
	}

	return(returnVal);
}


/*
 *	find_group()
 *	For the given group name & type, lookup the group list. Return the 
 *	reference to the group, if it exists or else NULL. 
 */

group_entry_t*
find_group(group_entry_list_t *group_list,const char* name,const char* type)
{
	group_entry_list_t *group_list_elm;
	group_entry_t *group = NULL;

	group_list_elm = group_list;
	while (group_list_elm != NULL) 
	{
		if ((group_list_elm->group->group_name!=NULL) &&
			(strcmp(group_list_elm->group->group_name,name)==0) &&
			(group_list_elm->group->group_type!=NULL) &&
			(strcmp(group_list_elm->group->group_type,type)==0))
		{
			group = group_list_elm->group;
			break;
		}
		group_list_elm = group_list_elm->next;
	}

	return(group);
}
 
/*
 *	is_element_in_group()
 *	Verifies the existence of the element in the group by traversing all
 *	the elements of the group. Returns 0 if element exists or else 1.
 */

int
is_element_in_group(const group_entry_t *group,const char *elm_name,const char* elm_type)
{
	char *token;
	char *type;
	int returnVal = 1;
	char *elm_list;

	if ((group == NULL) || (elm_name == NULL) || (elm_type == NULL))
	{
		returnVal = 1;
	}
	else
	{
		/* Make a copy of the element list for processing */
		elm_list = strdup(group->elm_list);
		ASSERT(elm_list!=NULL);

		token = strtok(elm_list,"{:}");
		while (token!=NULL)
		{
			type = strtok(NULL,":,}");
			if ((strcmp(token,elm_name)==0) && (type != NULL) 
				&& (strcmp(type,elm_type)==0))
			{
				returnVal = 0;
				break;
			}
			/* Get the next element */
			token = strtok(NULL,":,}");
		}

		/* Release memory for elm_list */
		if (elm_list != NULL)
		{
			free(elm_list);
		}
	}

	return(returnVal);
}		


/*
 *	add_element_to_group()
 *	Adds an element to the specified group, if does not already exist.
 *	Return Value:	0 success
 *			1 failure
 */

int
add_element_to_group(const char* filename,const char* group_name,const char* group_type,const char* elm_name,const char* elm_type,char** msg)
{
	group_entry_list_t *group_list=NULL; 
	group_entry_t	*group=NULL;
	int	returnVal = 0;
	char	str[BUFSIZ];
	
       	if ((returnVal = xlv_read_group_file(filename,&group_list,msg))!=0)
	{
		returnVal = 1;
	}
	/* Check for invalid parameters */
	else if ((group_list == NULL) || (group_name == NULL) || 
		(group_type == NULL) || (elm_name == NULL) || 
		(elm_type == NULL))
	{
		sprintf(str,gettxt(":133",
		"Invalid parameters passed to add_element_to_group()\n"));
		add_to_buffer(msg,str);
		returnVal = 1;
	}
	/* Fetch the group */
	else if ((group=find_group(group_list,group_name,group_type))==NULL)
	{
		sprintf(str,gettxt(":132",
				"Group %s of type %s does not exist\n"),
				group_name,group_type);
		add_to_buffer(msg,str);
		returnVal = 1;
	}
	/* Check for element in group */
	else if (is_element_in_group((const group_entry_t*)group,elm_name,elm_type)==0)
	{
		sprintf(str,gettxt(":134",
		"Element %s Type %s already exists in group %s of type %s\n"),
				elm_name,elm_type,group_name,group_type);
		add_to_buffer(msg,str);
		returnVal = 1;
	}
	else
	{
		/* Add the element to the list */
		if (group->elm_list!=NULL) 
		{
			if (strcmp(group->elm_list,"{}")==0)
			{				
				strncpy(strrchr(group->elm_list,'}'),"\0",1);
			}
			else
			{
				strncpy(strrchr(group->elm_list,'}'),",",1);
			}

			sprintf(str,"%s:%s}",elm_name,elm_type);
			add_to_buffer(&group->elm_list,str);
			group->group_count++;

			returnVal = xlv_update_group_file(filename,
						group_list,msg);
		}
		else
		{
			returnVal = 1;
		}
	}

	/* Release the memory allocated for the group list */
	if (group_list != NULL)
	{
		delete_group_list(&group_list);
	}

	return(returnVal);
}

/*
 *	delete_element_from_group()
 *	Deletes element from the group if it exists.
 *	ReturnValue: 	0 success
 *			1 failure
 */

int
delete_element_from_group(const char* filename,const char *group_name,const char* group_type,const char *elm_name,const char *elm_type,char** msg)
{
	char	*name;
	char	*type;
	int	returnVal = 0;
	int	found = 1;
	char	*new_elm_list=NULL;
	group_entry_t	*group=NULL;
	group_entry_list_t *group_list=NULL; 
	char	str[BUFSIZ];

       	if ((returnVal = xlv_read_group_file(filename,&group_list,msg))!=0)
	{
		returnVal = 1;
	}
	else if ((group_list == NULL) || (group_name == NULL) ||
		(group_type == NULL) || (elm_name == NULL) 
		|| (elm_type == NULL))
	{
		sprintf(str,gettxt(":135",
	"Invalid parameters passed to delete_element_from_group()\n"));
		add_to_buffer(msg,str);
		returnVal = 1;
	}
	/* Fetch the group */
	else if ((group = find_group(group_list,group_name,group_type)) == NULL)
	{
		sprintf(str,gettxt(":132",
			"Group %s of type %s does not exist\n"),
			group_name,group_type);
		add_to_buffer(msg,str);
		returnVal = 1;
	}
	else
	{
		/* Make a copy of the group element list */
		new_elm_list = strdup(group->elm_list);
		ASSERT(new_elm_list!=NULL);
		memset(new_elm_list,'\0',strlen(new_elm_list));
		strcat(new_elm_list,"{");

		/* Traverse the group element list to locate the element */
		/* Copy the other elements to the new element list */
		name = strtok(group->elm_list,"{:");
		type = strtok(NULL,",}");	
		while ((name != NULL) && (type != NULL))
		{
			if ((strcmp(name,elm_name)==0) && 
				(strcmp(type,elm_type)==0))
			{
				/* Element found */
				group->group_count--;
				found = 0;
			}
			else
			{
				strcat(new_elm_list,name);
				strcat(new_elm_list,":");
				strcat(new_elm_list,type);
				strcat(new_elm_list,",");
			}

			name = strtok(NULL,":}");
			type = strtok(NULL,",}");	
		}

		/* Terminate the new element list */
		if (strcmp(new_elm_list,"{")==0)
		{
			strcat(new_elm_list,"}");
		}
		else
		{
			strncpy(strrchr(new_elm_list,','),"}",1);
		}

		if (found==0)
		{
			/*Overwrite group element list with the new list*/
			strcpy(group->elm_list,new_elm_list);
			returnVal = xlv_update_group_file(filename,
						group_list,msg);
		}
		else
		{
			sprintf(str,gettxt(":136",
		"Element %s Type %s does not exist in group %s of type %s\n"),
				elm_name,elm_type,group_name,group_type);
			add_to_buffer(msg,str);
			returnVal = 1;
		}

		/* release memory for new list */
		if (new_elm_list != NULL)
		{
			free(new_elm_list);
		}
	}


	/* Release the memory allocated for the group list */
	if (group_list != NULL)
	{
		delete_group_list(&group_list);
	}

	return(returnVal);	
}

/*
 *	list_group_elements()
 *	Lists all the elements of a group, given the element list.
 */

void
list_group_elements(char* elm_list,char** info)
{
	char*	elm_name;
	char*	elm_type;
	char	str[BUFSIZ];

	elm_name = strtok(elm_list,"{:}");
	while (elm_name != NULL)
	{
		elm_type = strtok(NULL,":,}");
		if ((elm_name != NULL) && (elm_type != NULL))
		{
			sprintf(str,"%s:%s,%s\n",
				XFS_GROUP_ELM_STR,elm_name,elm_type);
			add_to_buffer(info,str);
		}

		/* Get the next element */
		elm_name = strtok(NULL,":,}");
	}
}

/*
 *	list_groups()
 *	Lists all the groups in the specified file. Each group's name, type
 *	and the elements are displayed.
 */

int
list_groups(char* filename,char** info,char** msg)
{
	group_entry_list_t *group_list=NULL;
 	group_entry_list_t *group_list_elm;
        group_entry_t *group;
	int returnValue = 0;
	char	str[BUFSIZ];

        returnValue = xlv_read_group_file(filename,&group_list,msg);

	if (returnValue == 0)
	{
		group_list_elm = group_list;
		while (group_list_elm != NULL)
		{
			group = group_list_elm->group;
			if (group != NULL)
			{
				sprintf(str,"%s:%s\n%s:%s\n",
				XFS_GROUP_NAME_STR,group->group_name,
				XFS_GROUP_TYPE_STR,group->group_type);
				add_to_buffer(info,str);

				sprintf(str,"%s:0\n",XFS_GROUP_BEGIN_STR);
				add_to_buffer(info,str);
				list_group_elements(group->elm_list,info);
				sprintf(str,"%s:0\n",XFS_GROUP_END_STR);
				add_to_buffer(info,str);
			}
			group_list_elm = group_list_elm->next;
		}
	}

	/* Release the memory allocated for the group list */
	if (group_list != NULL)
	{
		delete_group_list(&group_list);
	}

	return(returnValue);
}
