/*
 *   CENTER FOR THEORY AND SIMULATION IN SCIENCE AND ENGINEERING
 *			CORNELL UNIVERSITY
 *
 *      Portions of this software may fall under the following
 *      copyrights: 
 *
 *	Copyright (c) 1983 Regents of the University of California.
 *	All rights reserved.  The Berkeley software License Agreement
 *	specifies the terms and conditions for redistribution.
 *
 *  GATED - based on Kirton's EGP, UC Berkeley's routing daemon (routed),
 *	    and DCN's HELLO routing Protocol.
 */

#ifndef	lint
static char *rcsid = "$Header: /proj/irix6.5.7m/isms/eoe/cmd/bsd/gated/RCS/af.c,v 1.2 1989/11/13 16:14:39 jleong Exp $";
#endif	not lint

/*
 *  modified and extracted from 4.3BSD routed sources
 */

#include "include.h"

/*
 * Address family support routines
 */

int	inet_hash(), inet_netmatch(), inet_output(),
	inet_portmatch(), inet_portcheck(),
	inet_checkhost(), inet_ishost(), inet_canon();
char	*inet_format();
#define NIL	{ 0 }
#define	INET \
	{ inet_hash,		inet_netmatch,		inet_output, \
	  inet_portmatch,	inet_portcheck,		inet_checkhost, \
	  inet_ishost,		inet_canon, 		inet_format }

struct afswitch afswitch[AF_MAX] =
	{ NIL, NIL, INET, };

/*
 * hash routine for the route table.
 */

inet_hash(sin, hp)
	register struct sockaddr_in *sin;
	struct afhash *hp;
{
  register u_long n;

  n = gd_inet_netof(sin->sin_addr);
  if (n)
    while ((n & 0xff) == 0)
      n >>= 8;
  hp->afh_nethash = n;
  hp->afh_hosthash = ntohl(sin->sin_addr.s_addr);
  hp->afh_hosthash &= 0x7fffffff;
}

inet_netmatch(sin1, sin2)
	struct sockaddr_in *sin1, *sin2;
{
  return(gd_inet_netof(sin1->sin_addr) == gd_inet_netof(sin2->sin_addr));
}

/*
 * Verify the message is from the right port.
 */

inet_portmatch(sin)
	register struct sockaddr_in *sin;
{
#ifndef	NSS
  return(sin->sin_port == sp->s_port);
#endif	NSS
}

/*
 * Verify the message is from a "trusted" port.
 */

inet_portcheck(sin)
	struct sockaddr_in *sin;
{
  return (ntohs(sin->sin_port) <= IPPORT_RESERVED);
}

/*
 * Internet output routine.
 */

inet_output(sr, flags, sin, size)
	int sr, flags;
	struct sockaddr_in *sin;
	int size;
{
#ifndef	NSS
  struct sockaddr_in dst;

  dst = *sin;
  sin = &dst;
  if (sin->sin_port == 0)
    sin->sin_port = sp->s_port;
  if (sendto(sr, rip_packet, size, flags, (struct sockaddr *)sin, sizeof (*sin)) < 0) {
    (void) sprintf(err_message, "inet_output: sendto() error sending %d bytes to %s",
      size, inet_ntoa(dst.sin_addr));
    p_error(err_message);
  }
#else	NSS
  TRACE_TRC("inet_output called at %s\n", strtime);
  abort();
#endif	NSS
}

/*
 * Return 1 if the address is believed
 * for an Internet host -- THIS IS A KLUDGE.
 */

inet_checkhost(sin)
	struct sockaddr_in *sin;
{
  u_long i = ntohl(sin->sin_addr.s_addr);

#ifndef IN_BADCLASS
#define	IN_BADCLASS(i)	(((long) (i) & 0xe0000000) == 0xe0000000)
#endif	IN_BADCLASS

  if (IN_BADCLASS(i) || sin->sin_port != 0)
    return (0);
  if (i != 0 && (i & 0xff000000) == 0)
    return (0);
  for (i = 0; i < sizeof(sin->sin_zero)/sizeof(sin->sin_zero[0]); i++)
    if (sin->sin_zero[i])
      return (0);
  return (1);
}

/*
 * Return 1 if the address is
 * for an Internet host, 0 for a network.
 */

inet_ishost(sin)
	struct sockaddr_in *sin;
{
  return(gd_inet_lnaof(sin->sin_addr) != 0);
}

inet_canon(sin)
	struct sockaddr_in *sin;
{
  sin->sin_port = 0;
}

char *
inet_format(sin)
	struct sockaddr_in *sin;
{
#ifndef sgi
  char *inet_ntoa();
#endif

  return(inet_ntoa(sin->sin_addr));
}
