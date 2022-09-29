/* @(#)nfs_server.c	1.7 88/08/06 NFSSRC4.0 from 2.85 88/02/08 SMI
 *
 * Copyright (c) 1987 by Sun Microsystems, Inc.
 */
#ident      "$Revision: 1.46 $"

#define NFSSERVER
#include "types.h"
#include <string.h>
#include <sys/buf.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/dirent.h>
#include <sys/errno.h>
#include <ksys/vfile.h>
#include <sys/mbuf.h>
#include <sys/mkdev.h>
#include <sys/pathname.h>
#include <sys/pda.h>
#include <sys/proc.h>
#include <sys/sema.h>
#include <sys/siginfo.h>	/* for CLD_EXITED exit code */
#include <sys/socket.h>
#include <sys/vsocket.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/tcpipstats.h>
#include <sys/mac_label.h>
#include <sys/uuid.h>
#include <sys/capability.h>
#include <sys/sysmacros.h>
#include <sys/unistd.h>
#include <net/if.h>
#include <sys/atomic_ops.h>
#include <sys/runq.h>
#include <ksys/xthread.h>
#include "auth.h"
#include "auth_unix.h"
#include "svc.h"
#include "xdr.h"
#include "nfs.h"
#include "export.h"
#include "nfs_stat.h"
#include "nfs_impl.h"
#include <rpcsvc/nlm_prot.h>
#include "lockd_impl.h"

int nfsq_total;			/* total number of daemons */
int nfsq_init;			/* initial number of daemons */
mutex_t nfsd_count_lock;		/* lock for above */
extern void nfs_svckudp(SVCXPRT *, struct mbuf *, struct pernfsq *);
extern void nfstcp_service(SVCXPRT *, void *, struct pernfsq *);
extern void *nfstcp_DQGLOBAL(void);
extern int nfstcp_start(struct vsocket *, int);
extern void nfstcp_stop(void);
extern void nfstcp_reapconn(void);
static int nfs_runflags;
static void
nfs_svc_run(SVCXPRT *xprt, struct pernfsq *p);

extern int nfs_port;		/* hook into the udp layer */
extern int nfstcp_doreap;	/* reap dangling tcp connections */

void
stop_nfsd(void)
{
	mutex_lock(&nfsd_count_lock, PZERO - 1);
	if (--nfsq_total == 0) {
		nfs_port = -1;
		nfsq_init = 0;
		nfs_runflags = 0;
		nfstcp_stop();
	}
	mutex_unlock(&nfsd_count_lock);
}

/*
 * NFS Server system call.
 * Does all of the work of running a NFS server.
 * sock is the fd of an open UDP or TCP socket.
 * lockd runs within nfsd 
 */
/*
 * The first arg here, xxx_kudzu_hack, is used to determine if an old
 * nfsd is calling.  This is for the transition period when people 
 * may be running the kudzu kernel with a 6.2 nfsd.  6.2 nfsd will
 * have a small integer file descriptor in this arg, kudzu has -1.
 * This arg should be removed when people are no longer using 6.2 nfsds.
 */
struct nfssvca {
	sysarg_t xxx_kudzu_hack;
	sysarg_t sock;
	sysarg_t flags;
	sysarg_t connections;
};
/*
 * Flag values.  If TCP is enabled, then the sock and connections
 * arguments are assumed valid.
 */
#define NFSD_TCP	0x1
#define NFSD_UDP	0x2

/* ARGSUSED */
int
nfs_svc(struct nfssvca *uap)
{
	extern struct mac_label *mac_equal_equal_lp;
	SVCXPRT *xprt;
	struct vsocket *vso;
	struct socket *so;
	int number;			/* nfsq number */
	int error;
#ifdef SN
	cnodemask_t cnm;
#endif

	/*
	 * Must have sufficient privilege to provide NFS services:
	 * DAC override, MAC override, and network management.
	 */
	if (!_CAP_ABLE(CAP_DAC_OVERRIDE) || !_CAP_ABLE(CAP_MAC_READ) ||
	    !_CAP_ABLE(CAP_MAC_WRITE) || !_CAP_ABLE(CAP_NETWORK_MGT))
		return EPERM;

	if (uap->xxx_kudzu_hack == (sysarg_t)-1) {
	    nfs_runflags |= uap->flags;
	if (uap->flags & NFSD_TCP) {
	    if (error = getvsock(uap->sock, &vso))
		return error;
	    so = (struct socket *)(BHV_PDATA(VSOCK_TO_FIRST_BHV(vso)));
	    if (so->so_type != SOCK_STREAM) 
		return EINVAL;
	    /*
	     * start tcp services.  Won't return unless error or signal.
	     */
	    if (error = nfstcp_start(vso, uap->connections))
		return error;
	    exit(CLD_EXITED, 0);
	}
	} else
	    nfs_runflags = NFSD_UDP;

	/*
	 * We just need the user area to block, so release the VM.
	 * Some day we should use lighter-weight threads.
	 * Use a non-degrading priority that overlaps interactive users.
	 */
	if (error = vrelvm())
		return error;

	mutex_lock(&nfsd_count_lock, PZERO - 1);
	number = nfsq_total++;
	/*
	 * Enable udp socket bypass reception.
	 */
	if (number == 0)
		nfs_port = NFS_PORT;
	number %= max_nfsq;
	nfsq_init++;
	mutex_unlock(&nfsd_count_lock);

	/*
	 * Each thread can handle either a udp or tcp request.  
	 * The socket (tcp only) and xprt type are set per received
	 * request. We try a bit to keep each thread close to the
	 * node where the data is located.
	 */
#ifdef SN
	xprt = svckrpc_create(number);
	CNODEMASK_CLRALL(cnm);
	CNODEMASK_SETB(cnm, number);
	curthreadp->k_nodemask = cnm;
#else
	xprt = svckrpc_create(0);
#endif

	start_lockd();
	/*
	 * Give each daemon a private copy of the credentials
	 * (Bug 492072) give it a zero cap_set.
	 * (Bug 579942) give it a wildcard mac label.
	 */
	set_current_cred(crdup(get_current_cred()));
	/* opposite of cap_empower_cred(get_current_cred()) */
	get_current_cred()->cr_cap.cap_effective = CAP_ALL_OFF;
	get_current_cred()->cr_cap.cap_inheritable = CAP_ALL_OFF;
	get_current_cred()->cr_cap.cap_permitted = CAP_ALL_OFF;
	get_current_cred()->cr_mac = mac_equal_equal_lp;

	nfs_svc_run(xprt, nfsq_table[number]);
	/*
	 * daemons signalled to exit will return here
	 */
	stop_lockd();
	stop_nfsd();
	exit(CLD_EXITED, 0);
	/* NOTREACHED */
}

