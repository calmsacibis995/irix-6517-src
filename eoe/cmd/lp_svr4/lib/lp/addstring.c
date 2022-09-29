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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/lp/RCS/addstring.c,v 1.1 1992/12/14 13:27:44 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#include "string.h"
#include "errno.h"
#include "stdlib.h"

#include "lp.h"

/**
 ** addstring() - ADD ONE STRING TO ANOTHER, ALLOCATING SPACE AS NEEDED
 **/

int
#if	defined(__STDC__)
addstring (
	char **			dst,
	char *			src
)
#else
addstring (dst, src)
	char			**dst;
	char			*src;
#endif
{
	size_t			len;

	if (!dst || !src) {
		errno = EINVAL;
		return (-1);
	}

	len = strlen(src) + 1;
    
	if (*dst) {
		if (!(*dst = Realloc(*dst, strlen(*dst) + len))) {
			errno = ENOMEM;
			return (-1);
		}
	} else {
		if (!(*dst = Malloc(len))) {
			errno = ENOMEM;
			return (-1);
		}
		(*dst)[0] = '\0';
	}

	(void) strcat(*dst, src);
	return (0);
}
