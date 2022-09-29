/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Id: disp_idbg.c,v 1.89 1999/08/26 22:49:00 sfoehner Exp $"

#include "sgidefs.h"
#include "sys/types.h"
#include "sys/param.h"
#include "sys/idbgentry.h"
#include "sys/space.h"
#include "sys/proc.h"
#include "sys/sched.h"
#include "os/proc/pproc_private.h"	/* XXX bogus */
#include "q.h"
#include "sys/rt.h"
#include <ksys/sthread.h>
#include <ksys/xthread.h>
#include <sys/miser_public.h>
#include <sys/cpumask.h>

extern batchq_t batch_queue;
extern struct q_element_s miser_queue;
extern struct q_element_s miser_batch;
extern kthread_t** batch_bindings;
extern cpumask_t   batch_cpumask;

extern void idbg_mq(__psint_t x);
extern void idbg_vpagg(__psint_t x);
extern void idbg_mqb_disp(struct q_element_s *);
extern char *idbg_kthreadname(kthread_t*);

static void
doq(struct q_element_s *q, char *parent_type)
{
	struct q_element_s *qe;

	qprintf("     queue 0x%x: %s\n", q, q_empty(q) ? "empty" : "");
	qprintf("     forw 0x%x back 0x%x\n", q->qe_forw, q->qe_back);
	qprintf("     parent 0x%x\n", q->qe_parent);

	/* print elements of q, if any */
	qe = q->qe_forw;
	if (qe == q) {
		qprintf("          No %ss in q\n", parent_type);
		return;
	}

	qprintf("\n");
	while (qe != q) {
		qprintf("     qe 0x%x: forw 0x%x back 0x%x\n",
			qe, qe->qe_forw, qe->qe_back);
		qprintf("     %s 0x%x", parent_type, qe->qe_parent);
		if (!strcmp(parent_type,"mqb"))
			idbg_mqb_disp(qe->qe_parent);
		qe = qe->qe_forw;
		qprintf("\n");
	}
}


static void
idbg_docpu(cpuid_t cpu, cpu_t *cs)
{
	kthread_t *kt;
	qprintf("cpu %d 0x%x\n", cpu, cs);
	qprintf("     lock 0x%x  restricted %d\n", &cs->c_lock,
						   cs->c_restricted);
#if MP
	qprintf("     onq %d peek %d\n", cs->c_onq, cs->c_peekrotor);
#endif
	if (kt = cs->c_threads)
		do {
			qprintf("     kthread 0x%x %s\n", kt, idbg_kthreadname(kt));
			kt = kt->k_rflink;
		} while (cs->c_threads != kt);
	qprintf("\n");
}

void
sched_stddbgp(job_t *j)
{
	qprintf("No accounting for job of type %s\n", j->j_ops.jo_str);
}

char *
kjpolicy_to_string(int kjp)
{
	switch (kjp) {
		case SCHED_TS:		return("TS");
		case SCHED_FIFO:	return("FIFO");
		case SCHED_RR:		return("RR");
		case SCHED_NP: 		return("NP");
		default:		return("unknown");
	}
}

/* NOTE: this is part of the exported functions, io/idbg.c:doproc() uses it */ 
void
print_ssthread(kthread_t *kt)
{
	if (KT_ISUTHREAD(kt))
		qprintf(" job 0x%x oncpu %d onrq %d policy %s\n",
			KT_TO_UT(kt)->ut_job, kt->k_sonproc, kt->k_onrq,
			kjpolicy_to_string(KT_TO_UT(kt)->ut_policy));
}

void
idbg_pcpu(__psint_t x)
{
	cpuid_t	cpu;

	if (x == -1L) {
		for (cpu = 0; cpu < maxcpus; cpu++)
			if (cpu_enabled(cpu))
				idbg_docpu(cpu, &pdaindr[cpu].pda->p_cpu);
	} else if (x >= 0 && x < maxcpus)
		idbg_docpu(x, &pdaindr[x].pda->p_cpu);
	else {
		for (cpu = 0; cpu < maxcpus; cpu++)
			if (cpu_enabled(cpu) &&
			    ((cpu_t *)x == &pdaindr[cpu].pda->p_cpu)) {
				idbg_docpu(cpu, (cpu_t *)x);
				break;
			}
		if (cpu == maxcpus)
			qprintf("CPU ID not valid (%d)\n", x);
	}	
}

