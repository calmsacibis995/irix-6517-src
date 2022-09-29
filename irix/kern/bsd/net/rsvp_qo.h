/*
 * Copyright 1995 Silicon Graphics, Inc. 
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or 
 * duplicated in any form, in whole or in part, without the prior written 
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions 
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or 
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished - 
 * rights reserved under the Copyright Laws of the United States.
 */

#ident "$Revision: 1.5 $"

/*
 * Types of queues for a packet to get put on
 */
#define PSQO_QID_CL		1
#define PSQO_QID_BE		2
#define PSQO_QID_NON_CONFORM	3

/*
 * struct dest acts as a handle for the pkts that are queued.
 */
struct dest {
	struct mbuf *d_m;
	struct rtentry	*d_rte;
	struct sockaddr d_addr;
};

/*
 * A queue holds packets from a certain class of packets.  There is
 * a seperate queue for the Controlled Load, best effort, and non-conforming
 * class of packets.  The queue is implemented as a ring.  psq_inq_ind
 * points to the next slot to queue a packet.  psq_deq_ind points to
 * the next slot to dequeue a packet.
 */
struct psq {
	struct dest	*psq_darray;
	int		psq_enq_ind;
	int		psq_deq_ind;
	int		psq_max_ind;
	int		psq_qlen;	/* current qlen			*/
	uint		psq_pkts;	/* total pkts serviced		*/
};

/*
 * A queue object is a collection of queues.  Each packet scheduling
 * interface element has one queue object.
 */
struct psqo {
	int		qo_num;		/* number of pkts in all queues */
	lock_t		qo_lock;	/* big lock for all queues	*/
	struct psq	qo_cl;		/* controlled-load queue	*/

	struct psq	qo_be;		/* best effort queue		*/
	struct psq	qo_nc;		/* non-conformant queue		*/
};


/* 
 * Public functions of rsvp_qo
 */
void		psqo_init(struct psqo *, char *);
int		psqo_enq_pkt(struct psqo *, int qid, struct mbuf *,
			     struct sockaddr *, struct rtentry *);
struct dest *	psqo_get_next_pkt(struct psqo *, int);
void		psqo_alloc_queue(struct psqo *, int qid, int num);
void		psqo_free_queue(struct psqo *, int qid);