/*
 * streamlined nfs service loop
 */
extern struct smap *smapQhead;
extern int smapQlen;

static void
nfs_svc_run(SVCXPRT *xprt, struct pernfsq *p)
{
	int s; 
	int justwoke, didsomething;
	struct mbuf *m;
	void *vp;
	struct pernfsq *first_p = p;

	justwoke = FALSE;
	while (TRUE) {
		if (atomicClearInt(&nfstcp_doreap, 1)) {
			#pragma mips_frequency_hint NEVER
			nfstcp_reapconn();
		}
		s = mutex_spinlock(&p->nfsq.ifq_lock);
retry_new_pernfsq:
		IF_DEQUEUE_NOLOCK(&p->nfsq, m);
		didsomething = 0;
		if (m != NULL) {
			mutex_spinunlock(&p->nfsq.ifq_lock, s);
			didsomething = 1;
			justwoke = FALSE;
			nfs_svckudp(xprt, m, p);
		}
		if (vp = nfstcp_DQGLOBAL()) {
			if (didsomething == 0)
				mutex_spinunlock(&p->nfsq.ifq_lock, s);
			didsomething = 1;
			justwoke = FALSE;
			nfstcp_service(xprt, vp, p);
		}
		if (didsomething)
			continue;
		if (justwoke) {
			#pragma mips_frequency_hint NEVER
			RSSTAT.rsnullrecv++;
			justwoke = FALSE;
		}
		mutex_spinunlock(&p->nfsq.ifq_lock, s);
		p = first_p;
		do {
			if (p->nfsq.ifq_len &&
			    (s = mutex_spintrylock(&p->nfsq.ifq_lock)))
				goto retry_new_pernfsq;
			p = nfsq_table[(p->node + 1) % MIN(nfsq_init, max_nfsq)];
		} while (p != first_p);
		s = mutex_spinlock(&p->nfsq.ifq_lock);
		if (smapQhead || p->nfsq.ifq_len)
			goto retry_new_pernfsq;
		p->looking++;
		if (sv_wait_sig(&p->waiter, PZERO+1, &p->nfsq.ifq_lock, s)) {
			#pragma mips_frequency_hint NEVER
			return;
		}
		justwoke = TRUE;
	}
}

/*
 * This function is called from the udp_input module with a packet for us.
 * at this point it includes the IP and UDP headers. Also called from TCP
 * with the mbuf being NULL, just to do a wakeup.
 */
void
nfs_input(m)
     struct mbuf *m;
{
#ifdef SN0
#define	NODE_NUMBER(cpu) cputocnode(cpu)
#else
#define NODE_NUMBER(cpu) (cpu / 2)
#endif

	int s = cpuid();
	int qlen;
	struct pernfsq *p, *wakep;

	p = nfsq_table[NODE_NUMBER(s) % MIN(nfsq_init, max_nfsq)];

	s = mutex_spinlock(&p->nfsq.ifq_lock);
	if (m) {
		IF_ENQUEUE_NOLOCK(&p->nfsq, m);
	}
	qlen = p->nfsq.ifq_len;
	if (qlen > 1024) {
		/*
		 * If the queue becomes way too large, throw away
		 * the oldest packet, not the new one.
		 */
		#pragma mips_frequency_hint NEVER
		IF_DEQUEUE_NOLOCK(&p->nfsq, m);
		m_freem(m);
	}
	/* if we're all busy, wake up someone else */
	wakep = p;
	while (wakep->looking == 0) {
		if (wakep->node == 0)
			wakep = nfsq_table[MIN(nfsq_init, max_nfsq) - 1];
		else
			wakep = nfsq_table[wakep->node - 1];
		if (wakep == p)
			break;	/* no one else is free */
	}
	if (wakep != p) {
		mutex_spinunlock(&p->nfsq.ifq_lock, s);
		s = mutex_spinlock(&wakep->nfsq.ifq_lock);
	}
	if (wakep->looking && sv_signal(&wakep->waiter))
		wakep->looking--;
	
	mutex_spinunlock(&wakep->nfsq.ifq_lock, s);
}
