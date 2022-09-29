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
/* $Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/include/RCS/access.h,v 1.1 1992/12/14 13:21:52 suresh Exp $ */
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#if	!defined(_LP_ACCESS_H)
#define	_LP_ACCESS_H

#include "stdio.h"

/*
 * To speed up reading in each allow/deny file, ACC_MAX_GUESS slots
 * will be preallocated for the internal copy. If these files
 * are expected to be substantially larger than this, bump it up.
 */
#define ACC_MAX_GUESS	100

#if (defined(__STDC__) || (__SVR4__STDC))

int	allow_form_printer ( char **, char * );
int	allow_user_form ( char ** , char * );
int	allow_user_printer ( char **, char * );
int	allowed ( char *, char **, char ** );
int	deny_form_printer ( char **, char * );
int	deny_user_form ( char ** , char * );
int	deny_user_printer ( char **, char * );
int	dumpaccess ( char *, char *, char *, char ***, char *** );
int	is_form_allowed_printer ( char *, char * );
int	is_user_admin ( void );
int	is_user_allowed ( char *, char ** , char ** );
int	is_user_allowed_form ( char *, char * );
int	is_user_allowed_printer ( char *, char * );
int	load_formprinter_access ( char *, char ***, char *** );
int	load_userform_access ( char *, char ***, char *** );
int	load_userprinter_access ( char *, char ***, char *** );
int	loadaccess ( char *, char *, char *, char ***, char *** );
int	bangequ ( char * , char * );
int	bang_searchlist ( char * , char ** );
int	bang_dellist ( char *** , char * );

char *	getaccessfile ( char *, char *, char *, char * );

#else

int	allow_form_printer();
int	allow_user_form();
int	allow_user_printer();
int	allowed();
int	deny_form_printer();
int	deny_user_form();
int	deny_user_printer();
int	dumpaccess();
int	is_form_allowed_printer();
int	is_user_admin();
int	is_user_allowed();
int	is_user_allowed_form();
int	is_user_allowed_printer();
int	load_formprinter_access();
int	load_userform_access();
int	load_userprinter_access();
int	loadaccess();
int	bangequ();
int	bang_searchlist();
int	bang_dellist();

char *	getaccessfile();

#endif

#endif
