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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/postscript/RCS/tempnam.c,v 1.1 1992/12/14 13:33:06 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#include <stdio.h>
#include <errno.h>

#if defined(V9) || defined(BSD4_2)
char *tempnam(dir, pfx)
char *dir, *pfx;
{
	int pid;
	unsigned int len;
	char *tnm, *malloc();
	static int seq = 0;

	pid = getpid();
	len = strlen(dir) + strlen(pfx) + 10;
	if ((tnm = malloc(len)) != NULL) {
		sprintf(tnm, "%s", dir);
		if (access(tnm, 7) == -1)
			return(NULL);
		do {
			sprintf(tnm, "%s/%s%d%d", dir, pfx, pid, seq++);
			errno = 0;
			if (access(tnm, 7) == -1)
				if (errno == ENOENT)
					return(tnm);
		} while (1);
	}
	return(tnm);
}
#endif
