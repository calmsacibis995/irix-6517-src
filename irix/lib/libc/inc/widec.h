#ifndef __WIDEC_H__
#define __WIDEC_H__
#ifdef __cplusplus
extern "C" {
#endif
#ident "$Revision: 1.2 $"
/*
*
* Copyright 1984-1995, Silicon Graphics, Inc.
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
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#include <stdio.h>
#include <wchar.h>

/* 
 * Old, non XPG wide string operations
 */
extern wchar_t
	*wscpy(wchar_t *, const wchar_t *),
	*wsncpy(wchar_t *, const wchar_t *, size_t),
	*wscat(wchar_t *, const wchar_t *),
	*wsncat(wchar_t *, const wchar_t *, size_t),
	*wschr(const wchar_t *, wchar_t),
	*wsrchr(const wchar_t *, wchar_t),
	*wspbrk(const wchar_t *, const wchar_t *),
	*wstok(wchar_t *, const wchar_t *);
extern int
	wscmp(const wchar_t *, const wchar_t *),
	wsncmp(const wchar_t *, const wchar_t *, size_t);
extern size_t
	wslen(const wchar_t *),
	wsspn(const wchar_t *, const wchar_t *),
	wscspn(const wchar_t *, const wchar_t *);
extern char
	*wstostr(char *, wchar_t *);
extern	wchar_t
	*strtows(wchar_t *, char *);

#ifdef __cplusplus
}
#endif
#endif /* !__WIDEC_H__ */
