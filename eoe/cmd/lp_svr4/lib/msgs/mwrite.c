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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/msgs/RCS/mwrite.c,v 1.1 1992/12/14 13:31:42 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* LINTLIBRARY */

# include	<errno.h>
# include	<string.h>
# include	<stropts.h>

# include	"lp.h"
# include	"msgs.h"

int	Lp_prio_msg = 0;

#if	defined(__STDC__)
static int	_mwrite ( MESG * md , char * msgbuf , int );
#else
extern int	write3_2();
#endif

#if	defined(__STDC__)
int mwrite ( MESG * md, char * msgbuf )
#else
int mwrite (md, msgbuf)
    MESG	*md;
    char	*msgbuf;
#endif
{
    short		size;
    MQUE *		p;
    MQUE *		q;

    if (md == NULL)
    {
	errno = ENXIO;
	return(-1);
    }
    if (msgbuf == NULL)
    {
	errno = EINVAL;
	return(-1);
    }

    size = stoh(msgbuf);

    if (LAST_MESSAGE < stoh(msgbuf + MESG_TYPE))
    {
	errno = EINVAL;
	return (-1);
    }
    if (md->mque)
	goto queue;

    if (_mwrite(md, msgbuf, size) == 0)
	return(0);
    if (errno != EAGAIN)
	return(-1);

queue:
    if ((p = (MQUE *)Malloc(sizeof(MQUE))) == NULL
        || (p->dat = (struct strbuf *)Malloc(sizeof(struct strbuf))) == NULL
    	|| (p->dat->buf = (char *)Malloc(size)) == NULL)
    {
	errno = ENOMEM;
	return(-1);
    }
    (void) memcpy(p->dat->buf, msgbuf, size);
    p->dat->len = size;
    p->next = 0;

    if ((q = md->mque))
    {
	while (q->next)
	    q = q->next;
	q->next = p;
    	while((p = md->mque))
    	{
		if(_mwrite(md, p->dat->buf, p->dat->len) != 0)
	    	return(errno == EAGAIN ? 0 : -1);
		md->mque = p->next;
		Free(p->dat->buf);
		Free(p->dat);
		Free(p);
    	}
    }
    else
    	md->mque = p;

    return(0);
}

#if	defined(__STDC__)
static int _mwrite ( MESG * md, char * msgbuf , int size )
#else
int _mwrite (md, msgbuf, size)
    MESG	*md;
    char	*msgbuf;
    int		size;
#endif
{
    int			flag = 0;
    char		buff [MSGMAX];
    struct strbuf	ctl;
    struct strbuf	dat;

    switch (md->type)
    {
        case MD_CHILD:
	case MD_STREAM:
	case MD_BOUND:
	    if (size <= 0 || size > MSGMAX)
	    {
		errno = EINVAL;
		return(-1);
	    }

	    ctl.buf = "xyzzy";
	    ctl.maxlen = 
	    ctl.len = strlen(ctl.buf)+1;
	    dat.buf = msgbuf;
	    dat.len = size;
	    flag = Lp_prio_msg;
	    Lp_prio_msg = 0;	/* clean this up so there are no surprises */

	    if (Putmsg(md, &ctl, &dat, flag) == 0)
		return(0);
	    if (errno == EAGAIN)
		break;
	    return(-1);

	case MD_SYS_FIFO:
	case MD_USR_FIFO:
	    switch (write3_2(md, msgbuf, size))
	    {
		case -1:
		    return(-1);
		case 0:
		    break;
		default:
		    return(0);
	    }
	    break;

	default:
	    errno = EINVAL;
	    return(-1);
    }
    return 0;
}
