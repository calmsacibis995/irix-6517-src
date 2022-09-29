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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/users/RCS/storepri.c,v 1.1 1992/12/14 13:34:53 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* LINTLIBRARY */

# include	<stdio.h>

# include	"lp.h"
# include	"users.h"

#define WHO_AM_I        I_AM_LPUSERS
#include "oam.h"

/*
Inputs:
Outputs:
Effects:
*/
#if	defined(__STDC__)
int print_tbl ( struct user_priority * ppri_tbl )
#else
int print_tbl (ppri_tbl)
    struct user_priority	*ppri_tbl;
#endif
{
    int limit;

    LP_OUTMSG2(INFO, E_LPU_DEFPRI, ppri_tbl->deflt, ppri_tbl->deflt_limit);
    printlist_setup ("", "", ",", "\n");
    for (limit = PRI_MIN; limit <= PRI_MAX; limit++) {
	if (ppri_tbl->users[limit - PRI_MIN])
	{
	    printf("   %2d     ", limit);
	    printlist(stdout, ppri_tbl->users[limit - PRI_MIN]);
	}
    }
}

/*
Inputs:
Outputs:
Effects:
*/
int output_tbl ( FILE * f, struct user_priority * ppri_tbl )
{
    int		limit;
    char	**ptr;
    int		ucnt;

    fprintf(f, "%d\n%d:\n", ppri_tbl->deflt, ppri_tbl->deflt_limit);
    printlist_setup ("	", "\n", "", "");
    for (limit = PRI_MIN; limit <= PRI_MAX; limit++)
	if (ppri_tbl->users[limit - PRI_MIN])
	{
	    fprintf(f, "%d:", limit);
	    printlist(f, ppri_tbl->users[limit - PRI_MIN]);
	}
}
