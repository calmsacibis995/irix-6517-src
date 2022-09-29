/* stdioP.h - header file for ANSI stdio library */

/* Copyright 1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01d,14nov92,jcf  removed redundant prototypes.
01c,22sep92,rrr  added support for c++
01b,02aug92,jcf  moved srget/swbuf prototypes to stdio.h.
01a,29jul92,jcf  added stdio internal routines to manage memory.
	    smb  taken from UCB stdio library.
*/

#ifndef __INCstdioPh
#define __INCstdioPh

#ifdef __cplusplus
extern "C" {
#endif

/*
DESCRIPTION
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
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
 *	@(#)local.h	5.2 (Berkeley) 2/5/91
 *
 * Information local to this implementation of stdio,
 * in particular, macros and private variables.
 *
SEE ALSO: American National Standard X3.159-1989
*/

#include "stdio.h"


/*
 * This is a #define because the function is used internally and
 * (unlike vfscanf) the name __svfscanf is guaranteed not to collide
 * with a user function when _ANSI_SOURCE or _POSIX_SOURCE is defined.
 */

#define vfscanf	__svfscanf

/* Routines that are purely local to the implementation */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS	stdioFpDestroy (FILE *fp);
extern FILE *	stdioFpCreate (void);
extern char *	fgetline (FILE *, size_t *);
extern int	fpurge (FILE *);
extern int	pclose (FILE *);
extern FILE *	popen (const char *, const char *);
extern char *	tempnam (const char *, const char *);
extern int	snprintf (char *, size_t, const char *, ...);
extern int	vsnprintf (char *, size_t, const char *, va_list);
extern int	vscanf (const char *, va_list);
extern int	vsscanf (const char *, const char *, va_list);
extern int	__svfscanf (FILE *, const char *, va_list);
extern int	__sflush (FILE *);
extern FILE *	__sfp (void);
extern int	__srefill (FILE *);
extern int	__sread (FILE *, char *, int);
extern int	__swrite (FILE *, char const *, int);
extern fpos_t	__sseek (FILE *, fpos_t, int);
extern int	__sclose (FILE *);
extern void	__sinit (void);
extern void	__smakebuf (FILE *);
extern int	__swsetup (FILE *);
extern int	__sflags (const char *, int *);

#else /* __STDC__ */

extern STATUS	stdioFpDestroy ();
extern FILE *	stdioFpCreate ();
extern char *	fgetline ();
extern int	fpurge ();
extern int	pclose ();
extern FILE *	popen ();
extern char *	tempnam ();
extern int	snprintf ();
extern int	vsnprintf ();
extern int	vscanf ();
extern int	vsscanf ();
extern int	__svfscanf ();
extern int	__sflush ();
extern FILE *	__sfp ();
extern int	__srefill ();
extern int	__sread ();
extern int	__swrite ();
extern fpos_t	__sseek ();
extern int	__sclose ();
extern void	__sinit ();
extern void	__smakebuf ();
extern int	__swsetup ();
extern int	__sflags ();

#endif /* __STDC__ */

/* Return true iff the given FILE cannot be written now.  */

#define	cantwrite(fp) 							\
	((((fp)->_flags & __SWR) == 0 || (fp)->_bf._base == NULL) && 	\
	 __swsetup(fp))

/* Test whether the given stdio file has an active ungetc buffer;
 * release such a buffer, without restoring ordinary unread data.
 */

#define	HASUB(fp) ((fp)->_ub._base != NULL)
#define	FREEUB(fp) 				\
    {						\
    if ((fp)->_ub._base != (fp)->_ubuf)		\
	free ((char *)(fp)->_ub._base);		\
    (fp)->_ub._base = NULL;			\
    }

/* test for an fgetline() buffer.  */

#define	HASLB(fp) ((fp)->_lb._base != NULL)
#define	FREELB(fp) 				\
    {						\
    free ((char *)(fp)->_lb._base);		\
    (fp)->_lb._base = NULL;			\
    }

#ifdef __cplusplus
}
#endif

#endif /* __INCstdioPh */
