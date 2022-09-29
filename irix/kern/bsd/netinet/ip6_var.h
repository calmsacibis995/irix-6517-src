/*
 * Copyright (c) 1982, 1986, 1993
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
 *	@(#)ip_var.h	8.1 (Berkeley) 6/10/93
 */

#ifndef _NETINET_IP6_VAR_H_
#define _NETINET_IP6_VAR_H_

/*
 * Overlay for IPv6 header used for checksum by other protocols.
 */
struct ip6ovck {
	u_int32_t ih6_wrd0;			/* first word */
	union {
		u_int32_t ihuw6_wrd1;		/* second word */
		struct {
			u_int8_t  ihu6_x;	/* reserved (0) */
			u_int8_t  ihu6_pr;	/* protocol */
			u_int16_t ihu6_len;	/* payload length */
		} ihus6_wrd1;
	} ihu6_wrd1;
};
#define ih6_wrd1	ihu6_wrd1.ihuw6_wrd1
#define ih6_x		ihu6_wrd1.ihus6_wrd1.ihu6_x
#define ih6_pr		ihu6_wrd1.ihus6_wrd1.ihu6_pr
#define ih6_len		ihu6_wrd1.ihus6_wrd1.ihu6_len

/*
 * Structure for ctlinput routines.
 */
struct ctli_arg {
	struct ipv6 *ctli_ip;
	struct mbuf *ctli_m;
};

/*
 * IPv6 reassembly queue structure.  Each fragment
 * being reassembled is attached to one of these structures.
 * They are timed out after ipq_ttl drops to 0, and may also
 * be reclaimed if memory becomes tight.
 */
struct ip6q {
	struct	  ip6q *next, *prev;	/* to other reass headers */
	u_int8_t  ip6q_ttl;		/* time for reass q to live */
	u_int8_t  ip6q_nh;		/* next header type of this fragment */
	u_int16_t ip6q_spare;		/* for alignment */
	u_int32_t ip6q_id;		/* sequence id for reassembly */
	struct	  ip6asfrag *ip6q_next, *ip6q_prev;
					/* to IPv6 headers of fragments */
	struct	  in6_addr ip6q_src, ip6q_dst;
	struct	  mbuf *ip6q_mbuf;	/* back pointer to mbuf header */
};

/*
 * IPv6 header, when holding a fragment.
 *
 * Note: ip6f_next must be at same offset as ip6q_next above
 */
struct	ip6asfrag {
	u_int32_t ip6f_head;		/* next */
	u_int16_t ip6f_len;		/* prev */
	u_int8_t  ip6f_nh;
	u_int8_t  ip6f_hlim;
#if (_MIPS_SZPTR == 64)
	u_int32_t ip6f_pad1;		/* pad to make sure ip6f_next is at */
	u_int32_t ip6f_pad2;		/* the same offset as ip6q_next above */
#endif
	u_int32_t ip6f_w2;		/* ttl/nh */
	u_int32_t ip6f_w3;		/* id */
	struct	  ip6asfrag *ip6f_next;	/* ip6q_next */
	struct	  ip6asfrag *ip6f_prev;	/* ip6q_prev */
#if (_MIPS_SZPTR == 32)
	struct	  in6_addr ip6f_dst;
#endif
	u_int8_t  ip6f_fnh;		/* fragment header */
	u_int8_t  ip6f_mff;
	u_int16_t ip6f_off;
	u_int32_t ip6f_id;
};

#if defined(_KERNEL) && defined(INET6)
#define IP6_DONTFRAG	0x4		/* ip6_outpt must not fragment */

#define IP6_INSOPT_NOALLOC	1
#define IP6_INSOPT_RAW          2

extern u_int32_t  ip6_id;		/* IPv6 packet ctr, for ids */
extern struct socket *ip6_mrouter;	/* multicasr routing daemon */

