#include <sys/types.h>
#include <net/in.h>
#include <ctype.h>
#include <libsc.h>

/*	inet_makeaddr.c	4.3	82/11/14	*/

/* XXX -- override net/in.h to quiet ragnarok cmplrs */
#undef IN_CLASSA
#undef IN_CLASSB
#define	IN_CLASSA(i)		(((unsigned int)(i) & 0x80000000) == 0)
#define	IN_CLASSB(i)		(((unsigned int)(i) & 0xc0000000) == 0x80000000)

#ifndef _STANDALONE
/*
 * Formulate an Internet address from network + host.  Used in
 * building addresses stored in the ifnet structure.
 */
struct in_addr
inet_makeaddr(register int net, register int host)
{
	unsigned int addr;

	if (net < 128)
		addr = (net << IN_CLASSA_NSHIFT) | host;
	else if (net < 65536)
		addr = (net << IN_CLASSB_NSHIFT) | host;
	else
		addr = (net << IN_CLASSC_NSHIFT) | host;
	addr = htonl(addr);
	return (*(struct in_addr *)&addr);
}

/*
 * Internet network address interpretation routine.
 * The library routines call this routine to interpret
 * network numbers.
 */
unsigned int
inet_network(register char *cp)
{
	register unsigned int val, base, n;
	register char c;
	unsigned int parts[4], *pp = parts;
	register int i;

again:
	val = 0; base = 10;
	if (*cp == '0')
		base = 8, cp++;
	if (*cp == 'x' || *cp == 'X')
		base = 16, cp++;
	while (c = *cp) {
		if (isdigit(c)) {
			val = (val * base) + (c - '0');
			cp++;
			continue;
		}
		if (base == 16 && isxdigit(c)) {
			val = (val << 4) + (c + 10 - (islower(c) ? 'a' : 'A'));
			cp++;
			continue;
		}
		break;
	}
	if (*cp == '.') {
		if (pp >= parts + 4)
			return (-1);
		*pp++ = val, cp++;
		goto again;
	}
	if (*cp && !isspace(*cp))
		return (-1);
	*pp++ = val;
	n = pp - parts;
	if (n > 4)
		return (-1);
	for (val = 0, i = 0; i < n; i++) {
		val <<= 8;
		val |= parts[i] & 0xff;
	}
	return (val);
}
#endif /* !_STANDALONE */

/*
 * Return the local network address portion of an
 * internet address; handles class a/b/c network
 * number formats.
 */
unsigned int
inet_lnaof(struct in_addr in)
{
	register unsigned int i = ntohl(in.s_addr);

	if (IN_CLASSA(i))
		return ((i)&IN_CLASSA_HOST);
	else if (IN_CLASSB(i))
		return ((i)&IN_CLASSB_HOST);
	else
		return ((i)&IN_CLASSC_HOST);
}

/*
 * Internet address interpretation routine.
 * All the network library routines call this
 * routine to interpret entries in the data bases
 * which are expected to be an address.
 * The value returned is in network order.
 */
struct in_addr
inet_addr(register char *cp)
{
	register unsigned int val, base, n;
	register char c;
	unsigned int parts[4], *pp = parts;
	struct in_addr in_addr;

again:
	/*
	 * Collect number up to ``.''.
	 * Values are specified as for C:
	 * 0x=hex, 0=octal, other=decimal.
	 */
	val = 0; base = 10;
	if (*cp == '0')
		base = 8, cp++;
	if (*cp == 'x' || *cp == 'X')
		base = 16, cp++;
	while (c = *cp) {
		if (isdigit(c)) {
			val = (val * base) + (unsigned int)(c - '0');
			cp++;
			continue;
		}
		if (base == 16 && isxdigit(c)) {
			val = (val << 4) +
			      (unsigned int)(c + 10 - (islower(c) ? 'a' : 'A'));
			cp++;
			continue;
		}
		break;
	}
	if (*cp == '.') {
		/*
		 * Internet format:
		 *	a.b.c.d
		 *	a.b.c	(with c treated as 16-bits)
		 *	a.b	(with b treated as 24 bits)
		 */
		if (pp >= parts + 4) {
			in_addr.s_addr = (u_int) -1;
			return (in_addr);
		}
		*pp++ = val, cp++;
		goto again;
	}
	/*
	 * Check for trailing characters.
	 */
	if (*cp && !isspace(*cp)) {
		in_addr.s_addr = (u_int) -1;
		return (in_addr);
	}
	*pp++ = val;
	/*
	 * Concoct the address according to
	 * the number of parts specified.
	 */
	n = (unsigned int)(pp - parts);
	switch (n) {

	case 1:				/* a -- 32 bits */
		val = parts[0];
		break;

	case 2:				/* a.b -- 8.24 bits */
		val = (parts[0] << 24) | (parts[1] & 0xffffff);
		break;

	case 3:				/* a.b.c -- 8.8.16 bits */
		val = (parts[0] << 24) | ((parts[1] & 0xff) << 16) |
			(parts[2] & 0xffff);
		break;

	case 4:				/* a.b.c.d -- 8.8.8.8 bits */
		val = (parts[0] << 24) | ((parts[1] & 0xff) << 16) |
		      ((parts[2] & 0xff) << 8) | (parts[3] & 0xff);
		break;

	default:
		in_addr.s_addr = (u_int) -1;
		return (in_addr);
	}
	val = htonl(val);
	in_addr.s_addr = val;
	return (in_addr);
}

#ifndef _STANDALONE
/*
 * Return the network number from an internet
 * address; handles class a/b/c network #'s.
 */
int
inet_netof(struct in_addr in)
{
	register unsigned int i = ntohl(in.s_addr);

	if (IN_CLASSA(i))
		return (((i)&IN_CLASSA_NET) >> IN_CLASSA_NSHIFT);
	else if (IN_CLASSB(i))
		return (((i)&IN_CLASSB_NET) >> IN_CLASSB_NSHIFT);
	else
		return (((i)&IN_CLASSC_NET) >> IN_CLASSC_NSHIFT);
}
#endif /* !_STANDALONE */

/*
 * Convert network-format internet address
 * to base 256 d.d.d.d representation.
 */
char *
inet_ntoa(struct in_addr in)
{
	static char b[18];
	register char *p;

	p = (char *)&in;
#define	UC(b)	(((int)b)&0xff)
	sprintf(b, "%d.%d.%d.%d", UC(p[0]), UC(p[1]), UC(p[2]), UC(p[3]));
	return (b);
}
