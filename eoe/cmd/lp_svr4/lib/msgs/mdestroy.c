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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/msgs/RCS/mdestroy.c,v 1.1 1992/12/14 13:31:11 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

# include	<string.h>
# include	<stropts.h>
# include	<errno.h>
# include	<stdlib.h>
# include	<unistd.h>

# include	"lp.h"
# include	"msgs.h"

#if	defined(__STDC__)
int mdestroy ( MESG * md )
#else
int mdestroy (md)
    MESG	*md;
#endif
{
    if (!md || md->type != MD_MASTER || md->file == NULL)
    {
	errno = EINVAL;
	return(-1);
    }

    if (fdetach(md->file) != 0)
        return(-1);

    if (ioctl(md->writefd, I_POP, "connld") != 0) {
fprintf(stderr,"DBG: mdestroy I_POP md->wid %d failed \n",md->writefd);
        return(-1);
    }
fprintf(stderr,"DBG: mdestroy I_POP md->wid %d success \n",md->writefd);
    Free(md->file);
    md->file = NULL;

    (void) mdisconnect(md);

    return(0);
}