/* ARGSUSED */
void idbg_batch(__psint_t x) 
{
	int i = 0;

	qprintf("Batch queue \n");
	doq(&batch_queue.b_queue,"job");

	qprintf("Batch onq %d \n", batch_queue.b_work_onq);
	doq(&miser_queue, "mqq");
	doq(&miser_batch, "mqb");
	qprintf("End Batch queue \n");
	
	qprintf("Bindings \n");
	for (i = 0; i < maxcpus; i++) 
		qprintf("binding[%d]=0x%x \t", i, batch_bindings[i]);
	qprintf("\n");
	for (i = 0; i < maxcpus; i++) {
		if (CPUMASK_TSTB(batch_cpumask, i))
			qprintf(" %d ", i);
		else
			qprintf(" X ");
	}
	qprintf("\n");
} 

void
idbg_job(__psint_t x)
{
	if (x == -1L) {
		extern wtree_t		wtree;
		job_t *			j;

		qprintf("wtree: lock 0x%x", &wtree.wt_lock);
#if MP
		qprintf(" rotor %d", wtree.wt_rotor);
#endif
#ifdef DEBUG
		qprintf(" cnt %d twt %lld bal %lld mul %lld acc %lld\n",
			wtree.wt_lastcnt, wtree.wt_lasttwt, wtree.wt_lastbal,
			wtree.wt_lastmul, wtree.wt_lastacc);
#else
		qprintf("\n");
#endif
		if (JOB_FIRST(&wtree) == JOB_ENDQ(&wtree))
			qprintf("No jobs in wtree\n");

		for (j = JOB_FIRST(&wtree);
		     j != JOB_ENDQ(&wtree);
		     j = JOB_NEXT(j) ) {
			kthread_t	*kt;

			qprintf("job 0x%x\n", j);
			qprintf("    nthreads %d\n", j->j_nthreads);
			j->j_ops.jo_dbgp(j);
			for (kt = j->j_thread; kt; kt = kt->k_link)
				qprintf("          kthread 0x%x oncpu %d"
					" onrq %d pri %d\n", kt, kt->k_sonproc,
					kt->k_onrq, kt->k_pri);

		}
		qprintf("\n");

	} else {
		job_t *j = (job_t *)x;
		kthread_t *kt;

		qprintf("job 0x%x\n", j);
		qprintf("locks 0x%x\n", &j->j_flags);
		qprintf(" sv 0x%x \n", j->j_sv);
		qprintf("     flags 0x%x type %s\n",
			j->j_flags, j->j_ops.jo_str);
		qprintf("     nthreads %d\n", j->j_nthreads);
		j->j_ops.jo_dbgp(j);
		qprintf("     forw 0x%x back 0x%x\n",
			j->j_queue.qe_forw, j->j_queue.qe_back);
		qprintf("     parent 0x%x\n", j->j_queue.qe_parent);
		for (kt = j->j_thread; kt; kt = kt->k_link)
			qprintf("        kthread 0x%x oncpu %d onrq %d\n",
					kt, kt->k_sonproc, kt->k_onrq);
	}
}

/* ARGSUSED */
void
idbg_rt(__psint_t x)
{
	kthread_t *kt;
	proc_t	  *p;
	int i;
	extern int resetpri[];

	qprintf("Lower bounds \n");
	for (i = 1; i < maxcpus; i++) {
		if (cpusets[i].cs_idler != -2)
			qprintf("cpuset[%d] name [%s] lbpri %d \n", i,
				&cpusets[i].cs_name, cpusets[i].cs_lbpri);
	}
	qprintf("Bound Cpus:");
	for (i = 0; i < maxcpus; i++) {
		if (!bindings[i])
			continue;
		kt = bindings[i];
		if (KT_ISUTHREAD(kt)) {
		    p = UT_TO_PROC(KT_TO_UT(kt));
		    qprintf("\n     cpu %d:rbind %d proc 0x%x pid %d pri %d",
			i, kt->k_binding, p, p->p_pid, kt->k_pri);
		} else
			qprintf("\n     cpu %d: kthread 0x%x pri %d",
				i, kt, kt->k_pri);
	}
	qprintf("\nFree Cpus:");
	for (i = 0; i < maxcpus; i++) {
		if (bindings[i])
			continue;
		qprintf(" %d", i);
	}
	qprintf("\nOverflow Q:");
	if (kt = rt_gq)
		do {
			if (KT_ISUTHREAD(kt)) {
			        p = UT_TO_PROC(KT_TO_UT(kt));
				qprintf("\n    rbind %d proc 0x%x pid %d pr %d",
					kt->k_binding, p, p->p_pid, kt->k_pri);
			} else
				qprintf("\n    kthread 0x%x pri %d",
								kt, kt->k_pri);
			kt = kt->k_rflink;
		} while (kt != rt_gq);
	qprintf("\n");
}

