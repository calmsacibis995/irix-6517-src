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
/* $Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/include/RCS/secure.h,v 1.1 1992/12/14 13:23:30 suresh Exp $ */
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#if	!defined(_LP_SECURE_H)
#define _LP_SECURE_H

#include "sys/types.h"

/**
 ** The disk copy of the secure request files:
 **/

/*
 * There are 7 fields in the secure request file.
 */
#define	SC_MAX  7
# define SC_REQID	0	/* Original request id */
# define SC_UID		1	/* Originator's user ID */
# define SC_USER	2	/* Originator's real login name */
# define SC_GID		3	/* Originator's group ID */
# define SC_SIZE	4	/* Total size of the request data */
# define SC_DATE	5	/* Date submitted (in seconds) */
# define SC_SYSTEM	6	/* Originating system */

/**
 ** The internal copy of a request as seen by the rest of the world:
 **/

typedef struct SECURE {
    uid_t	uid;
    gid_t	gid;
    off_t	size;
    time_t	date;
    char	*system;
    char	*user;
    char	*req_id;
}			SECURE;

/**
 ** Various routines.
 **/

#if (defined(__STDC__) || (__SVR4__STDC))

SECURE *	getsecure ( char * );
int		putsecure ( char *, SECURE * );
void		freesecure ( SECURE * );

#else

SECURE *	getsecure();
int		putsecure();
void		freesecure();

#endif

#endif
