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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/lpNet/RCS/memdup.c,v 1.1 1992/12/14 13:30:18 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/



/*==================================================================*/
/*
*/
#include	<stdlib.h>
#include	<memory.h>
#include	<errno.h>
#include	"memdup.h"

#ifndef	NULL
#define	NULL	0
#endif

extern	int	errno;
/*==================================================================*/

/*==================================================================*/
/*
*/
void *
memdup (memoryp, length)

void	*memoryp;
int	length;
{
	/*----------------------------------------------------------*/
	/*
	*/
	void	*newp;

	/*----------------------------------------------------------*/
	/*
	*/
	if (length <= 0)
	{
		errno = EINVAL;
		return	NULL;
	}
	newp = (void *) malloc (length);

	if (newp == NULL)
	{
		errno = ENOMEM;
		return	NULL;
	}
	(void)	memcpy (newp, memoryp, length);


	return	newp;
}
/*==================================================================*/