#define MAXRTTHREADS		512
static struct q_element_s 	priq[MAXRTTHREADS];
static struct q_element_s 	priqh;

static void
add_priq(struct q_element_s *nqe, pri_t npri)
{
	struct q_element_s *qe;

	/* insert in pri order */
	for (qe = priqh.qe_forw; qe != &priqh; qe = qe->qe_forw)
		if (((kthread_t *) qe->qe_parent)->k_pri < npri)
			break;
	q_insert_before(qe == &priqh ? &priqh : qe, nqe);
}

/* ARGSUSED */
static void
idbg_allrt(__psint_t x)
{
	int 	i = 0;
	extern sthread_t sthreadlist;
	extern xthread_t xthreadlist;
	xthread_t *xt;
	sthread_t *st;
	kthread_t *kt;
	struct q_element_s *qe;

	init_q_element(&priqh, &priqh);

	for (st = sthreadlist.st_next; st != &sthreadlist; st = st->st_next, i++) {
		if (i >= MAXRTTHREADS)
			goto printit;
		init_q_element(&priq[i], ST_TO_KT(st));
		add_priq(&priq[i], ST_TO_KT(st)->k_pri);
	}
	for (xt = xthreadlist.xt_next; xt != &xthreadlist; xt = xt->xt_next, i++) {
		if (i >= MAXRTTHREADS)
			goto printit;
		init_q_element(&priq[i], XT_TO_KT(xt));
		add_priq(&priq[i], XT_TO_KT(xt)->k_pri);
	}


printit:
	for (qe = priqh.qe_forw; qe != &priqh; qe = qe->qe_forw) {

		kt = qe->qe_parent;
		switch (kt->k_type) {
		case KT_STHREAD:
			st = KT_TO_ST(kt);
			qprintf("ST 0x%x [%s] ", st, st->st_name);
			break;
		case KT_XTHREAD:
			xt = KT_TO_XT(kt);	
			qprintf("XT 0x%x [%s] ", xt, xt->xt_name);
			break;
		default:
			qprintf("UNK ktype 0x%x ", kt);
		}
		qprintf("runq %d cpu %d pri(%d) binding %d\n",
			kt->k_onrq, kt->k_sonproc, kt->k_pri, kt->k_binding);
	}
	if (i >= MAXRTTHREADS)
		qprintf("Warning: Only %d threads displayed\n", MAXRTTHREADS);
}

extern void idbg_cpuset(__psint_t);
extern void idbg_rtpri(__psint_t);
extern void idbg_rtq(__psint_t);
void
disp_idbg_init(void)
{
	extern void idbg_gang(__psint_t);
	idbg_addfunc("cpuset", idbg_cpuset);
	idbg_addfunc("batch", idbg_batch);
	idbg_addfunc("pcpu", idbg_pcpu);
	idbg_addfunc("job", idbg_job);
	idbg_addfunc("rt", idbg_rt);
	idbg_addfunc("allrt", idbg_allrt);
	idbg_addfunc("gang", idbg_gang);
	idbg_addfunc("mqb", idbg_mq); 
	idbg_addfunc("mq", idbg_mq);
	idbg_addfunc("mqq", idbg_mq);
	idbg_addfunc("vpagg", idbg_vpagg);
	idbg_addfunc("rtpri", idbg_rtpri);
	idbg_addfunc("rtq", idbg_rtq);
}
