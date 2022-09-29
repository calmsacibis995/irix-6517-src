/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/tolower.c	1.7"
/*
 * If arg is upper-case, return lower-case, otherwise return arg.
 * International version
 */

#include "synonyms.h"
#include <ctype.h>

int
tolower(c)
register int c;
{
    return ( __libc_attr._ctype_tbl->_lower+1 ) [ c ];
}

