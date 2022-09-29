/*
 * Copyright (c) 1982, 1986, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)tcp_var.h	8.3 (Berkeley) 4/10/94
 */

#ifndef _NETINET_TCP6_VAR_H_
#define _NETINET_TCP6_VAR_H_

/* get a 64-bit value of type t from two 32-bit fields */
#define DWORD_GET(f1,f2,t) ((t)((__uint64_t)(*(__uint32_t*)&(f2)) | \
        (__uint64_t)(*(__uint32_t*)&(f1)) << 32))
/* put a 64-bit field into two 32-bit fields */
#define DWORD_PUT(f1,f2,p) {\
	*(__uint32_t*)&(f2) = (__uint32_t)((__uint64_t)(p) & 0xffffffff);\
	*(__uint32_t*)&(f1) = (__uint32_t)((__uint64_t)(p) >> 32 & 0xffffffff);}

/*
 * We want to avoid doing m_pullup on incoming packets but that
 * means avoiding dtom on the tcp reassembly code.  That in turn means
 * keeping an mbuf pointer in the reassembly queue (since we might
 * have a cluster).  As a quick hack, ti_sport, ti_dport, and ti_ack
 * (which are no longer needed once we've located the tcpcb) are
 * overlaid with an mbuf pointer.
 */
#if (_MIPS_SZLONG != 32)
#define REASS_MBUF6_PUT(tr,m) DWORD_PUT((tr)->tr_sport, (tr)->tr_ack, m)
#define REASS_MBUF6_GET(tr) DWORD_GET((tr)->tr_sport, (tr)->tr_ack, struct mbuf*)
#else
#define REASS_MBUF6_PUT(tr,m) (tr)->tr_ack = (__uint32_t)(m)
#define REASS_MBUF6_GET(tr) ((struct mbuf *)((tr)->tr_ack))
#endif

#if defined(_KERNEL) && defined(INET6)

int	 tcp6_connect __P((struct tcpcb *, struct mbuf *));
void	 tcp6_ctlinput __P((int, struct sockaddr *, void *));
int	 tcp6_ctloutput __P((int, struct socket *, int, int, struct mbuf **));
void	 tcp6_input __P((struct mbuf *, struct ifnet *ifp,
  struct ipsec *ipsec, struct mbuf *));
int	 tcp6_mssopt __P((struct tcpcb *));
int	 tcp6_output __P((struct tcpcb *));
int	 tcp6_reass __P((struct tcpcb *, struct tcp6hdrs *, struct mbuf *));
struct rtentry *tcp6_rtlookup __P((struct inpcb *));
int	 tcp6_usrreq __P((struct socket *,
	    int, struct mbuf *, struct mbuf *, struct mbuf *));
#endif

#endif /* _NETINET_TCP6_VAR_H_ */
