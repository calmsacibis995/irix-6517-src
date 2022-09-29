#ident "$Revision: 1.4 $"
/*	Copyright (c) 1993 UNIX System Laboratories, Inc.	*/
/*	(a wholly-owned subsidiary of Novell, Inc.).     	*/
/*	All Rights Reserved.                             	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF UNIX SYSTEM     	*/
/*	LABORATORIES, INC. (A WHOLLY-OWNED SUBSIDIARY OF NOVELL, INC.).	*/
/*	The copyright notice above does not evidence any actual or     	*/
/*	intended publication of such source code.                      	*/

/*	"@(#)libc-port:str/wctype.c	1.3"				*/

#ifdef __STDC__
	#pragma weak wctype = _wctype
	#pragma weak iswctype = _iswctype
#endif

#define __wc
#include "synonyms.h"
#include <wchar.h>
#include <string.h>

wctype_t
wctype(const char *name)
{
	static const struct t
	{ 
		char		name[10];
		wctype_t	bits;
	} table[] =
	{
		"alnum",	_ISwalnum,
		"alpha",	_ISwalpha,
		"blank",	_ISwblank,
		"cntrl",	_ISwcntrl,
		"digit",	_ISwdigit,
		"graph",	_ISwgraph,
		"lower",	_ISwlower,
		"print",	_ISwprint,
		"punct",	_ISwpunct,
		"space",	_ISwspace,
		"upper",	_ISwupper,
		"xdigit",	_ISwxdigit,

		"phonogram",	_ISwphonogram,
		"ideogram",	_ISwideogram,
		"english",	_ISwenglish,
		"number",	_ISwnumber,
		"special",	_ISwspecial,
		"other",	_ISwother,

	};
	register const struct t *p = table;
	register int ch = *name++;

	do
	{
		if (ch == p->name[0] && strcmp(name, &p->name[1]) == 0)
			return p->bits;
	} while (++p < &table[sizeof(table) / sizeof(table[0])]);
	return 0;
}

int
iswctype(wint_t wi, wctype_t x)
{
	if ((wi & ~(wchar_t)0xff) == 0)
	    return((int)(x & (__libc_attr._ctype_tbl->_class+1)[wi]));

	return(__iswctype(wi, x));
}
