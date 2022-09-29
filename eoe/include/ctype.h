#ifndef __CTYPE_H__
#define __CTYPE_H__
#ifdef __cplusplus
extern "C" {
#endif
#ident "$Revision: 1.29 $"
/*
*
* Copyright 1992, Silicon Graphics, Inc.
* All Rights Reserved.
*
* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
* the contents of this file may not be disclosed to third parties, copied or
* duplicated in any form, in whole or in part, without the prior written
* permission of Silicon Graphics, Inc.
*
* RESTRICTED RIGHTS LEGEND:
* Use, duplication or disclosure by the Government is subject to restrictions
* as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
* and Computer Software clause at DFARS 252.227-7013, and/or in similar or
* successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
* rights reserved under the Copyright Laws of the United States.
*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Note - this is an ANSI and XPG4 header file - In ANSI mode only
 * ANSI symbols are permitted!
 */

#include <standards.h>

#define	_U	0x00000001	/* Upper case */
#define	_L	0x00000002	/* Lower case */
#define	_N	0x00000004	/* Numeral (digit) */
#define	_S	0x00000008	/* Spacing character */
#define	_P	0x00000010	/* Punctuation */
#define	_C	0x00000020	/* Control character */
#define	_B	0x00000040	/* Obsolete: was to designate the space character only */
#define	_X	0x00000080	/* heXadecimal digit */

#define _A	0x00004000	/* Alphabetical characters only */
#define _PR	0x00008000	/* Printable characters only */
#define _G	0x40000000	/* Graphic characters only */
#define _BL	0x80000000	/* The blank character class */

#define _ISalpha	(_U | _L | _A)
#define	_ISupper	(_U)
#define	_ISlower	(_L)
#define	_ISdigit	(_N)
#define	_ISxdigit	(_X)
#define	_ISalnum	(_U | _L | _A | _N)
#define	_ISspace	(_S | _BL)
#define _ISblank	(_BL)
#define	_ISpunct	(_P)
#define	_ISprint	(_P | _U | _L | _N | _A | _X | _PR)
#define	_ISgraph	(_P | _U | _L | _N | _A | _X | _G)
#define	_IScntrl	(_C)

extern int isalnum(int);
extern int isalpha(int);
extern int iscntrl(int);
extern int isdigit(int);
extern int isgraph(int);
extern int islower(int);
extern int isprint(int);
extern int ispunct(int);
extern int isspace(int);
extern int __isblank(int);
extern int isupper(int);
extern int isxdigit(int);
extern int tolower(int);
extern int toupper(int);

/* ANSI permits anything that starts with a 'is' or 'to' */
extern int isascii(int);        
extern int toascii(int);        
#if _XOPEN4 && _NO_ANSIMODE
extern int _tolower(int);
extern int _toupper(int);
#endif

#ifndef _KERNEL
#include <locale_attr.h>

#ifndef _LINT

#define	isalpha(c)	((__libc_attr._ctype_tbl->_class+1)[c] & _ISalpha)
#define	isupper(c)	((__libc_attr._ctype_tbl->_class+1)[c] & _ISupper)
#define	islower(c)	((__libc_attr._ctype_tbl->_class+1)[c] & _ISlower)
#define	isdigit(c)	((__libc_attr._ctype_tbl->_class+1)[c] & _ISdigit)
#define	isxdigit(c)	((__libc_attr._ctype_tbl->_class+1)[c] & _ISxdigit)
#define	isalnum(c)	((__libc_attr._ctype_tbl->_class+1)[c] & _ISalnum)
#define	isspace(c)	((__libc_attr._ctype_tbl->_class+1)[c] & _ISspace)
#define __isblank(c)	((__libc_attr._ctype_tbl->_class+1)[c] & _ISblank)
#define	ispunct(c)	((__libc_attr._ctype_tbl->_class+1)[c] & _ISpunct)
#define	isprint(c)	((__libc_attr._ctype_tbl->_class+1)[c] & _ISprint)
#define	isgraph(c)	((__libc_attr._ctype_tbl->_class+1)[c] & _ISgraph)
#define	iscntrl(c)	((__libc_attr._ctype_tbl->_class+1)[c] & _IScntrl)

#define	isascii(c)	(!((c) & ~0177))
#define	toascii(c)	((c) & 0177)
#if _XOPEN4 && _NO_ANSIMODE
#define	_toupper(c)     ((__libc_attr._ctype_tbl->_upper+1)[c])
#define	_tolower(c)	((__libc_attr._ctype_tbl->_lower+1)[c])
#endif

#endif	/* lint */

#else	/* _KERNEL */

extern unsigned char __ctype[];

#ifndef _LINT

#define	isalpha(c)	((__ctype +1)[c] & _ISalpha)
#define	isupper(c)	((__ctype +1)[c] & _ISupper)
#define	islower(c)	((__ctype +1)[c] & _ISlower)
#define	isdigit(c)	((__ctype +1)[c] & _ISdigit)
#define	isxdigit(c)	((__ctype +1)[c] & _ISxdigit)
#define	isalnum(c)	((__ctype +1)[c] & _ISalnum)
#define	isspace(c)	((__ctype +1)[c] & _ISspace)
#define __isblank(c)	((__ctype +1)[c] & _ISblank)
#define	ispunct(c)	((__ctype +1)[c] & _ISpunct)
#define	isprint(c)	((__ctype +1)[c] & (_ISprint | _B))
#define	isgraph(c)	((__ctype +1)[c] & _ISgraph)
#define	iscntrl(c)	((__ctype +1)[c] & _IScntrl)

#define	isascii(c)	(!((c) & ~0177))
#define	toascii(c)	((c) & 0177)
#if _XOPEN4 && _NO_ANSIMODE
#define	_toupper(c)     ((__ctype + 258)[c])
#define	_tolower(c)	((__ctype + 258)[c])
#endif

#endif	/* lint */

#endif

#ifdef __cplusplus
}
#endif
#endif /* !__CTYPE_H__ */
