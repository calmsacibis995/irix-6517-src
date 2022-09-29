/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifdef __STDC__
	#pragma weak inet_network = _inet_network
#endif
#include "synonyms.h"

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)inet_network.c	5.5 (Berkeley) 6/27/88";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <netinet/in.h>
#include <ctype.h>

/*
 * Internet network address interpretation routine.
 * The library routines call this routine to interpret
 * network numbers.
 */
u_long
inet_network(register char *cp)
{
	register u_long val, base, n;
	register char c;
	u_long parts[4], *pp = parts;
	register int i;

	if (!cp || !*cp) {
		return (INADDR_NONE);
	}
again:
	val = 0; base = 10;
	if (*cp == '0') {
		cp++;
		if (*cp == 'x' || *cp == 'X')
			base = 16, cp++;
		else
		    base = 8;
	}
	while (c = *cp) {
		if (c == '.') {
			break;
		}
		if (isdigit(c)) {
			if ((c - '0') >= base) {
				return (INADDR_NONE);
			}
			val = (val * base) + (u_long)(c - '0');
			cp++;
			continue;
		}
		if (base == 16 && isxdigit(c)) {
			val = (val << 4) + (u_long)(c + 10 - (islower(c) ? 'a' : 'A'));
			cp++;
			continue;
		}

		/* bogus character */
		if (!val && isspace(c)) {
			cp++;
		} else {
			break;
		}
	}
	if (*cp == '.') {
		if (pp >= parts + 4)
			return (INADDR_NONE);
		*pp++ = val, cp++;
		goto again;
	}
	if (*cp && !isspace(*cp))
		return (INADDR_NONE);
	*pp++ = val;
	n = (u_long)(pp - parts);
	if (n > 4)
		return (INADDR_NONE);
	for (val = 0, i = 0; i < (int)n; i++) {
		val <<= 8;
		val |= parts[i] & 0xff;
	}
	return (val);
}
