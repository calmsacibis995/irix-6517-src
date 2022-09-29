/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and that due credit is given
 * to the University of California at Berkeley. The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission. This software
 * is provided ``as is'' without express or implied warranty.
 *
 *	@(#)tcp_seq.h	7.2 (Berkeley) 12/7/87
 */
#ifdef __cplusplus
extern "C" {
#endif

/*
 * TCP sequence numbers are 32 bit integers operated
 * on with modular arithmetic.  These macros can be
 * used to compare such integers.
 */
#define	SEQ_LT(a,b)	((__int32_t)((a)-(b)) < 0)
#define	SEQ_LEQ(a,b)	((__int32_t)((a)-(b)) <= 0)
#define	SEQ_GT(a,b)	((__int32_t)((a)-(b)) > 0)
#define	SEQ_GEQ(a,b)	((__int32_t)((a)-(b)) >= 0)
#define SEQ_EQ(a,b)	((a) == (b))
#define SEQ_NE(a,b)	((a) != (b))

/*
 * Macros to initialize tcp sequence numbers for
 * send and receive from initial send and receive
 * sequence numbers.
 */
#define	tcp_rcvseqinit(tp) \
	(tp)->rcv_adv = (tp)->rcv_nxt = (tp)->irs + 1

#define	tcp_sendseqinit(tp) \
	SND_HIGH((tp)) = (tp)->snd_una = (tp)->snd_nxt = (tp)->snd_max = \
	(tp)->snd_up = (tp)->iss

#define	TCP_ISSINCR	(125*1024)	/* increment for tcp_iss each second */

#ifdef _KERNEL
tcp_seq	tcp_iss;		/* tcp initial send seq # */
#endif
#ifdef __cplusplus
}
#endif
