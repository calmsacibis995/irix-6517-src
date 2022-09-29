/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libw:inc/pcode.h	1.1"

/*
 * This header file contains internal macros and definitons
 */

#include "_wchar.h"

#ifndef _WCHAR16
#define	wcsize(x)	(((x) & EUCMASK) == P00 ? 1 : \
			((x) & EUCMASK) == P11 ? eucw1 : \
			((x) & EUCMASK) == P10 ? eucw3+1 : \
						 eucw2+1)
#else
#define	wcsize(x)	(((x) & H_EUCMASK) == H_P00 ? 1 : \
			((x) & H_EUCMASK) == H_P11 ? eucw1 : \
			((x) & H_EUCMASK) == H_P10 ? eucw3+1 : \
			control1(x)            ? 1 : eucw2+1)
#endif

#define	controlc(c)	((((c) & 0x7F) < 0x20) || (c) == 0x7F)
#define	control1(c)	((c) >= 0x80 && (c) <= 0x9F)
#define graphic(c)	((c) >= 0xA0 && (c) <= 0xFF)
