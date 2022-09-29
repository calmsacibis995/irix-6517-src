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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/users/RCS/llib-llpusr.c,v 1.1 1992/12/14 13:34:46 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* LINTLIBRARY */

#include "users.h"


/*	from file usermgmt.c */

int putuser ( const char * user, const USER * pri_s )
{
    static int _returned_value;
    return _returned_value;
}

USER * getuser ( const char * user )
{
    static USER * _returned_value;
    return _returned_value;
}

int deluser ( const char * user )
{
    static int _returned_value;
    return _returned_value;
}

int getdfltpri ( void )
{
    static int _returned_value;
    return _returned_value;
}

int trashusers ( void )
{
    static int _returned_value;
    return _returned_value;
}


/*	from file loadpri.c */
struct user_priority * ld_priority_file ( char * path )
{
    static struct user_priority * _returned_value;
    return _returned_value;
}

int add_user ( struct user_priority * ppri_tbl, char * user, int limit )
{
    static int _returned_value;
    return _returned_value;
}

char * next_user ( FILE * f, char * buf, char ** pp )
{
    static char * _returned_value;
    return _returned_value;
}

del_user ( struct user_priority * ppri_tbl, char * user )
{
    static  _returned_value;
    return _returned_value;
}


/*	from file storepri.c */
print_tbl ( struct user_priority * ppri_tbl )
{
    static  _returned_value;
    return _returned_value;
}

output_tbl ( FILE * f, struct user_priority * ppri_tbl )
{
    static  _returned_value;
	return _returned_value;
}
