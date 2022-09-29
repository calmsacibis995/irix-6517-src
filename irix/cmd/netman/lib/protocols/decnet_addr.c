/*
 * Copyright 1989 Silicon Graphics, Inc.  All rights reserved.
 *
 * rp_addr returns null on error and a pointer to the next char after
 * the address on success.
 */
#include <ctype.h>
#include <sys/types.h>
#include <protocols/decnet.h>

/*
 * DECnet Nodes addresses are composed of an area (1 to 63) and a node
 * (1 to 1023) number separated by a single dot. A valid  address would
 * be: 1.234
 *
 * Translate a DECnet dot-notation string into an unsigned short RP address
 * in host order.  Return updated string pointer or 0 on error.
 */

#define	MAX_AREA_NUM	63
#define	MAX_NODE_NUM	1063

#define BETWEEN(n, min, max)	((min <= n) && (n <= max))


char *
rp_addr(char *cp, u_short *addr)
{
	u_short *pp, parts[2];
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
		 * DECnet format allows at most 2 parts:
		 *	a.b	(with b treated as 10 bits)
		 */
		if (pp >= parts + 2)
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
	  case 1:		/* a: 10 bits */
		if ( !BETWEEN(parts[0], 1, MAX_AREA_NUM) )
			return 0;
		val = parts[0];
		break;

	  case 2:		/* a.b: 6.10 bits */
		if ( !BETWEEN(parts[0], 1, MAX_AREA_NUM) 
		    || !BETWEEN(parts[1], 1, MAX_NODE_NUM) )
			return 0;
		val = (parts[0] << 10) | (parts[1] & 0x03ff);
		break;
	}
	*addr = val;
	return cp;
}
