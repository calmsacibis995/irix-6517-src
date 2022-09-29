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
/* $Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/include/RCS/class.h,v 1.1 1992/12/14 13:22:03 suresh Exp $ */
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#if	!defined(_LP_CLASS_H)
#define	_LP_CLASS_H

/**
 ** The internal flags seen by the Spooler/Scheduler and anyone who asks.
 **/

#define CS_REJECTED	0x001

/**
 ** The internal copy of a class as seen by the rest of the world:
 **/

/*
 * A (char **) list is an array of string pointers (char *) with
 * a null pointer after the last item.
 */
typedef struct CLASS {
	char   *name;		/* name of class (redundant) */
	char   **members;       /* members of class */
}			CLASS;

/**
 ** Various routines.
 **/

#if (defined(__STDC__) || (__SVR4__STDC))

CLASS		*getclass ( char * );

int		putclass ( char *, CLASS * );
int		delclass ( char * );

void		freeclass ( CLASS * );

#else

CLASS		*getclass();

int		putclass(),
		delclass();

void		freeclass();

#endif

#endif
