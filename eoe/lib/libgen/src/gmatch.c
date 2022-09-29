/*
 * gmatch.c
 *
 *
 * Copyright 1991, Silicon Graphics, Inc.
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

#ident "$Revision: 1.4 $"


#ifdef __STDC__
	#pragma weak gmatch = _gmatch
#endif
#include "synonyms.h"

#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include "_wchar.h"
#include "_range.h"
#define	Popwchar(p, c) \
n = mbtowc(&cl, p, MB_LEN_MAX); \
c = cl; \
if(n <= 0) \
	return 0; \
p += n;

int
gmatch(register const char *s, register const char *p)
{
	register const char *olds;
	register wchar_t scc, c;
	register int n;
	wchar_t cl;

	olds = s;
	n = mbtowc(&cl, s, MB_LEN_MAX);
	if(n <= 0) {
		s++;
		scc = n;
	} else {
		scc = cl;
		s += n;
	}
	n = mbtowc(&cl, p, MB_LEN_MAX);
	if(n < 0)
		return(0);
	if(n == 0)
		return(scc == 0);
	p += n;
	c = cl;
	
	switch(c) 
	{
	case '[':
		if(scc <= 0)
			return(0);
	{
			int ok;
			wchar_t lc = 0;
			int notflag = 0;

			ok = 0;
			if (*p == '!')
			{
				notflag = 1;
				p++;
			}
			Popwchar(p, c)
			do
			{
				if (c == '-' && lc && *p!= ']')
				{
					Popwchar(p, c)
					if(c == '\\') {
						Popwchar(p, c)
					}
					if (notflag)
					{
						if(!multibyte || valid_range(lc, c)) {
							if (scc < lc || scc > c)
								ok++;
							else
								return(0);
						}
					} else {
						if(!multibyte || valid_range(lc, c))
							if (lc <= scc && scc <= c)
								ok++;
					}
				}
				else if(c == '\\') { /* skip to quoted character */
					Popwchar(p, c)
				}
				lc = c;
				if (notflag)
				{
					if (scc != lc)
						ok++;
					else
						return(0);
				}
				else
				{
					if (scc == lc)
						ok++;
				}
				Popwchar(p, c)
			} while (c != ']');
			return(ok ? gmatch(s,p) : 0);
		}

	case '\\':	
		Popwchar(p, c) /* skip to quoted character and see if it matches */
	default:
		if (c != scc)
			return(0);

	case '?':
		return(scc > 0 ? gmatch(s, p) : 0);

	case '*':
		while (*p == '*')
			p++;

		if (*p == 0)
			return(1);
		s = olds;
		while (*s)
		{
			if (gmatch(s, p))
				return(1);
			n = mbtowc(&cl, s, MB_LEN_MAX);
			if(n < 0)
				/* skip past illegal byte sequence */
				s++;
			else
				s += n;
		}
		return(0);
	}
}
