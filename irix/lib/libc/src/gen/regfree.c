/* regfree.c
 * $Header: /proj/irix6.5.7m/isms/irix/lib/libc/src/gen/RCS/regfree.c,v 1.10 1998/06/12 22:58:35 holzen Exp $
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

 * Copyright (c) 1992, 1993, 1994 Henry Spencer.
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Henry Spencer.
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
 *	@(#)regfree.c	8.3 (Berkeley) 3/20/94
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)regfree.c	8.3 (Berkeley) 3/20/94";
#endif /* LIBC_SCCS and not lint */

#ident "$Revision: 1.10 $"

#include <sys/types.h>
#include "synonyms.h"
#include <stdio.h>
#include <stdlib.h>
#define	_REGCOMP_INTERNAL		/* keep out static functions */
#include <regex.h>

#ifdef __STDC__
        #pragma weak regfree = _regfree
#endif


#include "utils.h"
#include "regex2.h"

static void __regfree(sgi_regex_t *);

void
regfree(sgi_regex_t *preg)
{
	/*
	 * This is the backward compatible version of regfree().  This will
	 * always get called from an application which was link-loaded using
	 * a previous version of libc.so.
	 */
	__regfree(preg);
}

/* BB3.0 Version */
/* ARGSUSED */
void
_xregfree(const int ver, regex_t *preg)
{
	__nregfree(preg);
}

void
__nregfree(regex_t *preg)
{
	sgi_regex_t	oregex;
	sgi_regex_t	*oregexp;
	unsigned char *cp1, *cp2;
	int i;

	/*
	 * Convert the MIPS ABI/XPG4 version of the regex_t structure into 
	 * the version used for SGI's implementation before 
	 * calling __regfree().
	 */

	/* new to old conversion */
	oregexp = &oregex;
	oregexp->re_nsub = preg->re_nsub;
		
	/*
	 * Store the re_pad[0] element from the regex_t structure
	 * into the re_magic struct element of the sgi_regex_t structure.
	 */
	cp1 = (unsigned char *)&preg->re_pad;
	cp2 = (unsigned char *)&oregexp->re_magic;
	for(i=0; i<sizeof(oregexp->re_magic); i++)
		*cp2++ = *cp1++;

	oregexp->re_endp = (const char *)preg->re_pad[1];
        oregexp->re_g = (struct re_guts *)preg->re_pad[2];

	__regfree(oregexp);

	/* old to new conversion */
	preg->re_nsub = oregexp->re_nsub;

	/*
	 * Store the re_magic element from the sgi_regex_t structure
	 * into the re_pad[0] struct element of the regex_t structure.
	 */
	cp1 = (unsigned char *)&preg->re_pad;
	cp2 = (unsigned char *)&oregexp->re_magic;
	for(i=0; i<sizeof(oregexp->re_magic); i++)
		*cp1++ = *cp2++;

	preg->re_pad[1] = (unsigned char *)oregexp->re_endp;
	preg->re_pad[2] = (unsigned char *)oregexp->re_g;
}

/*
 - regfree - free everything
 = extern void regfree(regex_t *);
 */
void
__regfree(sgi_regex_t *preg)
{
	register struct re_guts *g;

	if (preg->re_magic != MAGIC1)	/* oops */
		return;			/* nice to complain, but hard */

	g = preg->re_g;
	if (g == NULL || g->magic != MAGIC2)	/* oops again */
		return;
	preg->re_magic = 0;		/* mark it invalid */
	g->magic = 0;			/* mark it invalid */

	if (g->strip != NULL)
		free((char *)g->strip);
	if (g->sets != NULL)
		free((char *)g->sets);
	if (g->setbits != NULL)
		free((char *)g->setbits);
	if (g->must != NULL)
		free(g->must);
	if (g->mbs != NULL)
	        free(g->mbs);
	free((char *)g);
}
