/*-
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
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
 *	@(#)slcompress.c	7.7 (Berkeley) 5/7/91
 */

/*
 * Routines to compress and uncompess tcp packets (for transmission
 * over low speed serial lines.
 *
 * Van Jacobson (van@helios.ee.lbl.gov), Dec 31, 1989:
 *	- Initial distribution.
 */
#ident "$Revision: 1.16 $"

#ifndef DEF_MAX_STATES		/* nasty kludge! to avoid patch release */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/mbuf.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include "if.h"
#include <sys/systm.h>

#include "slcompress.h"
#endif /* kludge */

#define INCR(counter) ++comp->counter;



struct vj_comp*
vj_comp_init(struct vj_comp *comp,
	     int tx_states)
{
	int i;

	/* the linking of states makes assumptions */
	if (tx_states < MIN_MAX_STATES)
		tx_states = MIN_MAX_STATES;

	if (tx_states > MAX_MAX_STATES)
		tx_states = MAX_MAX_STATES;

	if (comp != 0
	    && comp->tx_states != tx_states) {
		kern_free(comp);
		comp = 0;
	}

	i = sizeof(struct vj_comp) + (tx_states-1)*sizeof(struct tx_cstate);
	if (comp == 0) {
		comp = (struct vj_comp*)kern_malloc(i);
		if (comp == 0)
			return 0;
	}

	bzero(comp, i);

	comp->tx_states = tx_states;

	for (i = tx_states - 1; i > 0; --i) {
		comp->tstate[i].cs_id = i;
		comp->tstate[i].cs_next = &comp->tstate[i-1];
	}
	comp->tstate[0].cs_next = &comp->tstate[tx_states-1];
	comp->tstate[0].cs_id = 0;
	comp->last_cs = &comp->tstate[0];
	comp->last = MAX_MAX_STATES+1;

	return comp;
}


struct vj_uncomp*
vj_uncomp_init(struct vj_uncomp *comp,
	       int rx_states)
{
	int i;

	/* the linking of states makes assumptions */
	if (rx_states < MIN_MAX_STATES)
		rx_states = MIN_MAX_STATES;

	if (rx_states > MAX_MAX_STATES)
		rx_states = MAX_MAX_STATES;

	if (comp != 0 && comp->rx_states != rx_states) {
		kern_free(comp);
		comp = 0;
	}

	i = sizeof(struct vj_uncomp) + rx_states*sizeof(struct rx_cstate);
	if (comp == 0) {
		comp = (struct vj_uncomp*)kern_malloc(i);
		if (comp == 0)
			return 0;
	}

	bzero(comp, i);

	comp->rx_states = rx_states;
	comp->flags = SLF_TOSS;

	return comp;
}


/* ENCODE encodes a number that is known to be non-zero.  ENCODEZ
 * checks for zero (since zero has to be encoded in the long, 3 byte
 * form).
 */
#define ENCODE(n) { \
	if ((u_short)(n) >= 256) { \
		*cp++ = 0; \
		cp[1] = (n); \
		cp[0] = (n) >> 8; \
		cp += 2; \
	} else { \
		*cp++ = (n); \
	} \
}
#define ENCODEZ(n) { \
	if ((u_short)(n) >= 256 || (u_short)(n) == 0) { \
		*cp++ = 0; \
		cp[1] = (n); \
		cp[0] = (n) >> 8; \
		cp += 2; \
	} else { \
		*cp++ = (n); \
	} \
}

#define DECODEL(f) { \
	if (*cp == 0) {\
		(f) = htonl(ntohl(f) + ((cp[1] << 8) | cp[2])); \
		cp += 3; \
	} else { \
		(f) = htonl(ntohl(f) + (u_long)*cp++); \
	} \
}

#define DECODES(f) { \
	if (*cp == 0) {\
		(f) = htons(ntohs(f) + ((cp[1] << 8) | cp[2])); \
		cp += 3; \
	} else { \
		(f) = htons(ntohs(f) + (u_long)*cp++); \
	} \
}

#define DECODEU(f) { \
	if (*cp == 0) {\
		(f) = htons((cp[1] << 8) | cp[2]); \
		cp += 3; \
	} else { \
		(f) = htons((u_long)*cp++); \
	} \
}


