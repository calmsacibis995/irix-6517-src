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

#ident  "xfs_addbuf.c: $Revision: 1.2 $"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/debug.h>


/*
 *	add_to_buffer()
 *	Appends the string to the buffer by allocating memory.
 *	The memory management is performed for the buffer dynamically.
 */

void
add_to_buffer(char** buffer,char* str)
{
	if(str == NULL || *str == NULL) {
		return;
	}

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
