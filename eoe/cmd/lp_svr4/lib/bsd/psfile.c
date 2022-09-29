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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/bsd/RCS/psfile.c,v 1.1 1992/12/14 13:24:56 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#include <fcntl.h>

#define	PSCOM	"%!"

#if defined(__STDC__)
psfile(char * fname)
#else
psfile(fname)
char	*fname;
#endif
{
	int		fd;
	register int	ret = 0;
	char		buf[sizeof(PSCOM)-1];

	if ((fd = open(fname, O_RDONLY)) >= 0 &&
    	    read(fd, buf, sizeof(buf)) == sizeof(buf) &&
    	    strncmp(buf, PSCOM, sizeof(buf)) == 0)
			ret++;
	(void)close(fd);
	return(ret);
}
