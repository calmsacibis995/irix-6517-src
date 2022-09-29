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
/* $Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/include/RCS/systems.h,v 1.1 1992/12/14 13:23:35 suresh Exp $ */
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


# define	SYS_PASSWD	0
# define	SYS_PROTO	1
# define	SYS_TMO		2
# define	SYS_RETRY	3
# define	SYS_COMMENT	4
# define	SYS_MAX		5

/**
 ** The internal copy of a system as seen by the rest of the world:
 **/

typedef struct SYSTEM
{
    char	*name;		/* name of system (redundant) */
    char	*passwd;        /* the encrypted passwd of the system */
    char	*reserved1;
    int		protocol;	/* lp networking protocol s5|bsd */
    char	*reserved2;	/* system address on provider */
    int		timeout;	/* maximum permitted idle time */
    int		retry;		/* minutes before trying failed conn */
    char	*reserved3;
    char	*reserved4;
    char	*comment;
} SYSTEM;

# define	NAME_S5PROTO	"s5"
# define	NAME_BSDPROTO	"bsd"

# define	S5_PROTO	1
# define	BSD_PROTO	2

/**
 ** Various routines.
 **/

#if (defined(__STDC__) || (__SVR4__STDC))

SYSTEM		*getsystem ( const char * );

int		putsystem ( const char *, const SYSTEM * ),
		delystem ( const char * );

#else

SYSTEM		*getsystem();

int		putsystem(),
		delystem();

#endif
