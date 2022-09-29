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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/filters/RCS/llib-llpflt.c,v 1.1 1992/12/14 13:26:19 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*LINTLIBRARY*/
# include	"stdio.h"

/*	from file conv.c */
# include	"filters.h"

/*	from file delfilter.c */

/**
 ** delfilter() - DELETE A FILTER FROM FILTER TABLE
 **/

int delfilter ( const char * name )
{
    static int _returned_value;
    return _returned_value;
}

/*	from file dumpfilters.c */

/**
 ** dumpfilters() - WRITE FILTERS FROM INTERNAL STRUCTURE TO FILTER TABLE
 **/
int dumpfilters (const char *file)
{
    static int  _returned_value;
    return _returned_value;
}

/*	from file freefilter.c */
/**
 ** freefilter() - FREE INTERNAL SPACE ALLOCATED FOR A FILTER
 **/
void freetempl (TEMPLATE * templ)
{
}
void free_filter (_FILTER * pf)
{
}
void freefilter (FILTER * pf)
{
}

/*	from file getfilter.c */

/**
 ** getfilter() - GET FILTER FROM FILTER TABLE
 **/
FILTER * getfilter ( const char * name)
{
    static FILTER * _returned_value;
    return _returned_value;
}

/*	from file filtertable.c */
/**
 ** get_and_load() - LOAD REGULAR FILTER TABLE
 **/
int get_and_load ( void )
{
    static int  _returned_value;
    return _returned_value;
}

/**
 ** open_filtertable( void )
 **/
FILE * open_filtertable (const char *file, const char *mode)
{
    static FILE * _returned_value;
    return _returned_value;
}

/**
 ** close_filtertable( void )
 **/
void close_filtertable (FILE * fp)
{
}

/*	from file insfilter.c */

/**
 ** insfilter( void )
 **/
FILTERTYPE insfilter ( char **pipes, char *input_type,
		       char *output_type, char *printer_type,
		       char *printer, char **parms,
		       unsigned short *flagsp)
{
    static FILTERTYPE _returned_value;
    return _returned_value;
}

/*	from file loadfilters.c */

int loadfilters (const char *file)
{
    static int _returned_value;
    return _returned_value;
}

/*	from file putfilter.c */

int putfilter (const char *name, const FILTER *flbufp)
{
    static int _returned_value;
    return _returned_value;
}

/*	from file search.c */
/**
 ** search_filter() - SEARCH INTERNAL FILTER TABLE FOR FILTER BY NAME
 **/
_FILTER *search_filter (const char *name)
{
    static _FILTER * _returned_value;
    return _returned_value;
}

/*	from file trash.c */

/**
 ** trash_filters() - FREE ALL SPACE ALLOCATED FOR FILTER TABLE
 **/
void trash_filters ( void )
{
}
