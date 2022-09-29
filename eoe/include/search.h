#ifndef __SEARCH_H__
#define __SEARCH_H__
#ifdef __cplusplus
extern "C" {
#endif
#ident "$Revision: 1.21 $"
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

#include <standards.h>

#if !defined(_SIZE_T) && !defined(_SIZE_T_)
#define _SIZE_T
#if (_MIPS_SZLONG == 32)
typedef unsigned int	size_t;
#endif
#if (_MIPS_SZLONG == 64)
typedef unsigned long	size_t;
#endif
#endif

typedef enum { FIND, ENTER } ACTION;
typedef enum { preorder, postorder, endorder, leaf } VISIT;
typedef struct entry { char *key; void *data; } ENTRY;

extern int hcreate(size_t);
extern void hdestroy(void);
extern ENTRY *hsearch(ENTRY, ACTION);
extern void *tdelete(const void *, void **,
		int (*)(const void *, const void *)); 
extern void *tfind(const void *, void *const *,
		int (*)(const void *, const void *));
extern void *tsearch(const void *, void **,
		int (*)(const void *, const void *));
extern void twalk(const void *, void (*)(const void *, VISIT, int)) ;
extern void *lfind(const void *, const void *, size_t *, size_t, 
	    int (*)(const void *, const void *));
extern void *lsearch(const void *, void *, size_t *, size_t,
	    int (*)(const void *, const void *));

#if _SGIAPI
struct qelem {
	struct qelem	*q_forw;
	struct qelem	*q_back;
};

void insque(struct qelem *, struct qelem *);
void remque(struct qelem *);
void *bsearch(const void *, const void *, size_t, size_t,
	    int (*)(const void *, const void *));
#endif	/* _SGIAPI */

#if (!_SGIAPI && _XOPEN4UX)
void insque(void *, void *);
void remque(void *);
#endif	/* (!_SGIAPI && _XOPEN4UX) */

#ifdef __cplusplus
}
#endif
#endif /* !__SEARCH_H__ */
