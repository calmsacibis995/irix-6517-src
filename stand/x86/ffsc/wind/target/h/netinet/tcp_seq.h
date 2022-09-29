/* tcp_seq.h - tcp sequence header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *      @(#)tcp_seq.h   7.3 (Berkeley) 6/29/88
 */

/*
modification history
--------------------
02g,22sep92,rrr  added support for c++
02f,26may92,rrr  the tree shuffle
02e,04oct91,rrr  passed through the ansification filter
		  -changed copyright notice
02d,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
02c,16apr89,gae  updated to new 4.3BSD.
02b,04nov87,dnw  moved definition of tcp_iss to tcp_input.c.
02a,03apr87,ecs  added header and copyright.
*/

#ifndef __INCtcp_seqh
#define __INCtcp_seqh

#ifdef __cplusplus
extern "C" {
#endif

/*
 * TCP sequence numbers are 32 bit integers operated
 * on with modular arithmetic.  These macros can be
 * used to compare such integers.
 */
#define	SEQ_LT(a,b)	((int)((a)-(b)) < 0)
#define	SEQ_LEQ(a,b)	((int)((a)-(b)) <= 0)
#define	SEQ_GT(a,b)	((int)((a)-(b)) > 0)
#define	SEQ_GEQ(a,b)	((int)((a)-(b)) >= 0)

/*
 * Macros to initialize tcp sequence numbers for
 * send and receive from initial send and receive
 * sequence numbers.
 */
#define	tcp_rcvseqinit(tp) \
	(tp)->rcv_adv = (tp)->rcv_nxt = (tp)->irs + 1

#define	tcp_sendseqinit(tp) \
	(tp)->snd_una = (tp)->snd_nxt = (tp)->snd_max = (tp)->snd_up = \
	    (tp)->iss

#define	TCP_ISSINCR	(125*1024)	/* increment for tcp_iss each second */

IMPORT tcp_seq	tcp_iss;		/* tcp initial send seq # */

#ifdef __cplusplus
}
#endif

#endif /* __INCtcp_seqh */
