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

#include <stdio.h>
#include <errno.h>


/*
 * Print system call error message and terminate
 */
void
syserr(char* msg)
{
	extern int errno,sys_nerr;
	extern	char *sys_errlist[];

	fprintf(stderr,"ERROR: %s (%d",msg,errno);
	if ((errno>0) && (errno<sys_nerr))
		fprintf(stderr,":%s)\n",sys_errlist[errno]);
	else
		fprintf(stderr,")\n");
	exit(1);
}

/*
 * Print error message and terminate
 */
void
fatal(char* msg)
{
	fprintf(stderr,"ERROR:%s\n",msg);
	exit(1);
}

/*
 * Print system call error message
 */
void
show_err(char* msg)
{
	extern int errno,sys_nerr;
	extern	char *sys_errlist[];

	fprintf(stderr,"ERROR: %s (%d",msg,errno);
	if ((errno>0) && (errno<sys_nerr))
		fprintf(stderr,":%s)\n",sys_errlist[errno]);
	else
		fprintf(stderr,")\n");
}