u_char
vj_compress(struct mbuf **mp,
	    struct vj_comp *comp,
	    int *wlenp,			/* ptr to write length for driver */
	    u_int linknum,		/* BF&I multilink number */
	    int do_compress,		/* 0=never compress */
	    int compress_cid)		/* 0=do not compress slot ID */
{
	register struct tx_cstate *cs = comp->last_cs->cs_next;
	struct ip *ip;
	struct mbuf *m = *mp;
	register u_int hlen;
	register struct tcphdr *oth;
	register struct tcphdr *th;
	register u_int deltaS, deltaA;
	register u_int changes = 0;
	u_char new_seq[16];
	register u_char *cp = new_seq;

	/*
	 * Bail if it is not a TCP/IP packet
	 */
	deltaA = m_length(m);
	if (deltaA < sizeof(struct ip)+sizeof(struct tcphdr))
		return (TYPE_IP);

	/*
	 * Deal with leading stunted mbufs
	 */
	if (m->m_len < sizeof(struct ip)) {
		m = *mp = m_pullup(m, MIN(deltaA, MAX_HDR));
		if (!m)
			return (TYPE_IP);
	}

	/*
	 * Bail if it is not a TCP/IP packet or it is an IP fragment.
	 */
	ip = mtod(m, struct ip*);
	if (ip->ip_p != IPPROTO_TCP
	    || 0 != (ip->ip_off & htons(0x3fff)))
		return (TYPE_IP);
	deltaS = ip->ip_hl;
	hlen = deltaS << 2;

	/*
	 * You cannot change the contents of an mbuf-cluster unless you
	 * are its sole owner.  It might be shared with user space,
	 * the buffer cache, or some other place.
	 */
	if (m->m_len < hlen+sizeof(struct tcphdr)
	    || (M_HASCL(m) && do_compress)) {
		m = *mp = m_pullup(m, MIN(deltaA, MAX_HDR));
		if (!m)
			return (TYPE_IP);
		ip = mtod(m, struct ip*);
	}

	th = (struct tcphdr *)&((int *)ip)[deltaS];
	if ((th->th_flags & (TH_SYN|TH_ACK)) != TH_ACK)
		return (TYPE_IP);

	hlen += (th->th_off << 2);
	if (hlen > MAX_HDR)
		return (TYPE_IP);

	if (m->m_len < hlen) {
		m = *mp = m_pullup(m, MIN(deltaA, MAX_HDR));
		if (!m)
			return (TYPE_IP);
		ip = mtod(m, struct ip*);
		th = (struct tcphdr *)&((int *)ip)[deltaS];
	}
	/*
	 * Packet is compressible
	 * Locate (or create) the connection state.  Special case the
	 * most recently used connection since it is most likely to be used
	 * again & we do no have to do any reordering if it is used.
	 */
	INCR(cnt_packets)
	if (ip->ip_src.s_addr != cs->cs_ip.ip_src.s_addr ||
	    ip->ip_dst.s_addr != cs->cs_ip.ip_dst.s_addr ||
	    *(int *)th != ((int *)&cs->cs_ip)[cs->cs_ip.ip_hl]) {
		/*
		 * Was not the first -- search for it.
		 *
		 * States are kept in a circularly linked list with
		 * last_cs pointing to the end of the list.  The
		 * list is kept in lru order by moving a state to the
		 * head of the list whenever it is referenced.  Since
		 * the list is short and, empirically, the connection
		 * we want is almost always near the front, we locate
		 * states via linear search.
		 */
		register struct tx_cstate *lcs;
		register struct tx_cstate *lastcs = comp->last_cs;
		register struct tx_cstate *qcs, *lqcs;

		qcs = 0;
		do {
			lcs = cs; cs = cs->cs_next;
			INCR(cnt_searches)
			if (ip->ip_src.s_addr == cs->cs_ip.ip_src.s_addr
			    && ip->ip_dst.s_addr == cs->cs_ip.ip_dst.s_addr
			    && *(int *)th == ((int *)&cs->cs_ip)[cs->cs_ip.ip_hl])
				goto found;
			if (cs->cs_active <= 0) {
				qcs = cs;
				lqcs = lcs;
			}
		} while (cs != lastcs);


		/*
		 * Did not find it.
		 */
		INCR(cnt_misses)

		/*
		 * If it was a FIN or RST, then just forget the whole thing.
		 */
		if (0 != (th->th_flags & (TH_FIN|TH_RST)))
			return (TYPE_IP);

		/*
		 * Re-use oldest tx_cstate or the oldest inactive tx_cstate.
		 */
		if (qcs) {
			/* There is an inactive tx_cstate, so use it
			 * after moving it to the head of list
			 */
			cs = qcs;
			if (cs == lastcs)
				comp->last_cs = cs;
			else {
				lqcs->cs_next = cs->cs_next;
				cs->cs_next = lastcs->cs_next;
				lastcs->cs_next = cs;
			}
		} else {
			/*
			 * Note that since the state list is circular, the
			 * oldest state points to the newest and we only
			 * need to set last_cs to update the lru linkage.
			 */
			comp->last_cs = lcs;
		}

		/* Send an uncompressed packet that tells the other side what
		 * connection number we are using for this conversation.
		 */
		if (cs->cs_active <= 0) {
			comp->actconn++;
			cs->cs_active = 1;
		}
		goto uncompressed;

	found:
		/*
		 * Found it -- move to the front on the connection list.
		 */
		if (cs == lastcs)
			comp->last_cs = lcs;
		else {
			lcs->cs_next = cs->cs_next;
			cs->cs_next = lastcs->cs_next;
			lastcs->cs_next = cs;
		}
	}
	/*
	 * If it was a FIN, then just forget the whole thing after
	 * poisoning the compressor, so that the caller can keep
	 * track of the number active TCP sessions.
	 */
	if (0 != (th->th_flags & (TH_FIN|TH_RST))) {
		if (cs->cs_active > 0)
			comp->actconn--;
		cs->cs_active = -1;
		return (TYPE_IP);
	}

	/* If we have seen a FIN or RST on this ((addr,port),(addr,port)),
	 * then we might now be seeing either the ACK for the other FIN
	 * from the other side or a brand new TCP virtual circuit.
	 * Do not believe it is a new TCP connection unless the sequence
	 * numbers have changed by more than what happens after a FIN.
	 */
	oth = (struct tcphdr *)&((int *)&cs->cs_ip)[deltaS];
	if (cs->cs_active <= 0) {
		if ((ntohl(th->th_ack) - ntohl(oth->th_ack) > 1
		     || ntohl(th->th_seq) - ntohl(oth->th_seq) > 1)
		    && ++cs->cs_active > 0)
			comp->actconn++;
	}

	/*
	 * If we are doing this only to count active TCP connections,
	 * then bail.
	 */
	if (!do_compress)
		goto uncompressed;

	/*
	 * If the multilink number changes, let the receive reset its
	 * state.
	 */
	if (cs->cs_linknum != linknum)
		goto uncompressed;

	/*
	 * Make sure that only what we expect to change changed. The first
	 * line of the "if" checks the IP protocol version, header length &
	 * type of service.  The 2nd line checks the "Don't fragment" bit.
	 * The 3rd line checks the time-to-live and protocol (the protocol
	 * check is unnecessary but costless).  The 4th line checks the TCP
	 * header length.  The 5th line checks IP options, if any.  The 6th
	 * line checks TCP options, if any.  If any of these things are
	 * different between the previous & current datagram, we send the
	 * current datagram "uncompressed".
	 */

	if (((u_short *)ip)[0] != ((u_short *)&cs->cs_ip)[0] ||
	    ((u_short *)ip)[3] != ((u_short *)&cs->cs_ip)[3] ||
	    ((u_short *)ip)[4] != ((u_short *)&cs->cs_ip)[4] ||
	    th->th_off != oth->th_off ||
	    (deltaS > 5 && bcmp(ip + 1, &cs->cs_ip + 1, (deltaS - 5) << 2)) ||
	    (th->th_off > 5 && bcmp(th + 1, oth + 1, (th->th_off - 5) << 2)))
		goto uncompressed;

	/*
	 * Figure out which of the changing fields changed.  The
	 * receiver expects changes in the order: urgent, window,
	 * ack, seq (the order minimizes the number of temporaries
	 * needed in this section of code).
	 */
	if (th->th_flags & TH_URG) {
		deltaS = ntohs(th->th_urp);
		ENCODEZ(deltaS);
		changes |= NEW_U;
	} else if (th->th_urp != oth->th_urp)
		/* argh! URG not set but urp changed -- a sensible
		 * implementation should never do this but RFC793
		 * does not prohibit the change so we have to deal
		 * with it. */
		 goto uncompressed;

	if (deltaS = (u_short)(ntohs(th->th_win) - ntohs(oth->th_win))) {
		ENCODE(deltaS);
		changes |= NEW_W;
	}

	if (deltaA = ntohl(th->th_ack) - ntohl(oth->th_ack)) {
		if (deltaA > 0xffff)
			goto uncompressed;
		ENCODE(deltaA);
		changes |= NEW_A;
	}

	if (deltaS = ntohl(th->th_seq) - ntohl(oth->th_seq)) {
		if (deltaS > 0xffff)
			goto uncompressed;
		ENCODE(deltaS);
		changes |= NEW_S;
	}

	switch(changes) {

	case 0:
		/*
		 * Nothing changed. If this packet contains data and the
		 * last one did not, this is probably a data packet following
		 * an ack (normal on an interactive connection) and we send
		 * it compressed.  Otherwise it is probably a retransmit,
		 * retransmitted ack or window probe.  Send it uncompressed
		 * in case the other side missed the compressed version.
		 */
		if (ip->ip_len != cs->cs_ip.ip_len &&
		    ntohs(cs->cs_ip.ip_len) == hlen)
			break;

		/* (fall through) */

	case SPECIAL_I:
	case SPECIAL_D:
		/*
		 * actual changes match one of our special case encodings --
		 * send packet uncompressed.
		 */
		goto uncompressed;

	case NEW_S|NEW_A:
		if (deltaS == deltaA &&
		    deltaS == ntohs(cs->cs_ip.ip_len) - hlen) {
			/* special case for echoed terminal traffic */
			changes = SPECIAL_I;
			cp = new_seq;
		}
		break;

	case NEW_S:
		if (deltaS == ntohs(cs->cs_ip.ip_len) - hlen) {
			/* special case for data xfer */
			changes = SPECIAL_D;
			cp = new_seq;
		}
		break;
	}

	deltaS = ntohs(ip->ip_id) - ntohs(cs->cs_ip.ip_id);
	if (deltaS != 1) {
		ENCODEZ(deltaS);
		changes |= NEW_I;
	}
	if (th->th_flags & TH_PUSH)
		changes |= TCP_PUSH_BIT;
	/*
	 * Grab the cksum before we overwrite it below.  Then update our
	 * state with the header from this packet.
	 */
	deltaA = ntohs(th->th_sum);
	bcopy(ip, &cs->cs_ip, hlen);

	/*
	 * We want to use the original packet as our compressed packet.
	 * (cp - new_seq) is the number of bytes we need for compressed
	 * sequence numbers.  In addition we need one byte for the change
	 * mask, one for the connection id and two for the tcp checksum.
	 * So, (cp - new_seq) + 4 bytes of header are needed.  hlen is how
	 * many bytes of the original packet to toss so subtract the two to
	 * get the new packet size.
	 */
	deltaS = cp - new_seq;
	*wlenp -= hlen-deltaS;
	cp = (u_char *)ip;
	if (compress_cid == 0 || comp->last != cs->cs_id) {
		comp->last = cs->cs_id;
		hlen -= deltaS + 4;
		cp += hlen;
		*cp++ = changes | NEW_C;
		*cp++ = cs->cs_id;
		*wlenp += 4;
	} else {
		hlen -= deltaS + 3;
		cp += hlen;
		*cp++ = changes;
		*wlenp += 3;
	}
	m->m_len -= hlen;
	m->m_off += hlen;
	*cp++ = deltaA >> 8;
	*cp++ = deltaA;
	bcopy(new_seq, cp, deltaS);
	INCR(cnt_compressed)
	return (TYPE_COMPRESSED_TCP);

	/*
	 * Update connection state cs
	 * If allowed, send uncompressed packet ('uncompressed' means a
	 * regular ip/tcp packet but with the 'slot id' we hope to use on
	 * future compressed packets in the protocol field).
	 */
uncompressed:
	bcopy(ip, &cs->cs_ip, hlen);
	cs->cs_linknum = linknum;

	comp->last = cs->cs_id;

	if (!do_compress)
		return (TYPE_IP);

	ip->ip_p = cs->cs_id;
	return (TYPE_UNCOMPRESSED_TCP);
}



