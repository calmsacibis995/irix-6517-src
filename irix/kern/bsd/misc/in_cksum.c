/* Internet Protocol Checksum
 *
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 */

#ident "$Revision: 3.9 $"

#include "sys/types.h"
#include "sys/mbuf.h"
#include "sys/cmn_err.h"
#include "net/if.h"

/*
 * External Procedures
 */
extern void panic(char *, ...);

/*
 * Compute Internet checksum for a string of mbufs
 */
unsigned int
in_cksum(struct mbuf *m, int len)
{
#define XCHG(xs) ((((xs)>>8) & 0xff) | (((xs) & 0xff) << 8))

	ushort ck;
	int mlen, rlen;
	unsigned short *addr;

	ck = rlen = 0;
	while (m != 0) {
		mlen = m->m_len;
		if (0 != mlen) {	/* skip 0-length mbufs */
			if (mlen > len)
				mlen = len;
			addr = mtod(m, unsigned short *);
			if ((rlen^(__psint_t)addr) & 1) {
				ck = in_cksum_sub(addr, mlen, XCHG(ck));
				ck = XCHG(ck);
			} else {
				ck = in_cksum_sub(addr, mlen, ck);
			}

			rlen += mlen;
			len -= mlen;
			if (len == 0)
				return (0xffff ^ ck);
		}
		m = m->m_next;
	}

	panic("in_cksum, ran out of data, %d bytes left\n", len);
	/* NOTREACHED */
}
