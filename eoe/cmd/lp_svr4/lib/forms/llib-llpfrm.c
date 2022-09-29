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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/forms/RCS/llib-llpfrm.c,v 1.1 1992/12/14 13:27:20 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*LINTLIBRARY*/

/*	from file delform.c */

# include	"lp.h"
# include	"form.h"

int delform(const char *formname)
{
    static int _returned_value;
    return _returned_value;
}

int formtype(const char *formname)
{
    static int _returned_value;
    return _returned_value;
}

/*	from file getform.c */
int getform ( const char * form, FORM * formp, FALERT * alertp,
	      FILE ** alignfilep )
{
    static int _returned_value;
    return _returned_value;
}

/*	from file putform.c */
int putform( const char * form, const FORM * formp, const FALERT *alertp,
	     FILE ** alignfilep)
{
    static int _returned_value;
    return _returned_value;
}
FILE *formfopen( const char *formname, const char *file, const char *type,
		 int dircreat)
{
    static FILE * _returned_value;
    return _returned_value;
}

/*	from file rdform.c */
int addform( const char *form_name, const FORM *formp, FILE *usrfp)
{
    static int _returned_value;
    return _returned_value;
}
int chform( const char *form_name, const FORM *cform, FILE *usrfp)
{
    static int _returned_value;
    return _returned_value;
}
int chfalert( const FALERT *new, const FALERT *old)
{
    static int _returned_value;
    return _returned_value;
}
int nscan( const char *s, const SCALED *var)
{
    static int _returned_value;
    return _returned_value;
}
void
stripnewline( const char *line)
{
}
char * mkcmd_usr ( const char * const cmd )
{
    static char *  _returned_value;
    return _returned_value;
}
char * getformdir ( const char * formname, int creatdir)
{
    static char *  _returned_value;
    return _returned_value;
}
int scform ( const char * form_nm, const FORM * formp, const FILE *fp,
	     short LP_FORM, int * opttag )
{
    static int _returned_value;
    return _returned_value;
}

/*	from file wrform.c */
int wrform( const FORM *formp, FILE *fp)
{
    static int _returned_value;
    return _returned_value;
}
int wralign( FILE *where, FILE *fp)
{
    static int _returned_value;
    return _returned_value;
}
void wralert( const FALERT *alertp)
{
}
int printcmt( FILE *where, const char *comment)
{
    static int _returned_value;
    return _returned_value;
}
