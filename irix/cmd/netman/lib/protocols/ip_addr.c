/*
 * Copyright 1989 Silicon Graphics, Inc.  All rights reserved.
 *
 * 4.3BSD inet_addr fixed to return the address in host order, and to return
 * it via a result parameter, rather than directly.  Thus the return value is
 * not overloaded with error status (-1L), which might be a plausible Internet
 * address (universal broadcast).  Instead, ip_addr returns null on error and
 * a pointer to the next char after the address on success.
 */
#include <ctype.h>
#include <sys/types.h>
#include <netinet/in.h>

char *
ip_addr(char *cp, u_long *ipa)
{
	u_long *pp, parts[4];
	u_long val, base;
	char c;

	pp = parts;
	for (;;) {
		/*
		 * Collect number up to '.'.  Values are specified as for C.
		 */
		val = 0, base = 10;
		if (*cp == '0')
			base = 8, cp++;
		if (*cp == 'x' || *cp == 'X')
			base = 16, cp++;
		while (c = *cp) {
			if (isdigit(c)) {
				val *= base;
				val += c - '0';
			} else if (base == 16 && isxdigit(c)) {
				val *= base;
				val += 10 + c - (islower(c) ? 'a' : 'A');
			} else
				break;
			cp++;
		}
		if (*cp != '.')
			break;
		/*
		 * Internet format allows at most 4 parts:
		 *	a.b.c.d
		 *	a.b.c	(with c treated as 16-bits)
		 *	a.b	(with b treated as 24 bits)
		 */
		if (pp >= parts + 4)
			return 0;
		*pp++ = val, cp++;
	}

	/*
	 * Check for bad trailing characters.
	 */
	if (*cp && !isspace(*cp))
		return 0;
	*pp++ = val;

	/*
	 * Concoct the address according to the number of parts specified.
	 */
	switch (pp - parts) {
	  case 1:		/* a: 32 bits */
		val = parts[0];
		break;

	  case 2:		/* a.b: 8.24 bits */
		val = (parts[0] << 24) | (parts[1] & 0xffffff);
		break;

	  case 3:		/* a.b.c: 8.8.16 bits */
		val = (parts[0] << 24) | ((parts[1] & 0xff) << 16)
		    | (parts[2] & 0xffff);
		break;

	  case 4:		/* a.b.c.d: 8.8.8.8 bits */
		val = (parts[0] << 24) | ((parts[1] & 0xff) << 16)
		    | ((parts[2] & 0xff) << 8) | (parts[3] & 0xff);
	}
	*ipa = val;
	return cp;
}
