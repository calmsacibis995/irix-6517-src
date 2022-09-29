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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/requests/RCS/llib-llpreq.c,v 1.1 1992/12/14 13:33:56 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*LINTLIBRARY*/

/* from file anyrequests.c */
# include	<sys/types.h>
# include	"requests.h"

/**
 ** anyrequests() - SEE IF ANY REQUESTS ARE ``QUEUED''
 **/
int anyrequests ()
{
    static int  _returned_value;
    return _returned_value;
}

/* from file freerequest.c */

/**
 ** freerequest() - FREE STRUCTURE ALLOCATED FOR A REQUEST STRUCTURE
 **/
void freerequest ( REQUEST * reqbufp )
{
}

/* from file getrequest.c */

/**
 ** getrequest() - EXTRACT REQUEST STRUCTURE FROM DISK FILE
 **/
REQUEST * getrequest (const char * file)
{
    static REQUEST * _returned_value;
    return _returned_value;
}

/* from file mod32s.c */
char * mod32s ( const char * const file )
{
    static char *  _returned_value;
    return _returned_value;
}

/**
 ** getreqno() - GET NUMBER PART OF REQUEST ID
 **/
char * getreqno (const char * req_id)
{
    static char * _returned_value;
    return _returned_value;
}

/* from file putrequest.c */
/**
 ** putrequest() - WRITE REQUEST STRUCTURE TO DISK FILE
 **/
int putrequest ( const char * file, const REQUEST * reqbufp)
{
    static int  _returned_value;
    return _returned_value;
}
