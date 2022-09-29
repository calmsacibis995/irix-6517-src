/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution is only permitted until one year after the first shipment
 * of 4.4BSD by the Regents.  Otherwise, redistribution and use in source and
 * binary forms are permitted provided that: (1) source distributions retain
 * this entire copyright notice and comment, and (2) distributions including
 * binaries display the following acknowledgement:  This product includes
 * software developed by the University of California, Berkeley and its
 * contributors'' in the documentation or other materials provided with the
 * distribution and in all advertising materials mentioning features or use
 * of this software.  Neither the name of the University nor the names of
 * its contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *	@(#)tcp_var.h	7.10 (Berkeley) 6/28/90
 */
#ifdef __cplusplus
extern "C" {
#endif

#ifdef __sgi
#define BSD 43
#define TCP_COMPAT_42	/* we have to interop w/4.2 systems */
#endif

#include <netinet/ip_var.h>
#include <netinet/tcpip.h>
#include <sys/rtmon.h>

#ifdef _TCP_UTRACE
#define TCP_UTRACE_TP(name, mb, ra) \
	UTRACE(RTMON_ALLOC, (name), (__int64_t)(mb), \
		   UTPACK((ra), (mb) ? (mb)->t_state : 0));
#define TCP_UTRACE_SO(name, mb, ra) \
	UTRACE(RTMON_ALLOC, (name), (__int64_t)(mb), \
		   UTPACK((ra), (mb) ? (mb)->so_state : 0));
#define TCP_UTRACE_PCB(name, mb, ra) \
	UTRACE(RTMON_ALLOC, (name), (__int64_t)(mb), \
		   UTPACK((ra), (mb) ? (mb)->inp_refcnt : 0));
#else
#define TCP_UTRACE_TP(name, mb, ra)
#define TCP_UTRACE_SO(name, mb, ra)
#define TCP_UTRACE_PCB(name, mb, ra)
#endif

/*
 * Kernel variables for tcp.
 */

/* Structures and parameters for implementing SACK */
#define		SACK_MAX	4	

struct sack_rcvblock {
	tcp_seq start;
	tcp_seq end;
};

struct sack_sndhole {
	tcp_seq start;			/* Start of unacked block */
	tcp_seq end;			/* End of unacked block */
	struct timespec stamp;		/* When this block was sent */
	struct sack_sndhole* next;	/* next seq node */
};


/*
 * Tcp control block, one per tcp; fields:
 */
struct tcpcb {
	/*
	 * Tcpcb points to TCP reassembly queue.  It is casted to tcpiphdr
	 * during queueing operations.  The beginning of tcpiphdr is ipovly,
	 * an old BSD heritage that avoids dynamic memory allocation by 
	 * overlaying memory. Having an ipovly here ensures that pointers 
	 * are at right offsets, making it easier to deal with IP overlay.
	 */
	struct  ipovly linkage;		/* reassembly queue */
#ifdef INET6
	/*
	 * For now we have a separate set of pointer for the tcp reassembly
	 * queue for ipv4 and ipv6.  The pointers are placed in different
	 * locations in the ip header due to differences in the ipv4 and ipv6
	 * headers.  Perhaps we can merge them some day.
	 */
	union {
		char *align;		/* to align t_link6 in 64bit kernels */
		struct  tcp6hdrs t_lnk6; /* ipv6 tcp reassembly queue */
	} u;
#endif
	short	t_state;		/* state of this connection */
	short	t_timer[TCPT_NTIMERS];	/* tcp timers */
	short	t_rxtshift;		/* log(2) of rexmt exp. backoff */
	short	t_rxtcur;		/* current retransmit value */
	short	t_dupacks;		/* consecutive dup acks recd */
#ifdef INET6
	u_int	t_maxseg;		/* maximum segment size */
#else
	u_short	t_maxseg;		/* maximum segment size */
#endif
	char	t_force;		/* 1 if forcing out a byte */
	u_short	t_flags;
#define	TF_ACKNOW	0x0001		/* ack peer immediately */
#define	TF_DELACK	0x0002		/* ack, but try to delay it */
#define	TF_NODELAY	0x0004		/* don't delay packets to coalesce */
#define	TF_NOOPT	0x0008		/* don't use tcp options */
#define	TF_SENTFIN	0x0010		/* have sent FIN */
#define TF_REQ_SCALE	0x0020		/* have/will request window scaling */
#define TF_RCVD_SCALE	0x0040		/* other side has requested scaling */
#define TF_REQ_TSTMP	0x0080		/* have/will request timestamps */
#define TF_RCVD_TSTMP	0x0100		/* a timestamp was received in SYN */
#define TF_WAS_RST	0x0200		/* a RST was received in TIME-WAIT */
#define TF_FASTACK	0x0400		/* don't delay ACKs */
#define TF_AGGRESSIVE_ACK 0x0800	/* ack every other segment */
#define TF_IMMEDIATE_ACK 0x1000		/* peer is in slow-start */
#define TF_GOFAST	0x2000		/* LAN go fast mode */
#define TF_RCVD_SACK	0x4000		/* Received SACK option */
#define TF_SNDCHECK_SACK 0x8000		/* Sender should check sacked packets */

#define TF_COPIED_OPTS	(TF_NODELAY|TF_FASTACK)	/* copied on accept() */
#ifdef INET6
	struct	tcptemp t_template;	/* skeletal packet for transmit */
#else
	struct	tcpiphdr t_template;	/* skeletal packet for transmit */
#endif
	struct	inpcb *t_inpcb;		/* back pointer to internet pcb */
	unsigned t_tcpgen;		/* generation # used by tcp_slowtimo */
/*
 * The following fields are used as in the protocol specification.
 * See RFC783, Dec. 1981, page 21.
 */
/* send sequence variables */
	tcp_seq	snd_una;		/* send unacknowledged */
	tcp_seq	snd_nxt;		/* send next */
	tcp_seq	snd_up;			/* send urgent pointer */
	tcp_seq	snd_wl1;		/* window update seg seq number */
	tcp_seq	snd_wl2;		/* window update seg ack number */
	tcp_seq	iss;			/* initial send sequence number */
	u_long	snd_wnd;		/* send window */
/* receive sequence variables */
	u_long	rcv_wnd;		/* receive window */
	tcp_seq	rcv_nxt;		/* receive next */
	tcp_seq	rcv_up;			/* receive urgent pointer */
	tcp_seq	irs;			/* initial receive sequence number */
/*
 * Additional variables for this implementation.
 */
/* receive variables */
	tcp_seq	rcv_adv;		/* advertised window */
/* retransmit variables */
	tcp_seq	snd_max;		/* highest sequence number sent;
					 * used to recognize retransmits
					 */
/* congestion control (for slow start, source quench, retransmit after loss) */
	u_long	snd_cwnd;		/* congestion-controlled window */
	u_long	snd_ssthresh;		/* snd_cwnd size threshhold for
					 * for slow start exponential to
					 * linear switch
					 */
/*
 * transmit timing stuff.  See below for scale of srtt and rttvar.
 * "Variance" is actually smoothed difference.
 */
	short	t_idle;			/* inactivity time */
	short	t_rtt;			/* round trip time */
	tcp_seq	t_rtseq;		/* sequence number being timed */
	short	t_srtt;			/* smoothed round-trip time */
	short	t_rttvar;		/* variance in round-trip time */
	u_short	t_rttmin;		/* minimum rtt allowed */
	u_long	max_sndwnd;		/* largest window peer has offered */

/* out-of-band data */
	char	t_oobflags;		/* have some */
	char	t_iobc;			/* input character */
#define	TCPOOB_HAVEDATA	0x01
#define	TCPOOB_HADDATA	0x02
	short	t_softerror;		/* possible error not yet reported */

/* RFC 1323 */
	u_char	snd_scale;		/* window scaling for send window */
	u_char	rcv_scale;		/* window scaling for recv window */
	u_char	request_r_scale;	/* pending window scaling */
	u_char	requested_s_scale;
	u_int32_t ts_recent;		/* timestamp echo data */
	u_int32_t ts_recent_age;	/* when ts_recent last updated */
	tcp_seq	last_ack_sent;
	time_t	t_purgeat;		/* kill at this time */
	u_short	t_maxseg0;		/* starting maximum segment size */

};

#define	intotcpcb(ip)	((struct tcpcb *)(ip)->inp_ppcb)
#define	sototcpcb(so)	(intotcpcb(sotoinpcb(so)))

/*
 * The tcpcb_sko structure is for internal SGI kernel use ONLY.  It
 * should _never_ be used by applications or by 3rd party kernel modules
 * since it's structure is likely to change between point releases
 * and no binary compatibility guarantees are made.
 */
#ifdef _KERNEL
struct tcpcb_sko {
 	struct tcpcb sko_tcpcb;		/* classic tcpcb */
	tcp_seq	sko_snd_high;		/* end of fast-recovery region */
	struct timespec sko_lastpkttime;/* snd_nxt timestamp */
	tcp_seq	sko_ts_seq;		/* seq. # of timed packet */
	int	sko_fastmode_thresh;

/* RFC 2018 */
	u_char sack_rcvcur;                             /* Next slot to fill with SACK */
	u_char sack_rcvnum;                             /* # of SACK slots filled */
	u_char sack_sndholes;				/* # of SACK holes */
	struct sack_rcvblock sack_rcvblk[SACK_MAX];	/* Most recently sent SACKs */
	struct sack_sndhole sack_sndlist;		/* List of unacked data */
        tcp_seq sack_sndmin;            		/* the lowest seq sacked */
	tcp_seq sack_sndmax;				/* highest seq sacked */
};
/*
 * Macro's used to access these fields
 */
#define SND_HIGH(tp) (((struct tcpcb_sko *)(tp))->sko_snd_high)
#define SKO_TIMESPEC(tp) (((struct tcpcb_sko *)(tp))->sko_lastpkttime)
#define SKO_TS_SEQ(tp) (((struct tcpcb_sko *)(tp))->sko_ts_seq)
#define SKO_FMTHRESH(tp) (((struct tcpcb_sko *)(tp))->sko_fastmode_thresh)
#define NTS_DELTA(a,b) ((((int)(a).tv_sec - (int)(b).tv_sec) * 1000) + \
			(((int)(a).tv_nsec - (int)(b).tv_nsec) / 1000000))
#define TCP_MAX_GOFAST_INC 32768

#define SACK_RCV(tp,n) (((struct tcpcb_sko *)(tp))->sack_rcvblk[n])
#define SACK_RCVCUR(tp) (((struct tcpcb_sko *)(tp))->sack_rcvcur)
#define SACK_RCVNUM(tp) (((struct tcpcb_sko *)(tp))->sack_rcvnum)
#define SACK_RCVCURBLK(tp) (SACK_RCV(tp,SACK_RCVCUR(tp)))
#define SACK_SND(tp)   (((struct tcpcb_sko *)(tp))->sack_sndlist)
#define SACK_SNDMIN(tp) (((struct tcpcb_sko *)(tp))->sack_sndmin)
#define SACK_SNDMAX(tp) (((struct tcpcb_sko *)(tp))->sack_sndmax)
#define SACK_SNDHOLES(tp) (((struct tcpcb_sko *)(tp))->sack_sndholes)
#define SACK_MAXHOLES	255
#define SACK_INCR(x)    (((x) + 1) % SACK_MAX)
#define SACK_DECR(x)    (((x) == 0) ? (SACK_MAX - 1): (x) - 1)
#define SACK_CLEAR(x)    ((x).start = 0, (x).end = 0)
#define SACK_FREE(x)     ((x).start == (x).end)
#ifdef SACK_DEBUG
#define SACK_PRINTF(x) printf x;
#else
#define SACK_PRINTF(x)
#endif


#endif /* _KERNEL */

/*
 * The smoothed round-trip time and estimated variance
 * are stored as fixed point numbers scaled by the values below.
 * For convenience, these scales are also used in smoothing the average
 * (smoothed = (1/scale)sample + ((scale-1)/scale)smoothed).
 * With these scales, srtt has 3 bits to the right of the binary point,
 * and thus an "ALPHA" of 0.875.  rttvar has 2 bits to the right of the
 * binary point, and is smoothed with an ALPHA of 0.75.
 */
#define	TCP_RTT_SCALE		8	/* multiplier for srtt; 3 bits frac. */
#define	TCP_RTT_SHIFT		3	/* shift for srtt; 3 bits frac. */
#define	TCP_RTTVAR_SCALE	4	/* multiplier for rttvar; 2 bits */
#define	TCP_RTTVAR_SHIFT	2	/* multiplier for rttvar; 2 bits */

/*
 * Parameters for deciding when to do aggressive acking (every other
 * data segment) or not.  If the number of bytes acked sent per fasttimo 
 * (200 ms) is less than TCP_AGAK_HYSTERESIS_LO, switch on aggressive acking.  
 * If the number of bytes acked sent per fasttimo is greater than 
 * tcp_agak_hysteresis_hi (a systuneable), switch it off.
 */
#define TCP_AGAK_HYSTERESIS_LO	32


/*
 * The initial retransmission should happen at rtt + 4 * rttvar.
 * Because of the way we do the smoothing, srtt and rttvar
 * will each average +1/2 tick of bias.  When we compute
 * the retransmit timer, we want 1/2 tick of rounding and
 * 1 extra tick because of +-1/2 tick uncertainty in the
 * firing of the timer.  The bias will give us exactly the
 * 1.5 tick we need.  But, because the bias is
 * statistical, we have to test that we don't drop below
 * the minimum feasible timer (which is 2 ticks).
 * This macro assumes that the value of TCP_RTTVAR_SCALE
 * is the same as the multiplier for rttvar.
 */
#define	TCP_REXMTVAL(tp) \
	(((tp)->t_srtt >> TCP_RTT_SHIFT) + (tp)->t_rttvar)

/*
 * SGI: The statistics formerly in this file are now in sys/tcpipstats.h
 */
#ifdef _KERNEL
struct ipsec;
extern struct	inpcb tcb;		/* head of queue of active tcpcb's */
void tcp_template(struct tcpcb *);
struct	tcpcb *tcp_close(struct tcpcb *);
struct	tcpcb *tcp_drop(struct tcpcb *, int, struct ipsec *);
struct	tcpcb *tcp_timers(struct tcpcb *, __psint_t);
struct	tcpcb *tcp_disconnect(struct tcpcb *, struct ipsec *);
struct	tcpcb *tcp_usrclosed(struct tcpcb *);

void	tcp_setpersist(struct tcpcb *);

int	tcp_mss(struct tcpcb *, u_short);
int	tcp_output(struct tcpcb *, struct ipsec *);
int	tcp_usrreq(struct socket *, int, struct mbuf *,
		   struct mbuf *, struct mbuf *);

void sack_rcvupdate(struct tcpcb *tp, tcp_seq start, tcp_seq end);
void sack_rcvacked(struct tcpcb *tp, tcp_seq ack);
void sack_rcvcleanup(struct tcpcb *tp);
void sack_sndupdate(struct tcpcb *tp, tcp_seq start, tcp_seq end);
void sack_sndacked(struct tcpcb *tp, tcp_seq ack);
void sack_sndlist(struct tcpcb *tp, char *opt, int len);
int sack_sndcheck(struct tcpcb *tp, tcp_seq start, tcp_seq end);
void sack_sndcleanup(struct tcpcb *tp);
void sack_dump(char *, int);

extern u_int32_t	tcp_now;	/* RFC 1323 timestamp clock. */

#ifdef INET6
void	tcp_respond(struct tcpcb *, void *, struct mbuf *,
		    tcp_seq, tcp_seq, int, struct ipsec *, int);
#else
void	tcp_respond(struct tcpcb *, struct tcpiphdr *, struct mbuf *,
		    tcp_seq, tcp_seq, int, struct ipsec *);
#endif

#endif

#define PFX(s) _BSD_ ## s
#define MD5Init		PFX(MD5Init)
#define MD5Update	PFX(MD5Update)
#define MD5Final	PFX(MD5Final)

#define MD5_DIGEST_LEN 16
typedef struct {
	u_int32_t state[4];		/* state (ABCD) */
	u_int32_t count[2];		/* # of bits, modulo 2^64 (LSB 1st) */
	unsigned char buffer[64];	/* input buffer */
} MD5_CTX;
extern void MD5Init(MD5_CTX *);
extern void MD5Update(MD5_CTX *, u_char *, u_int);
extern void MD5Final(u_char[MD5_DIGEST_LEN], MD5_CTX*);

extern tcp_seq iss_hash(struct tcpcb *);

void insque_tcb(struct tcpiphdr *, struct tcpiphdr *);
void remque_tcb(struct tcpiphdr *);
#ifdef __cplusplus
}
#endif
