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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/msgs/RCS/mdisconnect.c,v 1.1 1992/12/14 13:31:15 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* LINTLIBRARY */

# include	<stropts.h>
# include	<errno.h>
# include	<stdlib.h>

#include "lp.h"
#include "msgs.h"

#if	defined(__STDC__)
static void	disconnect3_2 ( MESG * );
#else
static void	disconnect3_2();
#endif

#if	defined(__STDC__)
int mdisconnect ( MESG * md )
#else
int mdisconnect (md)
    MESG	*md;
#endif
{
    int		retvalue = 0;
    void	(**fnp)();
    MQUE	*p;
    
    if (!md)
    {
	errno = ENXIO;
	return(-1);
    }

    switch(md->type)
    {
	case MD_CHILD:
	case MD_STREAM:
	case MD_BOUND:
	    if (md->writefd >= 0)
		(void) Close(md->writefd);
	    if (md->readfd >= 0)
		(void) Close(md->readfd);
	    break;

	case MD_USR_FIFO:
	case MD_SYS_FIFO:
	   disconnect3_2(md);
	   break;
    }

    if (md->on_discon)
    {
	for (fnp = md->on_discon; *fnp; fnp++)
	{
	    (*fnp)(md);
	    retvalue++;
	}
	Free(md->on_discon);
    }

    if (md->file)
	Free(md->file);
    
    if (md->mque)
    {
	while ((p = md->mque) != NULL)
	{
	    md->mque = p->next;
	    Free(p->dat->buf);
	    Free(p->dat);
	    Free(p);
	}
    }
    Free(md);

    return(retvalue);
}

int	discon3_2_is_running = 0;

#if	defined(__STDC__)
static void disconnect3_2 ( MESG * md )
#else
static void disconnect3_2 (md)
    MESG	*md;
#endif
{
    char	*msgbuf = 0;
    int		size;

    discon3_2_is_running = 1;

    if (md->writefd != -1)
    {
	size = putmessage((char *)0, S_GOODBYE);
	if ((msgbuf = (char *)Malloc((unsigned)size)))
	{
	    (void)putmessage (msgbuf, S_GOODBYE);
	    (void)msend (msgbuf);
	    Free (msgbuf);
	}

	(void) Close (md->writefd);
    }

    if (md->readfd != -1)
	(void) Close (md->readfd);

    if (md->file)
    {
	(void) Unlink (md->file);
	Free (md->file);
    }	

    discon3_2_is_running = 0;
}
