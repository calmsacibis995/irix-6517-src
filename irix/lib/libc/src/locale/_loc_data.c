/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/_loc_data.c	1.10"
#include "synonyms.h"
#include <locale.h>
#include "_locale.h"
#include "_wchar.h"
#include <ctype.h>
#include <sys/euc.h>

char _cur_locale[LC_ALL][LC_NAMELEN] = /* current locale names */
{
	"C", "C", "C", "C", "C", "C"		/* need a repeat count feature */
};

unsigned char _numeric[SZ_NUMERIC] =
{
	'.',	'\0',
};

__attr_t __libc_attr = {
    (__ctype_t *) &_ctype_dflt,	/* default ctype */

    {				/* Character encoding information */
	{ 1, 0, 0 },		/* Extended character set width */
	{ 1, 0, 0 },		/* Extended character set screen width */
	1,			/* Maximum bytes per character in current encoding */
    },

    {				/* EUC related functions */
	1,			/* Indicates if the current locale is an EUC encoding */
	__isset2,		/* Substitions for ISSET2 macros */
	__isset3		/* Substitions for ISSET3 macros */
    },

    {				/* Resources related to LC_COLLATE category */
	1			/* strcoll()/strxfrm() behave as strcmp()/strcpy() */
    }
};

/* pointers to multi-byte character table */
struct _wctype *_wcptr[] = { 0,0,0 };
struct _wctype_tbl _wctbl[3] = { { 0,0,0 }, { 0,0,0 }, { 0,0,0 } };

int	__isset2( int c )
{
    return (c == SS2);
}

int	__isset3( int c )
{
    return (c == SS3);
}

int	__iscodeset ( int codeset, wint_t wc )
{
    switch ( codeset ) {
    case 0:
	return !((wc) & ~0xff);
    case 1:
	return (((wc) >> 28) == 0x3);
    case 2:
	return (((wc) >> 28) == 0x1);
    case 3:
	return (((wc) >> 28) == 0x2);

    default:
	return 0;
    }

}
