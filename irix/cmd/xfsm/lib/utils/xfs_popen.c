/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1988, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident  "p_open.c: $Revision: 1.2 $"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <wait.h>
#include <paths.h>

#define	tst(a,b) (*mode == 'r'? (b) : (a))
#define	RDR	0
#define	WTR	1

#define BIN_SH _PATH_BSHELL
#define SH "sh"
#define SHFLG "-c"
static int *p_open_pid;

/*
 *	xfs_popen()
 *	Opens a pipe to establish connection with a child process
 *	that executes the command. 
 *
 *	If mode = "r", the child writes to the pipe and the parent
 *	reads from the pipe
 *	Vice versa if mode = "w"
 *
 * 	If mode = "r", attach_stream specifies the stream associated
 *	with the child's end of the pipe
 *
 *	attach_stream = 0: point stdout and stderr to the child's end of the pipe
 *	attach_stream = 1: point stdout to the child's end of the pipe	
 *	attach_stream = 2: point stderr to the child's end of the pipe
 */

FILE *
xfs_popen(const char* cmd,const char* mode, int attach_stream)
{
	int	p[2];
	register int *poptr;
	register int myside, yourside, pid;

	if ((p_open_pid == NULL) && 
		((p_open_pid = (int*)calloc(256,sizeof(int))) == NULL))
	{
		return(NULL);
	}

	if(pipe(p) < 0)
	{
		return(NULL);
	}

	myside = tst(p[WTR], p[RDR]);
	yourside = tst(p[RDR], p[WTR]);

	if((pid = fork()) == 0) {
		/* myside and yourside reverse roles in child */
		int	stdio;

		/* close all pipes from other popen's */
		for (poptr = p_open_pid; poptr < p_open_pid+256; poptr++) 
		{
			if(*poptr)
				close(poptr - p_open_pid);
		}

		stdio = tst(0, 1);
		(void) close(myside);
		/* if child is to write to the pipe */	
		if (stdio)
		{
			switch(attach_stream)
			{
				case 0:
					(void) close(1);
					(void) fcntl(yourside, F_DUPFD, 1);
	
					(void) close(2);
					(void) fcntl(yourside, F_DUPFD, 2);
					(void) close(yourside);
					break;
					
				case 1:
				case 2:
					if(yourside != attach_stream) 
					{
						(void) close(attach_stream);
						(void) fcntl(yourside, F_DUPFD, attach_stream);
						(void) close(yourside);
					}
					break;
					
				default:
					return(NULL);
			}	
		}
		else
		{
			if(yourside != stdio) 
			{
				(void) close(stdio);
				(void) fcntl(yourside, F_DUPFD, stdio);
				(void) close(yourside);
			}
		}
		(void) execl(BIN_SH, SH, SHFLG, cmd, (char *)0);
		_exit(1);
	}

	if (pid == -1)
	{
		return(NULL);
	}

	p_open_pid[myside] = pid;
	(void) close(yourside);
	return(fdopen(myside,mode));
}

/*
 *	xfs_pclose()
 *	Waits for completion of execution of the child process,
 *	before closing the pipe. The status returned by waitpid()
 *	is returned. A zero return value indicates success. For
 *	interpreting an error refer to waitpid(). 
 */

int
xfs_pclose(FILE *ptr)
{
	register int f;
	pid_t r, p;
	int status = 0;

	if (!p_open_pid)
		return -1;

	f = fileno(ptr);
	p = p_open_pid[f];

	while (waitpid(p, &status, 0) < 0)
	{
		if (errno != EINTR) 
		{
			status = -1;
			break;
		}
	}

	/* close the pipe */
	(void) fclose(ptr);

	/* mark this pipe closed */
	p_open_pid[f] = 0;

	return(status);
}
