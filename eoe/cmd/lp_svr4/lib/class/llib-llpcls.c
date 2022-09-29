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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/class/RCS/llib-llpcls.c,v 1.1 1992/12/14 13:25:28 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*LINTLIBRARY*/

/*	from file delclass.c */
#include "class.h"

/**
 ** delclass() - WRITE CLASS OUT TO DISK
 **/
int delclass (const char * name)
{
    static int  _returned_value;
    return _returned_value;
}

/*	from file freeclass.c */


/**
 ** freeclass() - FREE SPACE USED BY CLASS STRUCTURE
 **/
void freeclass (CLASS * clsbufp)
{
}

/*	from file getclass.c */
/**
 ** getclass() - READ CLASS FROM TO DISK
 **/
CLASS *getclass (const char * name)
{
    static CLASS * _returned_value;
    return _returned_value;
}

/*	from file putclass.c */
/**
 ** putclass() - WRITE CLASS OUT TO DISK
 **/
int putclass (const char * name, const CLASS * clsbufp)
{
    static int  _returned_value;
    return _returned_value;
}