int
vj_uncompress(struct mbuf *m,		/* mbuf chain with snoop/if header */
	      int len,			/* total data bytes in mbuf chain */
	      u_char **bufp,		/* start of data */
	      u_int type,		/* type of input packet */
	      struct vj_uncomp *comp)
{
	register u_char *cp;
	register u_int hlen, changes;
	register struct tcphdr *th;
	register struct rx_cstate *cs;
	register struct ip *ip;

	if (type == TYPE_UNCOMPRESSED_TCP) {
		/* flush the database after tossing a packet and when the
		 * database is finally being fixed. */
		if (comp->flags & SLF_TOSS) {
			cs = &comp->rstate[comp->rx_states];
			do {
				(--cs)->cs_hlen = 0;
			} while (cs != comp->rstate);
			comp->flags &= ~SLF_TOSS;
		}

		ip = (struct ip *) *bufp;

		if (ip->ip_p >= comp->rx_states)
			goto bad;
		cs = &comp->rstate[comp->last = ip->ip_p];
		ip->ip_p = IPPROTO_TCP;
		hlen = ip->ip_hl;
		hlen += ((struct tcphdr *)&((int *)ip)[hlen])->th_off;
		hlen <<= 2;
		bcopy(ip, &cs->cs_ip, hlen);
		cs->cs_ip.ip_sum = 0;
		cs->cs_hlen = hlen;
		INCR(cnt_uncompressed)
		return (len);
	}

