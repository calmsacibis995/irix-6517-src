#ifndef __WCHAR_H_
#define __WCHAR_H_

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libw:inc/_wchar.h	1.1"

#include <locale_attr.h>
#include <wchar.h>

#define SS2     0x8e
#define SS3     0x8f
#define H_EUCMASK 0x8080
#define H_P00     0          	  /* Code Set 0 */
#define H_P11     0x8080          /* Code Set 1 */
#define H_P01     0x0080          /* Code Set 2 */
#define H_P10     0x8000          /* Code Set 3 */
#define EUCMASK 0x30000000
#define P00     0          	    /* Code Set 0 */
#define P11     0x30000000          /* Code Set 1 */
#define P01     0x10000000          /* Code Set 2 */
#define P10     0x20000000          /* Code Set 3 */

#define multibyte       (__libc_attr._csinfo._mb_cur_max>1)	/* Are we in a mb encoding */
#define eucw1		(__libc_attr._csinfo._eucwidth[0])	/* length of code set 1 */
#define eucw2		(__libc_attr._csinfo._eucwidth[1])	/* length of code set 2 */
#define eucw3		(__libc_attr._csinfo._eucwidth[2])	/* length of code set 3 */
#define scrw1		(__libc_attr._csinfo._scrwidth[0])	/* width of code set 1 */
#define scrw2		(__libc_attr._csinfo._scrwidth[1])	/* width of code set 2 */
#define scrw3		(__libc_attr._csinfo._scrwidth[2])	/* width of code set 3 */
#define _mbyte		(__libc_attr._csinfo._mb_cur_max)	/* max bytes in a mb char */
	
/*	data structure for supplementary code set
		for character class and conversion	*/

struct	_wctype {
    wchar_t	tmin;		/* minimum code for wctype */
    wchar_t	tmax;		/* maximum code for wctype */
    off_t	index_off;	/* offset to the beginning of index table */
    off_t	type_off;	/* offset to the beginning of type table */
    wchar_t	cmin;		/* minimum code for conversion */
    wchar_t	cmax;		/* maximum code for conversion */
    off_t	code_off;	/* offset to the beginning of the conversion code */
};

struct	_wctype_tbl {
    unsigned char *	index;
    unsigned int *	type;
    wchar_t *		code;
};

extern struct _wctype *_wcptr[];
extern struct _wctype_tbl _wctbl[3];
#endif /* __WCHAR_H_ */
