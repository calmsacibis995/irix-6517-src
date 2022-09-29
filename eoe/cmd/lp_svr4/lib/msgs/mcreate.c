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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/msgs/RCS/mcreate.c,v 1.1 1992/12/14 13:31:08 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


# include	<unistd.h>
# include	<string.h>
# include	<stropts.h>
# include	<errno.h>
# include	<stdlib.h>

# include	"lp.h"
# include	"msgs.h"

#if	defined(__STDC__)
MESG * mcreate ( char * path )
#else
MESG * mcreate (path)
    char	*path;
#endif
{
    int			fds[2];
    MESG		*md;

    if (pipe(fds) != 0)
	return(NULL);

#if	!defined(NOCONNLD)
    if (ioctl(fds[1], I_PUSH, "connld") != 0) {
fprintf(stderr,"DBG: mcreate I_PUSH fds[1] %d failed\n",fds[1]);
	return(NULL);
	}
fprintf(stderr,"DBG: mcreate I_PUSH fds[1] %d success \n",fds[1]);
#endif

    if (fattach(fds[1], path) != 0)
        return(NULL);

    if ((md = (MESG *)Malloc(MDSIZE)) == NULL)
	return(NULL);
    
    md->admin = 1;
    md->file = Strdup(path);
    md->gid = getgid();
    md->mque = NULL;
    md->on_discon = NULL;
    md->readfd = fds[0];
    md->state = MDS_IDLE;
    md->type = MD_MASTER;
    md->uid = getuid();
    md->writefd = fds[1];
    
    return(md);
}