	if (type !=  TYPE_COMPRESSED_TCP)
		goto bad;

	/* We've got a compressed packet. */
	INCR(cnt_compressed)

	/* bail out if tossing packets after a loss */
	if (comp->flags & SLF_TOSS) {
		INCR(cnt_tossed)
		return (0);
	}

	cp = *bufp;
	changes = *cp++;
	if (changes & NEW_C) {
		/* Make sure the explicit state index in this packet
		 * is in range. */
		if (*cp >= comp->rx_states)
			goto bad;
		comp->last = *cp++;
	}
	cs = &comp->rstate[comp->last];
	if (cs->cs_hlen == 0) {
		/* If this database entry has not be fixed by an uncompressed
		 * packet, toss this new packet */
		INCR(cnt_tossed)
		return (0);
	}
	hlen = cs->cs_ip.ip_hl << 2;
	th = (struct tcphdr *)&((u_char *)&cs->cs_ip)[hlen];
	th->th_sum = htons((*cp << 8) | cp[1]);
	cp += 2;
	if (changes & TCP_PUSH_BIT)
		th->th_flags |= TH_PUSH;
	else
		th->th_flags &=~ TH_PUSH;

	switch (changes & SPECIALS_MASK) {
	case SPECIAL_I:
		{
		register u_int i = ntohs(cs->cs_ip.ip_len) - cs->cs_hlen;
		th->th_ack = htonl(ntohl(th->th_ack) + i);
		th->th_seq = htonl(ntohl(th->th_seq) + i);
		}
		break;

	case SPECIAL_D:
		th->th_seq = htonl(ntohl(th->th_seq) + ntohs(cs->cs_ip.ip_len)
				   - cs->cs_hlen);
		break;

	default:
		if (changes & NEW_U) {
			th->th_flags |= TH_URG;
			DECODEU(th->th_urp)
		} else
			th->th_flags &=~ TH_URG;
		if (changes & NEW_W)
			DECODES(th->th_win)
		if (changes & NEW_A)
			DECODEL(th->th_ack)
		if (changes & NEW_S)
			DECODEL(th->th_seq)
		break;
	}
	if (changes & NEW_I) {
		DECODES(cs->cs_ip.ip_id)
	} else
		cs->cs_ip.ip_id = htons(ntohs(cs->cs_ip.ip_id) + 1);

