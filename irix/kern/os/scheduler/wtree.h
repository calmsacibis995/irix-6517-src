/**************************************************************************
 *									  *
 * 		 Copyright (C) 1997 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef __WTREE_H__
#define __WTREE_H__

#include <sys/types.h>
#include <sys/atomic_ops.h>

typedef int64_t				credit_t;
typedef int64_t				weight_t;

struct job_s;


typedef struct wtree_s {
	lock_t				wt_lock;
#if MP
	cpuid_t				wt_rotor;
#endif
	struct q_element_s		wt_ajobq;
#ifdef DEBUG
	uint				wt_lastcnt;
	weight_t			wt_lasttwt;
	credit_t			wt_lastbal;
	credit_t			wt_lastmul;
	credit_t			wt_lastacc;
#endif
	weight_t			wt_weights[NZERO * 2 + 1];
} wtree_t;


#define JOB_FIRST(__w)	((struct job_s *) (__w)->wt_ajobq.qe_forw->qe_parent)
#define JOB_NEXT(__j)	((struct job_s *) (__j)->j_queue.qe_forw->qe_parent)
#define JOB_ENDQ(__w)	((struct job_s *) (__w))



void		wtree_init		(void);
void		wtree_sched_tick	(void);

void		wt_init			(struct job_s *j, kthread_t *kt);
int		wt_get_priority		(kthread_t *);
void		wt_set_weight		(kthread_t *, int);


#endif
