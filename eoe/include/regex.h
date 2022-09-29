#ifndef _REGEX_H
#define _REGEX_H
#ifdef __cplusplus
extern "C" {
#endif
#ident "$Revision: 1.11 $"
/*
*
* Copyright 1995, Silicon Graphics, Inc.
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

/*-
 * Copyright (c) 1992 Henry Spencer.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Henry Spencer of the University of Toronto.
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
 *	@(#)regex.h	8.2 (Berkeley) 1/3/94
 */

#include <sys/types.h>

/* regcomp() flags */
#define	REG_BASIC	0000
#define	REG_EXTENDED	0001	/* use extended regular expressions */
#define	REG_ICASE	0004	/* ignore case in match */
#define	REG_NOSUB	0010	/* report only success/fail in regexec() */
#define	REG_NEWLINE	0002	/* change the handling of <newline> */
#define	REG_NOSPEC	0020
#define	REG_PEND	0040
#define	REG_DUMP	0200

/* regerror() flags */
#define REG_ENOSYS	0034	/* unsupported */
#define	REG_NOMATCH	0024	/* regec() failed to match */
#define	REG_BADPAT	0002	/* invalid regular expression */
#define	REG_ECOLLATE	0025	/* invalid collating element referenced */
#define	REG_ECTYPE	0030	/* invalid character class type referenced */
#define	REG_EESCAPE	0026	/* trailing \ in pattern */
#define	REG_ESUBREG	0031	/* number in \digit invalid or in error */
#define	REG_EBRACK	0061	/* [] imbalance */
#define	REG_EPAREN	0052	/* \(\) or () imbalance */
#define	REG_EBRACE	0055	/* \{\} imbalance */
#define	REG_BADBR	0003	/* contents of \{\} invalid */
#define	REG_ERANGE	0027	/* invalid endpoint in expression */
#define	REG_ESPACE	0062	/* out of memory */
#define	REG_BADRPT	0032	/* ?,*,or + not preceeded by valid r.e. */
#define	REG_EMPTY	0016
#define	REG_ASSERT	0017
#define	REG_INVARG	0020
#define	REG_ATOI	255	/* convert name to number (!) */
#define	REG_ITOA	0400	/* convert number to name (!) */

/* regexec() flags */
#define	REG_NOTBOL	0001	/* ^ isn't line start */
#define	REG_NOTEOL	0002	/* $ isn't line end */
#define	REG_STARTEND	00004
#define	REG_TRACE	00400	/* tracing of execution */
#define	REG_LARGE	01000	/* force large representation */
#define	REG_BACKR	02000	/* force use of backref code */

/*
 *  MIPS ABI and X/Open regex_t structure.
 *  This structure conforms to the MIPS ABI BB2.0
 *  specification.  Applications that are recompiled
 *  will use this structure by default.
 */
typedef struct
{
	size_t re_nsub;		/* number of parenthesized subexpressions */
	unsigned char *re_pad[9];
} regex_t;

#if _SGIAPI
/*
 *  SGI implementation specific regex_t structure.
 *  Old applications which haven't been recompiled
 *  already are using this structure.
 */
typedef struct
{
	int re_magic;
	size_t re_nsub;		/* number of parenthesized subexpressions */
	const char *re_endp;	/* end pointer for REG_PEND */
	struct re_guts *re_g;	/* none of your business :-) */
} sgi_regex_t;
#endif	/* _SGIAPI */

#if  _MIPS_SIM == _ABIN32
typedef	__int64_t	regoff_t;
#else
typedef	ssize_t		regoff_t;
#endif

typedef struct
{
	regoff_t	rm_so;
	regoff_t	rm_eo;
} regmatch_t;

/*
 *  Applications recompiled with this include file will use
 *  the new routines: __nregcomp(), __nregexec(), __nregerror() and 
 *  __nregfree().  Old application binaries will use the old routines
 *  in libc: regcomp(), regexec(), regerror() and regfree().
 */
int	__nregcomp(regex_t *, const char *, int);
int	__nregexec(const regex_t *, const char *, size_t, regmatch_t *, int);
size_t	__nregerror(int, const regex_t *, char *, size_t);
void	__nregfree(regex_t *);

#ifndef _REGCOMP_INTERNAL

#if _ABIAPI
int	_xregcomp(const int, regex_t *, const char *, int);
int	_xregexec(const int, const regex_t *, const char *, size_t, regmatch_t *, int);
size_t	_xregerror(const int, int, const regex_t *, char *, size_t);
void	_xregfree(const int, regex_t *);
#define _MIPSABI_REGEX_VER 2

/*REFERENCED*/
static int
regcomp(regex_t *__preg, const char *__pattern, int __cflags)
{
	return _xregcomp(_MIPSABI_REGEX_VER, __preg, __pattern, __cflags);
}

/*REFERENCED*/
static int
regexec(const regex_t *__preg, const char *__string, size_t __nmatch,
		regmatch_t *__pmatch, int __eflags)
{
	return _xregexec(_MIPSABI_REGEX_VER, __preg, __string, __nmatch, __pmatch, __eflags);
}

/*REFERENCED*/
static size_t
regerror(int __errcode, const regex_t *__preg, char *__errbuf,
		size_t __errbuf_size)
{
	return _xregerror(_MIPSABI_REGEX_VER, __errcode, __preg, __errbuf, __errbuf_size);
}

/*REFERENCED*/
static void
regfree(regex_t *__preg)
{
	_xregfree(_MIPSABI_REGEX_VER, __preg);
}

#else /* _ABIAPI */

/*REFERENCED*/
static int
regcomp(regex_t *__preg, const char *__pattern, int __cflags)
{
	return __nregcomp(__preg, __pattern, __cflags);
}

/*REFERENCED*/
static int
regexec(const regex_t *__preg, const char *__string, size_t __nmatch,
		regmatch_t *__pmatch, int __eflags)
{
	return __nregexec(__preg, __string, __nmatch, __pmatch, __eflags);
}

/*REFERENCED*/
static size_t
regerror(int __errcode, const regex_t *__preg, char *__errbuf,
		size_t __errbuf_size)
{
	return __nregerror(__errcode, __preg, __errbuf, __errbuf_size);
}

/*REFERENCED*/
static void
regfree(regex_t *__preg)
{
	__nregfree(__preg);
}
#endif /* _ABIAPI */
#endif	/* _REGCOMP_INTERNAL */

#ifdef __cplusplus
}
#endif

#endif /*_REGEX_H*/
