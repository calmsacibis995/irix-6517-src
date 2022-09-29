/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/ctypefcns.c	1.4"
#include "synonyms.h"
#include <ctype.h>

#undef isalpha
#undef isupper
#undef islower
#undef isdigit
#undef isxdigit
#undef isalnum
#undef isspace
#undef __isblank
#undef ispunct
#undef isprint
#undef isgraph
#undef iscntrl
#undef isascii
#undef _toupper
#undef _tolower
#undef toascii

#ifdef __STDC__
	#pragma weak isascii = _isascii
	#pragma weak toascii = _toascii
#define isascii _isascii
#define toascii _toascii
#endif

int
isalpha(c)
int c;
{
	return((__libc_attr._ctype_tbl->_class+1)[c] & _ISalpha);
}

int
isupper(c)
int c;
{
	return((__libc_attr._ctype_tbl->_class+1)[c] & _ISupper);
}

int
islower(c)
int c;
{
	return((__libc_attr._ctype_tbl->_class+1)[c] & _ISlower);
}

int
isdigit(c)
int c;
{
	return((__libc_attr._ctype_tbl->_class+1)[c] & _ISdigit);
}

int
isxdigit(c)
int c;
{
	return((__libc_attr._ctype_tbl->_class+1)[c] & _ISxdigit);
}

int
isalnum(c)
int c;
{
	return((__libc_attr._ctype_tbl->_class+1)[c] & _ISalnum);
}

int
isspace(c)
int c;
{
	return((__libc_attr._ctype_tbl->_class+1)[c] & _ISspace);
}

int
__isblank(c)
int c;
{
	return( (__libc_attr._ctype_tbl->_class+1)[c] & _ISblank);
}

int
ispunct(c)
int c;
{
	return((__libc_attr._ctype_tbl->_class+1)[c] & _ISpunct);
}

int
isprint(c)
int c;
{
	return((__libc_attr._ctype_tbl->_class+1)[c] & _ISprint);
}

int
isgraph(c)
int c;
{
	return((__libc_attr._ctype_tbl->_class+1)[c] & _ISgraph);
}

int
iscntrl(c)
int c;
{
	return((__libc_attr._ctype_tbl->_class+1)[c] & _IScntrl);
}

int
isascii(c)
int c;
{
	return(!(c & ~0177));
}

int
_toupper(c)
int c;
{
	return( (__libc_attr._ctype_tbl->_upper+1) [c]);
}

int
_tolower(c)
int c;
{
	return( (__libc_attr._ctype_tbl->_lower+1) [c]);
}

int
toascii(c)
int c;
{
	return((c) & 0177);
}
