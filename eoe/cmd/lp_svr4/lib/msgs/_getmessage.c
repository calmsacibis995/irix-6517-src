
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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/msgs/RCS/_getmessage.c,v 1.1 1992/12/14 13:30:45 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* LINTLIBRARY */

# include	<stdarg.h>
# include	<string.h>
# include	<errno.h>
#include 	<stdio.h>
# include	"msgs.h"

extern char	*_lp_msg_fmts[];
extern int	errno;

/* VARARGS */
#if	defined(__STDC__)
int _getmessage ( char * buf, short rtype, va_list arg )
#else
int _getmessage (buf, rtype, va_alist)
    char	*buf;
    short	rtype;
    va_list 	arg;
#endif
{
    char	*endbuf;
    char	*fmt;
    char	**t_string;
    int		temp = 0;
    long	*t_long;
    short	*t_short;
    short	etype;

    if (buf == (char *)0)
    {
	errno = ENOSPC;
	return(-1);
    }

    /*
     * We assume that we're given a buffer big enough to hold
     * the header.
     */

    endbuf = buf + (long)stoh(buf);
/* fprintf(stderr,"DBG _getmessage legth of the buf %d buf <%s> \n",stoh(buf),buf); */
    if ((buf + MESG_DATA) > endbuf)
    {
	errno = ENOMSG;
	return(-1);
    }

    etype = stoh(buf + MESG_TYPE);
    if (etype < 0 || etype > LAST_MESSAGE)
    {
	errno = EBADMSG;
        return(-1);
    }

    if (etype != rtype)
    {
	if (rtype > 0 && rtype <= LAST_MESSAGE)
	    fmt = _lp_msg_fmts[rtype];
	else
	{
	    errno = EINVAL;
	    return(-1);
	}
    }
    else
	fmt = _lp_msg_fmts[etype];

/* fprintf(stderr,"DBG: _getmessage fmt %s etype %d pid %d\n",fmt,etype,getpid()); */
    buf += MESG_LEN; 


    while (*fmt != '\0')
	switch(*fmt++)
	{
	    case 'H':
/* fprintf(stderr,"DBG: _getmessage H  pid %d errno %d\n",getpid(),errno); */
	        if ((buf + 4) > endbuf)
		{
		    errno = ENOMSG;
		    return(-1);
		}

		t_short = va_arg(arg, short *);
		*t_short = stoh(buf);
/* fprintf(stderr,"DBG: _getmessage H t_short <%d> *t_short <%d>\n",t_short,*t_short); */
		buf += 4;
		break;

	    case 'L':
/* fprintf(stderr,"DBG: _getmessage L  pid %d errno %d\n",getpid(),errno); */
		if ((buf + 8) > endbuf)
		{
		    errno = ENOMSG;
		    return(-1);
		}
		
		t_long = va_arg(arg, long *);
		*t_long = stol(buf);
/* fprintf(stderr,"DBG: _getmessage L t_long <%d> *t_long <%d>\n",t_long,*t_long); */
		buf += 8;
		break;

	    case 'D':
/* fprintf(stderr,"DBG: _getmessage D  pid %d errno %d\n",*fmt,getpid(),errno); */
		if ((buf + 4) > endbuf)
		{
		    errno = ENOMSG;
		    return(-1);
		}

		t_short = va_arg(arg, short *);
		*t_short = stoh(buf);
/* fprintf(stderr,"DBG: _getmessage D t_short <%d> *t_short <%d>\n",t_short,*t_short); */
		buf += 4;
		t_string = va_arg(arg, char **);
		if ((buf + *t_short) > endbuf)
		{
		    errno = ENOMSG;
		    return(-1);
		}
		(*t_short)--;		/* Don't mention the null we added */
		*t_string = buf;
/* fprintf(stderr,"DBG: _getmessage D *t_string <%s> \n",t_string); */
		buf += *t_short;
		break;

	    case 'S':
/* fprintf(stderr,"DBG: _getmessage S  pid %d errno %d\n",getpid(),errno); */
		if ((buf + 4) > endbuf)
		{
		    errno = ENOMSG;
		    return(-1);
		}

		t_string = va_arg(arg, char **);
/* fprintf(stderr,"DBG: _getmessage S t_string <%s>  <%x> \n",t_string,t_string); */
		temp = stoh(buf);
		buf += 4;
		if ((buf + temp) > endbuf)
		{
		    errno = ENOMSG;
		    return(-1);
		}

		*t_string = buf;
/* fprintf(stderr,"DBG: _getmessage S *t_string <%s>  <%x>\n",*t_string,*t_string); */
		buf += temp;
		break;
	}
    return(etype);
}
