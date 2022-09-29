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
/* $Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/include/RCS/lp.set.h,v 1.1 1992/12/14 13:22:44 suresh Exp $ */
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/



#if	!defined(_LP_LP_SET_H)
#define	_LP_LP_SET_H

/*
 * How far should we check for "compressed" horizontal pitch?
 * Keep in mind that (1) too far and the user can't read it, and
 * (2) some Terminfo entries don't limit their parameters like
 * they should. Keep in mind the other hand, though: What is too
 * compact for you may be fine for the eagle eyes next to you!
 */
#define MAX_COMPRESSED	30	/* CPI */

#define	E_SUCCESS	0
#define	E_FAILURE	1
#define	E_BAD_ARGS	2
#define	E_MALLOC	3

#define	OKAY(P)		((P) && (*P))
#define R(F)		(int)((F) + .5)

#if	!defined(CHARSETDIR)
# define CHARSETDIR	"/usr/share/lib/charset"
#endif

#if (defined(__STDC__) || (__SVR4__STDC))

int		set_pitch ( char * , int , int );
int		set_size ( char * , int , int );
int		set_charset ( char * , int , char * );

#else

int		set_pitch(),
		set_size(),
		set_charset();

#endif

#endif
