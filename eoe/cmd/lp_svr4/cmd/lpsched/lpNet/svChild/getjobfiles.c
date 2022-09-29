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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/cmd/lpsched/lpNet/svChild/RCS/getjobfiles.c,v 1.1 1992/12/14 11:31:38 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/



# include	<unistd.h>
# include	<limits.h>
# include	<string.h>

# include	"lp.h"
# include	"requests.h"

/*
**	Call getjobfiles() with the request file name from the S_SEND_JOB
**	message.  This will return the list of files to be transmitted
**
**	The paths returned by getjobfiles, are absolute paths referencing
**	massaged copies of the request file, secure file, and user data files.
**	The path on the remote system where the data is to be written is
**	derived as follows:
**
**
**	remote_path = makepath(Lp_Spooldir, returned_path + Lp_NTBase, NULL);
**
**
**	(You don't need to use makepath, I just used that as an example.)
**
**
**	returned_path actually looks something like:
**
**	    /var/spool/lp/tmp/.net/tmp/<reqfile>
**	or
**	    /var/spool/lp/tmp/.net/requests/<reqfile>
**				   ^
**				   |
**				   +---------------------+
**							 |
**	returned_path + Lp_NTBase is a pointer to here --+
*/

static char	buff1[PATH_MAX];

#if	defined(__STDC__)
char ** getjobfiles ( char * reqfile )
#else
char ** getjobfiles ( reqfile )
char	*reqfile;
#endif
{
    REQUEST	*req;
    int		len;
    char	**listp;
    char	**flist = NULL;  /*  New  */
    char	**rlist = NULL;

    (void) sprintf(buff1, "%s/tmp/%s", Lp_NetTmp, reqfile);

    req = getrequest(buff1);
    /*  New  */
    if (!req)
	return(NULL);

    flist = req->file_list;
    req->file_list = NULL;
    freerequest(req);

    /*  New  */
    appendlist(&rlist, buff1);
    len = strlen (Lp_Tmp)+1;

    for (listp = flist; *listp; listp++) {
	(void) sprintf(buff1, "%s/tmp/%s", Lp_NetTmp, (*listp)+len);
	appendlist(&rlist, buff1);
    }
    (void) sprintf(buff1, "%s/requests/%s", Lp_NetTmp, reqfile);
    appendlist(&rlist, buff1);

    /*  New  */
    freelist(flist);
    return(rlist);
}