extern int	  ip6printfs;
#define D6_INPUT	0x00000001
#define D6_CTLIN	0x00000002
#define D6_MCASTIN	0x00000004
#define D6_REASS	0x00000008
#define D6_OPTIN	0x00000010
#define D6_ETHERIN	0x00000020
#define D6_SITIN	0x00000040
#define D6_FORWARD	0x00000080
#define D6_OUTPUT	0x00000100
#define D6_CTLOUT	0x00000200
#define D6_MCASTOUT	0x00000400
#define D6_FRAG		0x00000800
#define D6_OPTOUT	0x00001000
#define D6_ETHEROUT	0x00002000
#define D6_SITOUT	0x00004000
#define D6_SITCTL	0x00008000
#define D6_INIT		0x00010000
#define D6_RAW		0x00020000
#define D6_NDP0		0x00040000
#define D6_NDP1		0x00080000
#define D6_UDP		0x00100000
#define D6_PMTU		0x00200000
#define D6_TCP0		0x00400000
#define D6_TCP1		0x00800000
#define D6_AH		0x01000000
#define D6_ESP		0x02000000
#define D6_KEY		0x04000000
#define D6_TUGCTL	0x10000000
#define D6_TUGIN	0x20000000
#define D6_TUGOUT	0x40000000

void	 ah6_input __P((struct mbuf *, struct ifnet *,
  struct ipsec *, struct mbuf *));
struct mbuf *
	 ah6_output __P((struct mbuf *, struct mbuf *, struct ip_soptions *));
int	 dopt6_dontfrag __P((struct mbuf *));
void	 dopt6_input __P((struct mbuf *, struct ifnet *,
  struct ipsec *, struct mbuf *));
void	 end6_input __P((struct mbuf *, struct ifnet *,
  struct ipsec *, struct mbuf *));
void	 esp6_input __P((struct mbuf *, struct ifnet *,
  struct ipsec *, struct mbuf *));
struct mbuf *
	 esp6_output __P((struct mbuf *, struct mbuf *,
			  struct ip_soptions *, int, int));
void	 frg6_deq __P((struct ip6asfrag *));
void	 frg6_drain __P((void));
void	 frg6_enq __P((struct ip6asfrag *, struct ip6asfrag *));
void	 frg6_input __P((struct mbuf *, struct ifnet *,
  struct ipsec *, struct mbuf *));
void	 frg6_slowtimo __P((void));
int	 hd6_inoptions __P((struct mbuf *, struct ifnet *,struct mbuf *,int *));
void	 hd6_outoptions __P((struct mbuf *));
void	 hop6_input __P((struct mbuf *, struct ifnet *,
  struct ipsec *, struct mbuf *));
int	 ip6_ctloutput __P((int, struct socket *, int, int, struct mbuf **));
struct mbuf *
	 ip6_dropoption __P((struct mbuf *, int));
void	 ip6_freemoptions __P((struct mbuf *));
int	 ip6_getmoptions __P((int, struct mbuf *, struct mbuf **));
int	 ip6_getoptions __P((struct mbuf *, struct mbuf **, int));
void	 ip6_init __P((void));
void	 ip6_input __P((struct mbuf *, struct ifnet *,
  struct ipsec *, struct mbuf *));
struct mbuf *ip6_insertoption
	__P((struct mbuf *, struct mbuf *, struct ip_soptions *, int, int *));
int	 ip6_mforward __P((struct ipv6 *, struct ifnet *,
			   struct mbuf *, struct ip_moptions *));
int	 ip6_output __P((struct mbuf *, struct mbuf *, struct route *,
			 int, struct ip_moptions *, struct inpcb *));
int	 ip6_setcontrol __P((struct inpcb *, struct mbuf *));
int	 ip6_setmoptions __P((struct mbuf **, int, struct mbuf *));
int	 ip6_setsoptions __P((struct socket *, int, struct mbuf *));
int	 ip6_sysctl __P((int *, u_int, void *, size_t *, void *, size_t));
void	 ip6ip6_input __P((struct mbuf *, struct ifnet *,
  struct ipsec *, struct mbuf *));
void	 ipsec_duplist __P((struct  ip_soptions *, struct  ip_soptions *));
void	 opt6_ctlinput __P((int, struct sockaddr *, void *));
void	 rip6_init __P((void));
int	 rip6_ctloutput __P((int, struct socket *, int, int, struct mbuf **));
void     rip6_ctlinput __P((int, struct sockaddr *, void *));
void	 rip6_input __P((struct mbuf *, struct ifnet *,
  struct ipsec *ipsec, struct mbuf *));
int	 rip6_usrreq __P((struct socket *,
	    int, struct mbuf *, struct mbuf *, struct mbuf *));
void	 rt6_input __P((struct mbuf *, struct ifnet *,
  struct ipsec *, struct mbuf *));
void	 opt6_reverse __P((struct ipv6 *, struct mbuf *));

extern void log(int level, char *fmt, ...);

#endif

#endif
