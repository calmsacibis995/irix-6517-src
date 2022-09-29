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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/msgs/RCS/mgetputm.c,v 1.1 1992/12/14 13:31:18 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


# include	<unistd.h>
# include	<errno.h>
# include	<stdlib.h>

#if	defined(__STDC__)
# include	<stdarg.h>
#else
# include	<varargs.h>
#endif

# include	"lp.h"
# include	"msgs.h"


/*
**	Size and pointer for mgetm()
*/
static int	MBGSize = 0;
static char *	MBG = NULL;

/*
**	Size and pointer for mputm()
*/
static int	MBPSize = 0;
static char *	MBP = NULL;

int		peek3_2();

#if	defined(__STDC__)
int mgetm ( MESG * md, int type, ... )
#else
int mgetm (md, type, va_alist)
    MESG	*md;
    int		type;
    va_dcl
#endif
{
    va_list	vp;
    int		ret;
    int		needsize;

#if	defined(__STDC__)
    va_start(vp, type);
#else
    va_start(vp);
#endif

    needsize = mpeek(md);
    if (needsize <=0 || needsize > MSGMAX)
	needsize = MSGMAX;
    if (needsize > MBGSize)
    {
	if (MBG)
	    Free(MBG);
	if ((MBG = (char *)Malloc(needsize)) == NULL)
	{
	    MBGSize = 0;
	    MBG = NULL;
	    errno = ENOMEM;
	    return(-1);
	}
	MBGSize = needsize;
    }
    if (mread(md, MBG, MBGSize) < 0)
	return(-1);

    ret = _getmessage(MBG, type, vp);

    va_end(vp);

    return(ret);
}

#if	defined(__STDC__)
int mputm ( MESG * md, int type, ... )
#else
int mputm (md, type, va_alist)
    MESG	*md;
    int		type;
    va_dcl
#endif
{
    va_list	vp;
    int		needsize;

#if	defined(__STDC__)
    va_start(vp, type);
#else
    va_start(vp);
#endif

    if ((needsize = _putmessage(NULL, type, vp)) <= 0)
	return(-1);
    
    if (needsize > MBPSize)
    {
	if (MBP)
	    Free(MBP);
	if ((MBP = (char *)Malloc(needsize)) == NULL)
	{
	    MBPSize = 0;
	    MBP = NULL;
	    errno = ENOMEM;
	    return(-1);
	}
	MBPSize = needsize;
    }

    if ((needsize = _putmessage(MBP, type, vp)) <= 0)
	return(-1);
    
    va_end(vp);

    return(mwrite(md, MBP));
}

#if	defined(__STDC__)
void __mbfree ( void )
#else
void __mbfree ()
#endif
{
    MBGSize = MBPSize = 0;
    if (MBG)
	Free (MBG);
    if (MBP)
	Free (MBP);
    MBG = MBP = NULL;
}

#if	defined(__STDC__)
short mpeek ( MESG * md )
#else
short mpeek (md)
    MESG	*md;
#endif
{
    int size;

    if (md->type == MD_USR_FIFO || md->type == MD_SYS_FIFO)
	return(peek3_2(md->readfd) - EXCESS_3_2_LEN);

    if (ioctl(md->readfd, I_NREAD, &size))
	return((short)size);

    return(-1);
}
