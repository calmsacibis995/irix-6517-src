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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/msgs/RCS/write_fifo.c,v 1.1 1992/12/14 13:31:54 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* LINTLIBRARY */

# include	<unistd.h>
# include	<errno.h>
# include	<string.h>

# include	"lp.h"
# include	"msgs.h"

/*
** Choose at least one byte that won't appear in the body or header
** of a message.
*/
char	Resync[HEAD_RESYNC_LEN]		= { 0x01, 0xFE };
char	Endsync[HEAD_RESYNC_LEN]	= { 0x02, 0xFD };


/*
** write_fifo() - WRITE A BUFFER WITH HEADER AND CHECKSUM
*/

#if	defined(__STDC__)
int write_fifo ( int fifo, char * buf, unsigned int size )
#else
int write_fifo (fifo, buf, size)
    int			fifo;
    char		*buf;
    unsigned int	size;
#endif
{
    unsigned short	chksum	= 0;
    int			wbytes	= 0;

    (void)memcpy (buf + HEAD_RESYNC, Resync, HEAD_RESYNC_LEN);
    (void)memcpy (buf + TAIL_ENDSYNC(size), Endsync, TAIL_ENDSYNC_LEN);

    CALC_CHKSUM (buf, size, chksum);
    (void)htos (buf + TAIL_CHKSUM(size), chksum);


    /*
    ** A message must be written in one call, to avoid interleaving
    ** messages from several processes.
    **
    ** The caller is responsible for trapping SIGPIPE, so
    ** we just return what the "write()" system call does.
    **
    ** Well, almost.  If the pipe was almost full, we may have
    ** written a partial message.  If this is the case, we lie
    ** and say the pipe was full, so the caller can try again.
    **
    ** read_fifo can deal with a truncated message, so we let it
    ** do the grunt work associated with partial messages.
    **
    ** NOTE:  Writing the remainder of the message is not feasible
    ** as someone else may have written something to the fifo
    ** while we were setting up to retry.
    */

    if ((wbytes = write(fifo, buf, size)) > 0)
	if (wbytes != size)
	    return(0);

    return(wbytes);
}
