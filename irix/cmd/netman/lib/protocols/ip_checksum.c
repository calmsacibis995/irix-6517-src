/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 * Internet Protocol checksum routines.
 */
#include <sys/types.h>
#include "protocols/ip.h"

static u_long
ip_partialsum(char *p, int len)
{
	u_long sum;
	u_short *sp;

	sum = 0;
	if (len & 01) {
		p[len] = 0;
		len++;
	}
	len /= 2;
	for (sp = (u_short *) p; --len >= 0; sp++)
		sum += *sp;
	return sum;
}

int
ip_checksum(char *p, int len, u_short *rsum)
{
	u_long sum;

	if ((unsigned long)p & 01)
		return 0;
	sum = ip_partialsum(p, len);
	*rsum = IP_FOLD_CHECKSUM_CARRY(sum);
	return 1;
}

int
ip_checksum_frame(struct ipframe *ipf, char *p, int len, u_short *rsum)
{
	if (len != ipf->ipf_len - ipf->ipf_hdrlen)
		return 0;
	return ip_checksum(p, len, rsum);
}

struct pseudohdr {
	struct in_addr	src;
	struct in_addr	dst;
	u_char		zero;
	u_char		p;
	u_short		len;
};

int
ip_checksum_pseudohdr(struct ipframe *ipf, char *p, int len, u_short *rsum)
{
	struct pseudohdr ph;
	u_long sum;

	if (len != ipf->ipf_len - ipf->ipf_hdrlen)
		return 0;
	ph.src.s_addr = htonl(ipf->ipf_src.s_addr);
	ph.dst.s_addr = htonl(ipf->ipf_rdst.s_addr);
	ph.zero = 0;
	ph.p = ipf->ipf_prototype;
	ph.len = len;
	sum = ip_partialsum((char *) &ph, sizeof ph);
	sum += ip_partialsum(p, len);
	*rsum = IP_FOLD_CHECKSUM_CARRY(sum);
	return 1;
}