	/*
	 * At this point, cp points to the first byte of data in the
	 * packet.  If we're not aligned on a 4-byte boundary, copy the
	 * data down so the ip & tcp headers will be aligned.  Then back up
	 * cp by the tcp/ip header length to make room for the reconstructed
	 * header (we assume the packet we were handed has enough space to
	 * prepend 128 bytes of header).  Adjust the length to account for
	 * the new header & fill in the IP total length.
	 */
	len -= (cp - *bufp);
	if (len < 0)
		/* we must have dropped some characters (crc should detect
		 * this but the old slip framing won't) */
		goto bad;

	/*
	 * The data will usually be misaligned, so always copy it from
	 * the end of the 1st mbuf toward its start, leaving room for the
	 * uncompressed header.  Copy only the data that is in the 1st
	 * mbuf.
	 */
	if (len > 0)
		bcopy(cp, *bufp,
		      len > MLEN ? MLEN : len);
	m->m_len -= (cp - *bufp);	/* trim end of mbuf */
	cp = *bufp - cs->cs_hlen;
	mtod(m,struct ifheader*)->ifh_hdrlen -= cs->cs_hlen;
	len += cs->cs_hlen;
	cs->cs_ip.ip_len = htons(len);
	bcopy(&cs->cs_ip, cp, cs->cs_hlen);
	*bufp = cp;

	/* recompute the ip header checksum */
	{
		register u_short *bp = (u_short *)cp;
		for (changes = 0; hlen > 0; hlen -= 2)
			changes += *bp++;
		changes = (changes & 0xffff) + (changes >> 16);
		changes = (changes & 0xffff) + (changes >> 16);
		((struct ip *)cp)->ip_sum = ~ changes;
	}
	return (len);
bad:
	comp->flags |= SLF_TOSS;
	INCR(cnt_error)
	return (0);
}
