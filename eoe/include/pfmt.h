#ifndef __PFMT_H__
#define __PFMT_H__
#ifdef __cplusplus
extern "C" {
#endif
#ident "$Revision: 1.4 $"
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
/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#define MM_STD		0
#define MM_NOSTD	0x100
#define MM_GET		0
#define MM_NOGET	0x200

#define MM_ACTION	0x400

#define MM_NOCONSOLE	0
#define MM_CONSOLE	0x800

/* Classification */
#define MM_NULLMC	0
#define MM_HARD		0x1000
#define MM_SOFT		0x2000
#define MM_FIRM		0x4000
#define MM_APPL		0x8000
#define MM_UTIL		0x10000
#define MM_OPSYS	0x20000

/* Most commonly used combinations */
#define MM_SVCMD	MM_UTIL|MM_SOFT

/* Severity */
struct sev_tab {
	int severity;
	char *string;
};
#define MM_ERROR	0
#define MM_HALT		1
#define MM_WARNING	2
#define MM_INFO		3

#ifndef _STDIO_H
#include <stdio.h>
#endif
#ifndef _VARARGS_H
#include <stdarg.h>
#endif /* var_args */

int pfmt(FILE *, long, const char *, ...);
int lfmt(FILE *, long, const char *, ...);
int __pfmt_print(FILE *, long, const char *, const char **, const char **, va_list);
int __lfmt_log(const char *, const char *, va_list, long, int);
int vpfmt(FILE *, long, const char *, va_list);
int vlfmt(FILE *, long, const char *, va_list);
const char *setcat(const char *);
int setlabel(const char *);
int addsev(int, const char *);
const char *__gtxt(const char *, int, const char *);
char *gettxt(const char *, const char *);

#define DB_NAME_LEN		15
#define MAXLABEL		25

#ifdef __cplusplus
}
#endif
#endif /* !__PFMT_H__ */
