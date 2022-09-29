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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/msgs/RCS/msend.c,v 1.1 1992/12/14 13:31:36 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* LINTLIBRARY */
# include	<errno.h>

# include	"lp.h"
# include	"msgs.h"

extern MESG	*lp_Md;
extern int	discon3_2_is_running;

/*
** msend() - SEND A MESSAGE VIA FIFOS
*/

#if	defined(__STDC__)
int msend ( char * msgbuf )
#else
int msend (msgbuf)
    char	*msgbuf;
#endif
{
    int		rval;

    do
    {
	if ((rval = mwrite(lp_Md, msgbuf)) < 0)
	{
	    /*
	    ** "mclose()" will try to say goodbye to the Spooler,
	    ** and that, of course, will fail. But we'll call
	    ** "mclose()" anyway, for the other cleanup it does.
	    */
	    if (errno == EPIPE)
	    {
		if (!discon3_2_is_running)
		    (void)mclose ();
		errno = EIDRM;
	    }
	}
    }
    while (rval < 0 && errno == EINTR);

    return(rval);
}
