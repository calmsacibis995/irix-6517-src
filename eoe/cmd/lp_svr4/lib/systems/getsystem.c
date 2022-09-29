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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/systems/RCS/getsystem.c,v 1.1 1992/12/14 13:34:29 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* LINTLIBRARY */

# include	<stdio.h>
# include	<string.h>
# include	<errno.h>
# include	<stdlib.h>

# include	"lp.h"
# include	"systems.h"

# define	SEPCHARS	":\n"

/**
 ** getsystem() - EXTRACT SYSTEM STRUCTURE FROM DISK FILE
 **/

#if	defined(__STDC__)
SYSTEM * getsystem ( const char * name )
#else
SYSTEM * getsystem ( name )
char	*name;
#endif
{
    static FILE		*fp;
    static int		all = 0;
    static SYSTEM	sysbuf;
    char		*cp;
    char		buf[BUFSIZ];

    if (STREQU(name, NAME_ALL))
    {
	if (all == 0)
	{
	    all = 1;
	    if ((fp = open_lpfile(Lp_NetData, "r", MODE_READ)) == NULL)
		return(NULL);
	}
    }
    else
	if (all == 1)
	{
	    all = 0;
	    (void) rewind(fp);
	}
	else
	    if ((fp = open_lpfile(Lp_NetData, "r", MODE_READ)) == NULL)
		return(NULL);

    (void)	memset (&sysbuf, 0, sizeof (sysbuf));

    while(fgets(buf, BUFSIZ, fp) != NULL)
    {
	if (*buf == '#')
	    continue;

	if ((cp = strtok(buf, SEPCHARS)) == NULL)
	    continue;

	if (!all && STREQU(name, cp) == 0)
	    continue;

	sysbuf.name = Strdup(cp);

	if ((cp = strtok(NULL, SEPCHARS)) == NULL)
	{
	    freesystem(&sysbuf);
	    all = 0;
	    (void) close_lpfile(fp);
	    errno = EBADF;
	    return(NULL);
	}

	sysbuf.passwd = NULL;

	if ((cp = strtok(NULL, SEPCHARS)) == NULL)
	{
	    freesystem(&sysbuf);
	    all = 0;
	    (void) close_lpfile(fp);
	    errno = EBADF;
	    return(NULL);
	}

	sysbuf.reserved1 = NULL;

	if ((cp = strtok(NULL, SEPCHARS)) == NULL)
	{
	    freesystem(&sysbuf);
	    all = 0;
	    (void) close_lpfile(fp);
	    errno = EBADF;
	    return(NULL);
	}

	if (STREQU(NAME_S5PROTO, cp))
	    sysbuf.protocol = S5_PROTO;
	else
	    if (STREQU(NAME_BSDPROTO, cp))
		sysbuf.protocol = BSD_PROTO;

	if ((cp = strtok(NULL, SEPCHARS)) == NULL)
	{
	    freesystem(&sysbuf);
	    errno = EBADF;
	    all = 0;
	    (void) close_lpfile(fp);
	    return(NULL);
	}

	sysbuf.reserved2 = NULL;

	if ((cp = strtok(NULL, SEPCHARS)) == NULL)
	{
	    freesystem(&sysbuf);
	    errno = EBADF;
	    all = 0;
	    (void) close_lpfile(fp);
	    return(NULL);
	}
	if (*cp == 'n')
	    sysbuf.timeout = -1;
	else
	    sysbuf.timeout = atoi(cp);

	if ((cp = strtok(NULL, SEPCHARS)) == NULL)
	{
	    freesystem(&sysbuf);
	    all = 0;
	    (void) close_lpfile(fp);
	    errno = EBADF;
	    return(NULL);
	}

	if (*cp == 'n')
	    sysbuf.retry = -1;
	else
	    sysbuf.retry = atoi(cp);

	if ((cp = strtok(NULL, SEPCHARS)) == NULL)
	{
	    freesystem(&sysbuf);
	    all = 0;
	    (void) close_lpfile(fp);
	    errno = EBADF;
	    return(NULL);
	}

	sysbuf.reserved3 = NULL;

	if ((cp = strtok(NULL, SEPCHARS)) == NULL)
	{
	    freesystem(&sysbuf);
	    all = 0;
	    (void) close_lpfile(fp);
	    errno = EBADF;
	    return(NULL);
	}

	sysbuf.reserved4 = NULL;

	if ((cp = strtok(NULL, SEPCHARS)) == NULL)
		sysbuf.comment = NULL;
	else	
		sysbuf.comment = Strdup(cp);

	if (all == 0)
	    (void) close_lpfile(fp);

	return(&sysbuf);
    }

    (void) close_lpfile(fp);
    all = 0;
/*	No reason to set this value -suresh
    errno = ENOENT;


*/
    return(NULL);
}
