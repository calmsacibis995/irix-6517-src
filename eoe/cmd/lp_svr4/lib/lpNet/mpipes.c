/*
 * |-----------------------------------------------------------|
 * | Copyright (c) 1991, 1990 MIPS Computer Systems, Inc.      |
 * | All Rights Reserved                                       |
 * |-----------------------------------------------------------|
 * |          Restricted Rights Legend                         |
 * | Use, duplication, or disclosure by the Government is      |
 * | subject to restrictions as set forth in                   |
 * | subparagraph (c)(1)(ii) of the Rights in Technical        |
 * | Data and Computer Software Clause of DFARS 252.227-7013.  |
 * |         MIPS Computer Systems, Inc.                       |
 * |         950 DeGuigne Avenue                               |
 * |         Sunnyvale, California 94088-3650, USA             |
 * |-----------------------------------------------------------|
 */
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/lpNet/RCS/mpipes.c,v 1.1 1992/12/14 13:30:21 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


/*=================================================================*/
/*
*/

#include	<unistd.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<fcntl.h>
#include	<errno.h>
#include	"mpipes.h"
#include	"errorMgmt.h"

/*=================================================================*/

/*=================================================================*/
/*
*/
int
MountPipe (fds, path_p)

int	fds [];
char	*path_p;
{
	/*----------------------------------------------------------*/
	/*
	*/
	static	char	FnName []	= "MountPipe";


	/*----------------------------------------------------------*/
	/*
	*/
	if (fds == NULL || path_p == NULL) {
		errno = EINVAL;
		return	-1;
	}
	fds [0] = open (path_p, O_RDWR|O_CREAT, 0600);

	if (fds [0] == -1) {
		TrapError (NonFatal, Unix, FnName, "open");
		return	-1;
	}
	if (close (fds [0]) == -1) {
		TrapError (NonFatal, Unix, FnName, "close");
		return	-1;
	}
	if (pipe (fds) == -1) {
		TrapError (NonFatal, Unix, FnName, "pipe");
		return	-1;
	}
	if (fattach (fds [1], path_p) == -1) {
		TrapError (NonFatal, Unix, FnName, "fattach");
		return	-1;
	}
/*
	if (close (fds [1]) == -1) {
		TrapError (NonFatal, Unix, FnName, "close");
		return	-1;
	}
*/
	

	return	fds [0];
}
/*=================================================================*/

/*=================================================================*/
/*
*/
int
UnmountPipe (fds, path_p)

int	fds [];
char	*path_p;
{
	/*----------------------------------------------------------*/
	/*
	*/
	static	char	FnName []	= "UmountPipe";


	/*----------------------------------------------------------*/
	/*
	*/
	if (fds == NULL || path_p == NULL) {
		errno = EINVAL;
		return	-1;
	}
	if (fdetach (path_p) == -1) {
		TrapError (Fatal, Unix, FnName, "fdetach");
		return	-1;
	}
	if (unlink (path_p) == -1) {
		TrapError (Fatal, Unix, FnName, "unlink");
		return	-1;
	}
	if (fds != NULL) {
		if (fds [0] != -1 && close (fds [0]) == -1) {
			TrapError (Fatal, Unix, FnName, "close");
			return	-1;
		}
		if (fds [1] != -1 && close (fds [1]) == -1) {
			TrapError (Fatal, Unix, FnName, "close");
			return	-1;
		}
	}
	

	return	0;
}
/*=================================================================*/
