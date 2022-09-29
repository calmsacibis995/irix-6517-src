
/*
 * glob.h
 *
 *       header file for glob.c
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Guido van Rossum.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)glob.h	8.1 (Berkeley) 6/2/93
 */

#ifndef _GLOB_H
#define _GLOB_H

#ifdef __cplusplus
extern "C" {
#endif

#ident "$Revision: 1.9 $"

#include <sys/types.h>

/*
 *  MIPS ABI and X/Open glob_t structure.
 *  This structure conforms to the MIPS ABI BB2.0
 *  specification.  Applications that are recompiled
 *  will use this structure by default.
 */
typedef struct {
	size_t gl_pathc;	/* Count of total paths so far. */
	char **gl_pathv;	/* List of paths matching pattern. */
	size_t gl_offs;		/* Reserved at beginning of gl_pathv. */
	void *gl_pad[8];
} glob_t;

#if _SGIAPI
/*
 *  SGI implementation specific glob_t structure.
 *  Old applications which haven't been recompiled
 *  already are using this structure.
 */
typedef struct {
	size_t gl_pathc;	/* Count of total paths so far. */
	char **gl_pathv;	/* List of paths matching pattern. */
	size_t gl_offs;		/* Reserved at beginning of gl_pathv. */
	size_t gl_matchc;	/* Count of paths matching pattern. */
	size_t gl_flags;	/* Copy of flags parameter to glob. */
				/* Copy of errfunc parameter to glob. */
	int (*gl_errfunc) (const char *, int); 
} sgi_glob_t;
#endif

#define	GLOB_APPEND	0x0001	/* Append to output from previous call. */
#define	GLOB_DOOFFS	0x0002	/* Use gl_offs. */
#define	GLOB_ERR	0x0004	/* Return on error. */
#define	GLOB_MARK	0x0008	/* Append / to matching directories. */
#define	GLOB_NOCHECK	0x0010	/* Return pattern itself if nothing matches. */
#define	GLOB_NOSORT	0x0020	/* Don't sort. */
#define GLOB_NOESCAPE   0x0040  /* Disable backslash escaping. */

#define	GLOB_BRACE	0x0080	/* Expand braces ala csh. */
#define	GLOB_MAGCHAR	0x0100	/* Pattern had globbing characters. */
#define	GLOB_NOMAGIC	0x0200	/* GLOB_NOCHECK without magic chars (csh). */
#define	GLOB_QUOTE	0x0400	/* Quote special chars with \. */
#define	GLOB_TILDE	0x0800	/* Expand tilde names from the passwd file. */

#define GLOB_NOSYS	(-1)	/* The implementation does not support this */
#define GLOB_ABORTED    (-2)	/* The scan was stopped */
#define	GLOB_NOSPACE	(-3)	/* Malloc call failed. */
#define GLOB_NOMATCH    (-4)	/* No existing pathname matches */
#define	GLOB_ABEND	(-4)	/* Unignored error. */


/*
 *  Applications recompiled with this include file will use
 *  the new routines: __nglob() and __nglobfree().  Old application
 *  binaries will use the old routines in libc: glob() and  globfree().
 */
int	__nglob (const char *, int, int (*)(const char *, int), glob_t *);
void	__nglobfree (glob_t *);

#ifndef _GLOB_INTERNAL

#if _ABIAPI
/* BB3.0 interfaces */
int	_xglob (const int, const char *, int, int (*)(const char *, int), glob_t *);
void	_xglobfree (const int, glob_t *);
#define _MIPSABI_GLOB_VER	2

/*REFERENCED*/
static int
glob(const char *__pattern, int __flags, int (*__errfunc)(const char *, int), glob_t *__pglob)
{
	return _xglob(_MIPSABI_GLOB_VER, __pattern, __flags, __errfunc, __pglob);
}

/*REFERENCED*/
static void
globfree (glob_t *__pglob)
{
	_xglobfree(_MIPSABI_GLOB_VER, __pglob);
}

#else /* _ABIAPI */
/*REFERENCED*/
static int
glob(const char *__pattern, int __flags, int (*__errfunc)(const char *, int), glob_t *__pglob)
{
	return __nglob(__pattern, __flags, __errfunc, __pglob);
}

/*REFERENCED*/
static void
globfree (glob_t *__pglob)
{
	__nglobfree(__pglob);
}
#endif /* _ABIAPI */
#endif	/* _GLOB_INTERNAL */

#ifdef __cplusplus
}
#endif

#endif /* !_GLOB_H_ */
