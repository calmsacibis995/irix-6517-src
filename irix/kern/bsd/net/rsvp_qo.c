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

/*
 * Operations on the packet scheduling queue object.
 */

#ident "$Revision: 1.7 $"

#include <sys/types.h>
#include <sys/debug.h>
#include <sys/kthread.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/mbuf.h>

#include "rsvp_qo.h"


/*
 * Initialize the queue object 
 */
void
psqo_init(struct psqo *psqo_p, char *lock_name)
{
	bzero(psqo_p, sizeof(struct psqo));
	init_spinlock(&(psqo_p->qo_lock), lock_name, 1);
}


/*
 * Enqueue a mbuf on the specified queue.
 * Returns -1 on error (caller should free mbuf), 0 if OK (queue has
 * mbuf).
 */
int 
psqo_enq_pkt(struct psqo *psqo_p, int qid, struct mbuf *m, struct sockaddr *sa,
struct rtentry *rte)
{
	struct dest *dest_p;
	struct psq *psq_p;
	int s, enq_ind;

	switch(qid) {
	case PSQO_QID_CL:
		psq_p = &(psqo_p->qo_cl);
		break; 
	case PSQO_QID_BE:
		psq_p = &(psqo_p->qo_be);
		break;
	case PSQO_QID_NON_CONFORM:
		psq_p = &(psqo_p->qo_nc);
		break;
	default:
#ifdef DEBUG
		printf("psqo_enq_pkt: unkonwn queue type, qid %d\n", 
		       qid);
#endif
		return -1;
	}

	s = mutex_spinlock(&psqo_p->qo_lock);

	/*
	 * Check for queue full.  If not, point dest_p at the next
	 * empty slot in the queue.
	 */
	if ((psq_p->psq_enq_ind + 1 == psq_p->psq_deq_ind) ||
	    ((psq_p->psq_enq_ind == psq_p->psq_max_ind) && 
	     (psq_p->psq_deq_ind == 0))) {
		mutex_spinunlock(&psqo_p->qo_lock, s);
		return -1;
	} else {
		if (psq_p->psq_enq_ind == psq_p->psq_max_ind)
			enq_ind = 0;
		else
			enq_ind = psq_p->psq_enq_ind + 1;
		dest_p = &(psq_p->psq_darray[enq_ind]);
		if (dest_p->d_m != NULL) {
			mutex_spinunlock(&psqo_p->qo_lock, s);
			return -1;
		}
	}
	psq_p->psq_enq_ind = enq_ind;
	psq_p->psq_qlen++;
	psq_p->psq_pkts++;
	psqo_p->qo_num++;

	dest_p->d_addr = *sa;
	dest_p->d_m = m;
	dest_p->d_rte = rte;

	mutex_spinunlock(&psqo_p->qo_lock, s);

	return 0;
}


/*
 * Return a dest structure pointing to the next mbuf that should
 * be transmitted.  This function also decides what queue to pull
 * packets from.  Currently, the heirarchy is very simple :
 * 1. conformant CL 
 * 2. BE
 * 3. non-conformant CL
 * The caller is also allowed to specify the lowest priority queue
 * to get packets from via which_q
 */
struct dest *
psqo_get_next_pkt(struct psqo *psqo_p, int which_q)
{
	struct psq  *psq_p;
	struct dest *dest_p = NULL;
	int s;

	s = mutex_spinlock(&psqo_p->qo_lock);

	psq_p = &(psqo_p->qo_cl);
	if (psq_p->psq_deq_ind != psq_p->psq_enq_ind) {
		if (psq_p->psq_deq_ind == psq_p->psq_max_ind)
			psq_p->psq_deq_ind = 0;
		else
			psq_p->psq_deq_ind++;
		ASSERT(psq_p->psq_qlen);
		psq_p->psq_qlen--;
		psqo_p->qo_num--;
		dest_p = &(psq_p->psq_darray[psq_p->psq_deq_ind]);
		goto next_pkt_out;
	}

	if (which_q <= PSQO_QID_CL)
		goto next_pkt_out;

	psq_p = &(psqo_p->qo_be);
	if (psq_p->psq_deq_ind != psq_p->psq_enq_ind) {
		if (psq_p->psq_deq_ind == psq_p->psq_max_ind)
			psq_p->psq_deq_ind = 0;
		else
			psq_p->psq_deq_ind++;
		ASSERT(psq_p->psq_qlen);
		psq_p->psq_qlen--;
		psqo_p->qo_num--;
		dest_p = &(psq_p->psq_darray[psq_p->psq_deq_ind]);
		goto next_pkt_out;
	}

	if (which_q <= PSQO_QID_BE)
		goto next_pkt_out;

	psq_p = &(psqo_p->qo_nc);
	if (psq_p->psq_deq_ind != psq_p->psq_enq_ind) {
		if (psq_p->psq_deq_ind == psq_p->psq_max_ind)
			psq_p->psq_deq_ind = 0;
		else
			psq_p->psq_deq_ind++;
		ASSERT(psq_p->psq_qlen);
		psq_p->psq_qlen--;
		psqo_p->qo_num--;
		dest_p = &(psq_p->psq_darray[psq_p->psq_deq_ind]);
	}

next_pkt_out:
	mutex_spinunlock(&psqo_p->qo_lock, s);
	return dest_p;
}


/*
 * Adjust the number of dest structs on the free list.
 * If num is positive, add more dest structs, else remove dest structs.
 * The malloc can sleep, so this function should always succeed.
 */
void
psqo_alloc_queue(struct psqo *psqo_p, int qid, int num)
{
	struct psq *psq_p;

	switch(qid) {
	case PSQO_QID_CL:
		psq_p = &(psqo_p->qo_cl);
		break;
	case PSQO_QID_BE:
		psq_p = &(psqo_p->qo_be);
		break;
	case PSQO_QID_NON_CONFORM:
		psq_p = &(psqo_p->qo_nc);
		break;
	default:
#ifdef DEBUG
		printf("psqo_alloc_queue: unknown qid %d\n", qid);
#endif
		return;
	}
#ifdef DEBUG
	if (psq_p->psq_darray != NULL)
		printf("psqo_alloc_queue: psq_darray not NULL 0x%x\n",
		       psq_p->psq_darray);
#endif
	psq_p->psq_darray = kmem_zalloc(num * sizeof(struct dest), 0);
	psq_p->psq_enq_ind = 0;
	psq_p->psq_deq_ind = 0;
	psq_p->psq_max_ind = num - 1;
}


/*
 * Called when packet scheduling is turned off.  Free the allocated
 * queue structure.
 */
void
psqo_free_queue(struct psqo *psqo_p, int qid)
{
	struct psq *psq_p;

	switch(qid) {
	case PSQO_QID_CL:
		psq_p = &(psqo_p->qo_cl);
		break;
	case PSQO_QID_BE:
		psq_p = &(psqo_p->qo_be);
		break;
	case PSQO_QID_NON_CONFORM:
		psq_p = &(psqo_p->qo_nc);
		break;
	default:
#ifdef DEBUG
		printf("psqo_free_queue: unknown qid %d\n", qid);
#endif
		return;
	}
#ifdef DEBUG
	if (psq_p->psq_enq_ind != psq_p->psq_deq_ind)
		printf("psqo_free_queue: queue not empty, enq %d deq %d\n",
		       psq_p->psq_enq_ind, psq_p->psq_deq_ind);
#endif
	kmem_free(psq_p->psq_darray, (psq_p->psq_max_ind + 1) * sizeof(struct dest));
	psq_p->psq_darray = NULL;
	psq_p->psq_max_ind = 0;
	psq_p->psq_qlen = 0;
}

