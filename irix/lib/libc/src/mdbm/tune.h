/*
 * mdbm - ndbm work-alike hashed database library
 * tuning and portability constructs [not nearly enough]
 *
 * Copyright (c) 1991 by Ozan S. Yigit
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * This file is provided AS IS with no warranties of any kind.  The author
 * shall have no liability with respect to the infringement of copyrights,
 * trade secrets or any patents by this file or any part thereof.  In no
 * event will the author be liable for any lost revenue or profits or
 * other special, indirect and consequential damages.
 *
 *			oz@nexus.yorku.ca
 *
 *			Ozan S. Yigit
 *			Dept. of Computing and Communication Svcs.
 *			Steacie Science Library. T103
 *			York University
 *			4700 Keele Street, North York
 *			Ontario M3J 1P3
 *			Canada
 */

#ifndef _MDBM_TUNE_H_
#define _MDBM_TUNE_H_

#define	BITS_PER_BYTE		8

#ifdef	SVID
#include <unistd.h>
#endif

#ifdef BSD42
#define	SEEK_SET	L_SET
#define	memset(s, c, n)		bzero(s, n)	/* only when c is zero */
#define	memcpy(s1, s2, n)	bcopy(s2, s1, n)
#define	memcmp(s1, s2, n)	bcmp(s1, s2, n)
#endif

/*
 * important tuning parms (hah)
 */

#define	SEEDUPS			/* always detect duplicates */
#define	BADMESS			/* generate a message for worst case:
				    cannot make room after SPLTMAX splits */

/*
 * Debugging support
 */
#define	debug(x,y)
#define ASSERT(x)

/* Always debug these */
#define DEBUG_IOERR	0x0001
#define DEBUG_ERROR	0x0002

/* What info to debug */
#define DEBUG_ENTRY	0x0010
#define DEBUG_INFO	0x0020
#define DEBUG_DETAIL	0x0040
#define DEBUG_GUTS	0x0080

/* What sections to debug */
#define DEBUG_CORE	0x0100
#define DEBUG_GL	0x0200
#define DEBUG_GL_FIND	0x0400
#define DEBUG_CHAIN	0x0800
#define DEBUG_ATOMIC	0x1000
#define DEBUG_UTIL	0x2000
#define DEBUG_SUPPORT	0x4000
#define DEBUG_REPAIR	0x8000

#ifdef	DEBUG
#undef debug
#define debug(x,y) if (DEBUG & (x)) printf y

#ifndef TRACE_MMAP
#define TRACE_MMAP
#endif

#include <assert.h>
#undef ASSERT
#define ASSERT assert

#endif /* DEBUG */

#endif /*  _MDBM_TUNE_H_ */
