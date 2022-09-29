/*
 * Copyright (c) 1985 by Sun Microsystems, Inc.
 *
 * All routines necessary to deal with the file /etc/ethers.  The file
 * contains mappings from 48 bit ethernet addresses to their corresponding
 * hosts name.  The addresses have an ascii representation of the form
 * "x:x:x:x:x:x" where x is a hex number between 0x00 and 0xff;  the
 * bytes are always in network order.
 */

#ifdef __STDC__
	#pragma weak ether_line = _ether_line
	#pragma weak ether_ntoa = _ether_ntoa
	#pragma weak ether_aton = _ether_aton
	#pragma weak ether_hostton = _ether_hostton
	#pragma weak ether_ntohost = _ether_ntohost
#endif
#include "synonyms.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <rpc/rpc.h>			/* required for prototyping */
#include <rpcsvc/yp_prot.h>		/* required for prototyping */
#include <rpcsvc/ypclnt.h>
#include <string.h>			/* prototype for strlen() */
#include <bstring.h>			/* prototype for bcmp() */

/*
 * In SUN's case, this is in <netinet/if_ether.h>.  In 4.3, the
 * definition was removed and references replaced with "u_char foo[6]".
 * Let's try to avoid putting it in the include file and hope that
 * SUN code that calls this routine doesn't require struct ether_addr.
 * If this assumption is wrong (as it probably is), we will have to
 * bite the bullet.
 */
struct ether_addr {
	unsigned char ether_addr_octet[6];
};
static const char ethers[] = "/etc/ethers";

/* forward references */
int ether_line(char *, struct ether_addr *, char *);
char *ether_ntoa(struct ether_addr *);

/*
 * Parses a line from /etc/ethers into its components.  The line has the form
 * 8:0:20:1:17:c8	krypton
 * where the first part is a 48 bit ethernet addrerss and the second is
 * the corresponding hosts name.
 * Returns zero if successful, non-zero otherwise.
 */
int
ether_line(char *s,		/* the string to be parsed */
	struct ether_addr *e,	/* ethernet address struct to be filled in */
	char *hostname)		/* hosts name to be set */
{
	register int i;
	unsigned int t[6];
	
	i = sscanf(s, " %x:%x:%x:%x:%x:%x %s",
	    &t[0], &t[1], &t[2], &t[3], &t[4], &t[5], hostname);
	if (i != 7) {
		return (7 - i);
	}
	for (i = 0; i < 6; i++)
		e->ether_addr_octet[i] = (unsigned char)t[i];
	return (0);
}

/*
 * Converts a 48 bit ethernet number to its string representation.
 */
#define EI(i)	(unsigned int)(e->ether_addr_octet[(i)])
char *
ether_ntoa(struct ether_addr *e)
{
	static char s[18] _INITBSSS;

	s[0] = 0;
	sprintf(s, "%x:%x:%x:%x:%x:%x",
	    EI(0), EI(1), EI(2), EI(3), EI(4), EI(5));
	return (s);
}

/*
 * Converts a ethernet address representation back into its 48 bits.
 */
struct ether_addr *
ether_aton(char *s)
{
	static struct ether_addr e _INITBSSS;
	register int i;
	unsigned int t[6];
	
	i = sscanf(s, " %x:%x:%x:%x:%x:%x",
	    &t[0], &t[1], &t[2], &t[3], &t[4], &t[5]);
	if (i != 6)
	    return ((struct ether_addr *)NULL);
	for (i = 0; i < 6; i++)
		e.ether_addr_octet[i] = (unsigned char)t[i];
	return (&e);
}

/*
 * Given a host's name, this routine returns its 48 bit ethernet address.
 * Returns zero if successful, non-zero otherwise.
 */
int
ether_hostton(char *host,	/* function input */
	struct ether_addr *e)	/* function output */
{
	char currenthost[256];
	char buf[512];
	char *val = buf;
	int vallen;
	int error;
	FILE *f;
	
	if (_yellowup(0)) {
		error = yp_match(_yp_domain, "ethers.byname",
				 host, (int)strlen(host), &val, &vallen);
		if (!error) {
			error = ether_line(val, e, currenthost);
			free(val);  /* yp_match always allocates for us */
			return (error);
		}
		if (error != YPERR_DOMAIN) {
			return (error);
		}
	}
	if ((f = fopen(ethers, "r")) == NULL) {
		return (-1);
	}
	error = -1;
	while (fscanf(f, "%[^\n] ", val) == 1) {
		if ((ether_line(val, e, currenthost) == 0) &&
		    (strcmp(currenthost, host) == 0)) {
			error = 0;
			break;
		}
	}
	fclose(f);
	return (error);
}

/*
 * Given a 48 bit ethernet address, this routine return its host name.
 * Returns zero if successful, non-zero otherwise.
 */
int
ether_ntohost(char *host,	/* function output */
	struct ether_addr *e)	/* function input */
{
	struct ether_addr currente;
	char *a = ether_ntoa(e);
	char buf[512];
	char *val = buf;
	int vallen;
	int error;
	FILE *f;
	
	if (_yellowup(0)) {
		error = yp_match(_yp_domain, "ethers.byaddr",
				 a, (int)strlen(a), &val, &vallen);
		if (!error) {
			error = ether_line(val, &currente, host);
			free(val);  /* yp_match always allocates for us */
			return (error);
		}
		if (error != YPERR_DOMAIN) {
			return (error);
		}
	}
	if ((f = fopen(ethers, "r")) == NULL) {
		return (-1);
	}
	error = -1;
	while (fscanf(f, "%[^\n] ", val) == 1) {
		if ((ether_line(val, &currente, host) == 0) &&
		    (bcmp(e, &currente, sizeof(currente)) == 0)) {
			error = 0;
			break;
		}
	}
	fclose(f);
	return (error);
}
