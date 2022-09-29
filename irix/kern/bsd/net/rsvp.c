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

#ident "$Revision: 1.40 $"

#include <sys/types.h>
#include <sys/idbgentry.h>
#include <sys/kthread.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/kabi.h>
#include <sys/mbuf.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/atomic_ops.h>
#include <sys/ddi.h>
#include <sys/socketvar.h>
#include <sys/cmn_err.h>
#include <net/if.h>
#include <net/route.h>
#include <values.h>
#include "tc_isps.h"
#include "rsvp.h"
#include "rsvp_qo.h"

/*
 * =====> Packet Scheduling/Device Driver Interface:
 *
 * Device Driver to Packet Scheduler calls.  See bsd/net/rsvp.h.
 *
 * ps_init() - the driver calls this from its init routine.  The driver
 *             passes in a ps_parms argument.  The field bandwidth is the
 *             driver bandwidth in bits/sec.  If the bandwidth changes,
 *             ps_reinit should be called.
 *
 * ps_reinit() - same as ps_init, except ps_reinit can only be called 
 *               after ps_init has been called.  ps_reinit is useful for
 *               changes in link speed and to disable/enable packet scheduling.
 *
 * ps_txq_stat() - update packet scheduler's idea of the driver tx queue 
 *                   length.  This function should be called about every 2ms
 *                   if the driver is 100% busy.  See last paragraph in next
 *                   section for the relationship between update frequency
 *                   and psif_num_batch_pkts.
 *
 * Packet Scheduler to Device Driver calls.  See bsd/net/rsvp.h.
 * 
 * xxx_txfree_func() - a pointer to this function was passed in during ps_init.
 *                     This function should give the current number of free
 *                     slots in the driver tx queue.  The driver should do
 *                     garbage collection of the tx queue if neccessary.
 *
 * xxx_setstate() - a pointer to this function was passed in during ps_init.
 *                  The second argument of 1 indicates packet scheduling has
 *                  been turned on and that the driver should call ps_txq_stat
 *                  periodically.  0 means packet sheduling is off, and that
 *                  ps_txq_stat does not need to be called.
 *
 *
 * =====> Packet Scheduling Implementation Details:
 *
 * When packet scheduling is turned on, ps_output is "inserted" between
 * ip_output and ether_output.  (i.e. ifp->if_output points to ps_output
 * and psif->psif_output points to ether_output.)  This means all packets
 * coming out of ip_output is processed by ps_output first.
 *
 * ps_output first classifies the packet into best effort or controlled load
 * traffic.  If the packet is best effort, it goes on the best effort queue.
 * If the packet is controlled load, ps_output checks the packet for conformance
 * to the flow spec.  If the packet conforms to the flow spec, then it is put on
 * the controlled load queue, if not, it is put on the non-conforming queue.
 * ps_output then calls ps_send_down.
 *
 * ps_send_down takes packets from the packet scheduler's queues and sends
 * them down to the ethernet/fddi driver tx queue.  ps_send_down first tries
 * to almost fill the driver tx queue with controlled load packets.  If
 * there are no more controlled load packets and the tx queue is not too full,
 * it will send down some best effort packets.  Finally, if there are no more
 * best effort packets and the tx queue is not too full, it will send down
 * some non-conforming traffic.  "Not too full" is defined by psif_num_batch_pkts.
 * The idea is to send enough packets down to the driver to keep the line
 * 100% busy, but not too many (best effort packets) so controlled load packets
 * arriving later will experience low delay.
 *
 * The number of packets currently in the driver tx queue is tracked by
 * psif_txqlen.  This variable is synchronized with the driver under 3 conditions:
 * - if ps_send_down was called from ip_output, and psif_txqlen is greater
 *   than or equal to psif_ps_num_batch_pkts, and txqlen did not recently
 *   exceed psif_num_batch_pkts.  The second condition reduces unneccessary
 *   tx queue length update calls to the driver if we have not sent down
 *   many packets recently.  The third condition reduces unneccessary tx
 *   queue length update calls to the driver if the tx queue was recently
 *   full.
 * - if ps_send_down was called from ip_output, and there is an 
 *   explicit update flag, UPDATE_TXQLEN.  This is used by ps_send_down
 *   when the packet scheduling queue is full to make more space in the
 *   queues.  Otherwise, ps_send_down must drop the packet.
 * - if ps_send_down was called by the driver interrupt routine.  Since
 *   driver interrupt routines call the packet scheduler on the completion
 *   of some number of packets, the tx queue length must have changed by
 *   an interesting amount.
 *
 * Driver interrupt calls into the packet scheduler also provides a 
 * regular high frequency (2-3ms) "kick" for the packet scheduler.
 * This kick tells the packet scheduler to send more packets down to
 * the driver tx queue before the tx queue is completely drained,
 * thus keeping the line 100% busy.  For 100Mb ethernet, the interrupt
 * comes after every 16 packets.  For 10Mb ethernet, the interrupt comes 
 * every other packet.  For rns (FDDI), the interrupt comes every 2.048 ms.
 * For xpi and ipg, interrupts are very undependable, coming around every 30+
 * packets or after the board has been idle for 10 to 20ms.
 * If psif_num_batch_pkts is set below those numbers, you will get
 * very low delay, but also lower line utilization.
 *
 *
 * Locking note:  Since all data structures are per ifp, the IFNET lock is
 * used to protect everything.  All calls into the queue object must
 * first get IFNET lock.  Since we are threaded now, no spl's are needed.
 */

struct rsvp_flspec {
	struct rsvp_flspec *rsvpfs_next;
	struct rsvp_flspec *rsvpfs_prev;
	struct psif *rsvpfs_psif;
	unsigned int rsvpfs_handle;
	struct rsvp_filt *rsvpfs_filters;
	isps_fs_t rsvpfs_flspec;
	unsigned int rsvpfs_bktmax;	/* max bucket depth in bytes */
 	int	     rsvpfs_bktcur;	/* current bucket depth in bytes */
	unsigned int rsvpfs_clpkts;	/* cl pkts serviced */
	unsigned int rsvpfs_ncpkts;	/* non-conform pkts detected */
};

/*
 * Filters appear on two lists.  The first list is for the flow they
 * are associated with.  This list is used when filters are added or
 * deleted from a flow which isn't too often so it can stay a linked list.
 * The second list associates the filter with the interface the flow is on. 
 * The second list was added for a bit of efficiency.  This list will be
 * searched a lot (every packet passing through that interface).  Right
 * now it is a linked list but could be changed to a tree or hash table
 * if there end up being a lot of reservations.
 */
struct rsvp_filt {
	struct rsvp_filt *fnext;	/* flow spec filter list */
	struct rsvp_filt *fprev;	/* flow spec filter list */
	struct rsvp_filt *inext;	/* interface filter list (must spl) */
	struct rsvp_filt *iprev;	/* interface filter list (must spl) */
	struct rsvp_flspec *flspec;
	isps_filt_t filter;
};

struct psif {
	struct ifnet *psif_ifp;
	struct psqo *psif_qo;	/* queue object for this interface */
	int (*psif_output)(struct ifnet *, struct mbuf *, struct sockaddr *,
		struct rtentry *);
	int (*psif_txlen)(struct ifnet *);  /* func returns tx free list len */
	void (*psif_setstate)(struct ifnet *, int); 
	struct rsvp_flspec *psif_flspecs;
	struct rsvp_filt *psif_filters;
	uint psif_flags;
	volatile int psif_txqlen; /* current guess of number in tx queue */ 
	int psif_txfree;	/* how deep is tx queue */
	uint psif_clqlen;	/* controlled load packet queue len	*/
	uint psif_lowerfg;	/* flags protected by the lower half lock */
	mutex_t	psif_upperlk;	/* lock for upper half operations	*/
	mutex_t psif_lowerlk;	/* lock for lower half operations	*/
	int psif_num_batch_pkts;/* max number of pkts to send down at once */
	int psif_tick_batch_pkts; /* bigger batch for TICK_INTR drivers */
	int psif_tick_pkts;	/* estimate of how many pkts tx'd per tick,
				   for TICK_INTR drivers */
	int psif_tick_txfree;	/* last txfree during a TICK interrupt */
	uint psif_bwidth;	/* bandwidth of the device */
	uint psif_currsvpbwidth;/* how much bandwidth currently reserved */
	uint psif_maxrsvpbwidth;/* how much bandwidth to give to RSVP */
	char psif_iname[IFNAMSIZ];
};

#define QUEUE_ENABLED	1
#define QUEUE_DISABLED	0
static void ps_enable_q(struct psif *psif);
static void ps_disable_q(struct psif *psif);
static void ps_drop_all_queued(struct psif *psif);

static void	rsvp_init(void);
extern char	*inet_ntoa(struct in_addr);
static int	ps_tick_maint(int refill_buckets);
static int	ps_output(struct ifnet *, struct mbuf *, struct sockaddr *,
	                  struct rtentry *);
static void	ps_send_down(struct psif *, int update_flags);

/* flags for ps_send_down */
#define UPDATE_FROM_IP		0x01
#define UPDATE_FROM_INTR	0x02
#define UPDATE_FROM_TICK	0x04
#define UPDATE_TXQLEN		0x10

#define TICK_LOCK(f,ifp) {		\
	if ((f) & UPDATE_FROM_TICK)	\
		IFNET_UPPERLOCK(ifp);	\
}

#define TICK_UNLOCK(f,ifp) {		\
	if ((f) & UPDATE_FROM_TICK)	\
		IFNET_UPPERUNLOCK(ifp);	\
}

/*
 * Variables from master.d/bsd
 */
extern int ps_enabled;
extern int ps_rsvp_bandwidth;
extern int ps_num_batch_pkts;


/*
 * Variables for the packet scheduler maintenance timer.
 */
static mutex_t	rsvp_mutex;
static int	rsvp_inited=0; 
static int	rsvp_timer_running=0;
static int	rsvp_timer_period=100;		/* in ticks */
static int	rsvp_timer_count=0;
void		rsvp_timer(void);
 

/*
 * psifs is a static array for now.  Should be part of ifnet or etherif.
 */
#define PSIF_MAX	128
static struct psif *psifs[PSIF_MAX];

/* next handle value for newly created flow spec */
static unsigned int rsvpfs_handle_next=1;


static void
dump_filter(struct rsvp_filt *filter)
{
	qprintf("    FILTER: 0x%x\n", filter);
	qprintf("    flspec: 0x%x\n", filter->flspec);
	qprintf("    proto family: %d %d\n", filter->filter.f_pf,
	  filter->filter.f_ipv4_protocol);
	qprintf("    dest %s/%d\n", inet_ntoa(filter->filter.f_ipv4_destaddr),
	  filter->filter.f_ipv4_destport);
	qprintf("    src  %s/%d\n", inet_ntoa(filter->filter.f_ipv4_srcaddr),
	  filter->filter.f_ipv4_srcport);
}

static void
dump_flspec(struct rsvp_flspec *flspec)
{
	struct rsvp_filt *filter;

	qprintf("\n  FLWSPEC: 0x%x\n", flspec);
	qprintf("  psif: 0x%x\thandle: %d\n", flspec->rsvpfs_psif,
	  flspec->rsvpfs_handle);

	qprintf("  Bucket depth: %d (%d)\n",
	  flspec->rsvpfs_bktcur, flspec->rsvpfs_bktmax);

	qprintf("  police %d\n", flspec->rsvpfs_flspec.fs_plc);
	switch (flspec->rsvpfs_flspec.fs_tos) {
	case TOS_CNTRLD_LOAD:
		qprintf("  tos CL\n",
		  flspec->rsvpfs_flspec.fs_tspec.ts_c.c_rate);
		qprintf("  tspec: r: %d   b: %d   m: %d   M: %d\n",
		  flspec->rsvpfs_flspec.fs_tspec.ts_c.c_rate,
		  flspec->rsvpfs_flspec.fs_tspec.ts_c.c_bkt,
		  flspec->rsvpfs_flspec.fs_tspec.ts_c.c_m,
		  flspec->rsvpfs_flspec.fs_tspec.ts_c.c_M);
		break;
	default:
		cmn_err_tag(167,CE_WARN,
			    "BAD tos %d", flspec->rsvpfs_flspec.fs_tos);
	}
	for (filter = flspec->rsvpfs_filters; filter != NULL;
	  filter = filter->fnext)
		dump_filter(filter);
}

static void
dump_psif(struct psif *psif)
{
	struct rsvp_filt *filter;
	struct rsvp_flspec *flspec;

	qprintf("\n\nPSIF: 0x%x   flags 0x%x\n", psif, psif->psif_flags);
	qprintf("ifp: 0x%x  (%s)\n", psif->psif_ifp, psif->psif_iname);
	qprintf("ifp bandwidth %d maxrsvpbandwidth %d currsvpbandwidth %d\n",
	         psif->psif_bwidth,
		 psif->psif_maxrsvpbwidth,
		 psif->psif_currsvpbwidth);
	for (filter = psif->psif_filters; filter != NULL;
	  filter = filter->inext)
		dump_filter(filter);
	flspec = psif->psif_flspecs;
	if (flspec != NULL) {
		do {
			dump_flspec(flspec);
			flspec = flspec->rsvpfs_next;
		} while (flspec != psif->psif_flspecs);
	}
}

void
rsvp_dump(struct psif *psif)
{
	int i;

	if ((long)psif == -1L) {
		for (i = 0; i < PSIF_MAX; i++)
			if (psifs[i])
				dump_psif(psifs[i]);
	} else
		dump_psif(psif);
}


/*
 * Do global initialization stuff.
 */
static void
rsvp_init()
{
	int i;

	for (i=0; i < PSIF_MAX; i++)
		psifs[i] = NULL;

	mutex_init(&rsvp_mutex, MUTEX_DEFAULT, "RSVP");
}


/*
 * Called with psif_upperlk held.
 */
static struct rsvp_flspec *
find_rsvpflspec(struct psif *psif, int handle)
{
    	struct rsvp_flspec *flspec;

	flspec = psif->psif_flspecs;
	if (flspec == NULL)
		return NULL;

	do {
		if (flspec->rsvpfs_handle == handle)
			return (flspec);
		flspec = flspec->rsvpfs_next;
	} while (flspec != psif->psif_flspecs);

	return (NULL);
}

static struct psif *
find_psif(char *name)
{
    	struct psif *psif;
	int i;

	for (i = 0; i < PSIF_MAX; i++)
		if (psif = psifs[i])
			if (strncmp(psif->psif_iname, name, IFNAMSIZ) == 0)
			    return (psif);
	return (NULL);
}

/*
 * Called with psif_upperlk held.
 */
static struct rsvp_filt *
rsvp_find_filt(struct rsvp_flspec *rflspec, isps_filt_t *filt)
{
	struct rsvp_filt *rfilt;

	for (rfilt = rflspec->rsvpfs_filters; rfilt != NULL;
	  rfilt = rfilt->fnext) {
		/*
		 * Since the protocol family of filt was set to PF_UNSPEC
		 * by the rsvp daemon to indicate a delete operation, we
		 * must assume that the protocal family is PF_INET.  Since
		 * we only support PF_INET right now this isn't a problem.
		 */
		if (filt->f_ipv4_protocol == rfilt->filter.f_ipv4_protocol &&
		  filt->f_ipv4_destaddr.s_addr ==
		  rfilt->filter.f_ipv4_destaddr.s_addr &&
		  filt->f_ipv4_destport == rfilt->filter.f_ipv4_destport &&
		  filt->f_ipv4_srcaddr.s_addr ==
		  rfilt->filter.f_ipv4_srcaddr.s_addr &&
		  filt->f_ipv4_srcport == rfilt->filter.f_ipv4_srcport)
			return (rfilt);
	}
	return (NULL);
}

/*
 * Delete one filter from a flow.  Called with psif_upperlk held.
 */
static void
rsvp_del_filter(struct psif *psif, struct rsvp_flspec *rflspec,
  struct rsvp_filt *rfilt)
{

	/* delete the filter from the interface list */
	if (rfilt->iprev == NULL)
		psif->psif_filters = rfilt->inext;
	else
		rfilt->iprev->inext = rfilt->inext;
	if (rfilt->inext != NULL)
		rfilt->inext->iprev = rfilt->iprev;

	/* now delete the filter from the flow list */
	if (rfilt->fprev != NULL)
		rfilt->fprev->fnext = rfilt->fnext;
	else
		rflspec->rsvpfs_filters = rfilt->fnext;
	if (rfilt->fnext != NULL)
		rfilt->fnext->fprev = rfilt->fprev;

	kmem_free(rfilt, sizeof(struct rsvp_filt));
}

/*
 * Delete all the filters of a flow.  Called with psif_upperlk held.
 */
static void
rsvp_del_filters(struct psif *psif, struct rsvp_flspec *rflspec)
{
	struct rsvp_filt *rfilt;

	while ((rfilt = rflspec->rsvpfs_filters) != NULL)
		rsvp_del_filter(psif, rflspec, rfilt);
}

/*
 * Add or delete a filter.  f_pf == PF_UNSPEC means delete.
 */
static int
rsvp_setfilt(struct psif *psif, struct ispsreq  *irp)
{
	isps_filt_t *filt;
	struct rsvp_filt *rfilt;
	struct rsvp_flspec *rflspec;
	int error=0;

	filt = &irp->_iq_iqu._iq_filt;

	/* Only allow filters on UDP and TCP for now */
	if (filt->f_pf != PF_INET)
	    	return (EINVAL);

	if (filt->f_ipv4_protocol != IPPROTO_UDP &&
	  filt->f_ipv4_protocol != IPPROTO_TCP)
	    	return (EINVAL);

	if (filt->f_pf != PF_UNSPEC) {
		rfilt = kmem_alloc(sizeof(struct rsvp_filt), KM_SLEEP);
		if (rfilt == NULL)
			return ENOMEM;
	}

	mutex_lock(&psif->psif_upperlk, PZERO);

	/* find the flow spec */
	if ((rflspec = find_rsvpflspec(psif, irp->iq_handle)) == NULL) {
		error = EINVAL;
		goto setfilt_done;
	}

	/* This means delete the filter. */
	if (filt->f_pf == PF_UNSPEC) {
		rfilt = rsvp_find_filt(rflspec, filt);
		if (rfilt == NULL) {
			error = EBADFILT;
			goto setfilt_done;
		}
		rsvp_del_filter(psif, rflspec, rfilt);

	} else {

		/* add filter */
		rfilt->filter = *filt;
		rfilt->flspec = rflspec;

		/* Add it to the list of filters for this flow */
		rfilt->fnext = rflspec->rsvpfs_filters;
		rfilt->fprev = NULL;
		if (rflspec->rsvpfs_filters != NULL)
			rflspec->rsvpfs_filters->fprev = rfilt;
		rflspec->rsvpfs_filters = rfilt;

		/* Add it to the list of filters for this interface */
		rfilt->inext = psif->psif_filters;
		rfilt->iprev = NULL;
		if (psif->psif_filters != NULL)
			psif->psif_filters->iprev = rfilt;
		psif->psif_filters = rfilt;
	}

setfilt_done:
	mutex_unlock(&psif->psif_upperlk);

	return (error);
}


/*
 * Calculate the actual bandwidth usage of a flow based on the rate
 * and the bucket size.  If the bucket is large, then we expect this flow
 * to use more of the bandwidth than just the rate.  The question is how
 * much more....  again, I'm just going to guess and see how things work.
 */
static int
calc_weighted_rate(int rate, int bkt)
{
	if (rate >= bkt)
		return rate;
	else
		return (rate + ((bkt - rate)/50));
}


/*
 * Set params which are dependent on interface speed.
 * Must be called with psif_upperlk locked.
 */
static void
set_speed_params(struct psif *psif)
{

	if (psif->psif_bwidth == 125000000)
		psif->psif_num_batch_pkts = 45;
	else if (psif->psif_bwidth == 12500000)
                psif->psif_num_batch_pkts = DEFAULT_100MB_BATCH;
	else
		psif->psif_num_batch_pkts = DEFAULT_10MB_BATCH;

	/*
	 * Drivers with infrequent interrupts need a larger number
	 * of packets per batch, else the txq may go empty.
	 */
	if (psif->psif_tick_batch_pkts != 0)
		psif->psif_num_batch_pkts = psif->psif_tick_batch_pkts;

	/* user override */
	if (ps_num_batch_pkts)
		psif->psif_num_batch_pkts = ps_num_batch_pkts;
}


/*
 * This function decides if a new or modified flow can be handled on
 * this interface.  It also has two side effects:
 * - update psif_currsvpbwidth
 * - add more dest structs and update their associated variables
 * Called with psif_upperlk held.
 */
static int
admission_control(
	struct psif *psif,
	isps_fs_t *fs,
	struct rsvp_flspec *orflspec
)
{
	u_int32_t bandwidth;
	int rate, bkt, old_rate, old_bkt;

	switch (fs->fs_tos) {
	case TOS_CNTRLD_LOAD:
		/* max pkt size must be less than mtu */
		if (fs->fs_tspec.ts_c.c_M > psif->psif_ifp->if_mtu)
			return (EINVAL);

		rate = fs->fs_tspec.ts_c.c_rate;
		bkt =  fs->fs_tspec.ts_c.c_bkt;
		bandwidth = psif->psif_currsvpbwidth;

		/*
		 * If we are modifying an existing flow we need to
		 * take into account the resources held by the old flow.
		 */
		if (orflspec != NULL) {
			old_rate = orflspec->rsvpfs_flspec.fs_tspec.ts_c.c_rate;
			old_bkt = orflspec->rsvpfs_flspec.fs_tspec.ts_c.c_bkt;

			bandwidth -= calc_weighted_rate(old_rate, old_bkt);
		}
		bandwidth += calc_weighted_rate(rate, bkt);

		if (bandwidth > psif->psif_maxrsvpbwidth)
			return (ENOBWD);

		/*
		 * We've passed admission control now.  Update variable.
		 */
		psif->psif_currsvpbwidth = bandwidth;
		return (0);

	case TOS_GUARANTEED:
	case TOS_PREDICTIVE:
		/* fall through */
	default:
		return (EINVAL);
	}
}

/*
 * Called with psif_upperlk held
 */
static struct rsvp_flspec *
rsvp_findflow(struct psif *psif, struct ispsreq  *irp)
{
	struct rsvp_flspec *rflspec;

	rflspec = psif->psif_flspecs;
	if (rflspec == NULL)
		return NULL;

	do {
		if (irp->iq_handle == rflspec->rsvpfs_handle)
			return (rflspec);
		rflspec = rflspec->rsvpfs_next;
	} while (rflspec != psif->psif_flspecs);

	return (NULL);
}

static int
rsvp_addflow(struct psif *psif, struct ispsreq  *irp)
{
	struct rsvp_flspec *rflspec;
	int error, enable_q=0;

	/*
	 * Alloc rflspec first because it is easier to free rflspec if
	 * admission control fails than to back out bandwidth reservation.
	 */
	rflspec = kmem_zalloc(sizeof(struct rsvp_flspec), KM_SLEEP);
	if (rflspec == NULL)
		return (ENOMEM);

	mutex_lock(&psif->psif_upperlk, PZERO);

	error = admission_control(psif, &irp->iq_fs, NULL);
	if (error) {
		kmem_free(rflspec, sizeof(struct rsvp_flspec));
		mutex_unlock(&psif->psif_upperlk);
		return (error);
	}

	/* Only controlled load gets by admission control */
	ASSERT(irp->iq_fs.fs_tos == TOS_CNTRLD_LOAD);

	rflspec->rsvpfs_filters = NULL;
	rflspec->rsvpfs_flspec = irp->iq_fs;
	rflspec->rsvpfs_psif = psif;
	rflspec->rsvpfs_handle = rsvpfs_handle_next++;
	rflspec->rsvpfs_bktmax = irp->iq_fs.fs_tspec.ts_c.c_bkt +
		                 irp->iq_fs.fs_tspec.ts_c.c_rate;
	rflspec->rsvpfs_bktcur = rflspec->rsvpfs_bktmax;

	/*
	 * Insert this flowspec onto the psif, and if this is the
	 * first flow, start packet scheduling.
	 */
	if (psif->psif_flspecs == NULL) {
		if (ps_enabled &&
		    (psif->psif_flags & PS_DISABLED) == 0 &&
		    (psif->psif_flags & PSIF_ADMIN_PSOFF) == 0)
			enable_q = 1;
		psif->psif_flspecs = rflspec;
		rflspec->rsvpfs_next = rflspec;
		rflspec->rsvpfs_prev = rflspec;
	} else {
		rflspec->rsvpfs_next = psif->psif_flspecs;
		rflspec->rsvpfs_prev = psif->psif_flspecs->rsvpfs_prev;
		psif->psif_flspecs->rsvpfs_prev->rsvpfs_next = rflspec;
		psif->psif_flspecs->rsvpfs_prev = rflspec;
	}

	mutex_unlock(&psif->psif_upperlk);

	if (enable_q) {
		IFNET_UPPERLOCK(psif->psif_ifp);
		ps_enable_q(psif);
		IFNET_UPPERUNLOCK(psif->psif_ifp);
	}

	irp->iq_handle = rflspec->rsvpfs_handle;
	return (0);
}

/*
 * Remove the flow from the psif.  Also:
 * - update psif_currsvpbwidth
 * - remove dest structs and update their associated variables
 * (this is the inverse of admission_control)
 * Called with psif_upperlk held.
 */
static void
delete_flspec(struct psif *psif, struct rsvp_flspec *rflspec)
{
	int rate, bkt;

	/*
	 * Take the flowspec out of the doubly linked list and
	 * delete all filters associated with the flow.
	 */
	if (psif->psif_flspecs == rflspec &&
	    rflspec->rsvpfs_next == rflspec) {
		psif->psif_flspecs = NULL;
	} else {
		rflspec->rsvpfs_prev->rsvpfs_next = rflspec->rsvpfs_next;
		rflspec->rsvpfs_next->rsvpfs_prev = rflspec->rsvpfs_prev;
		if (psif->psif_flspecs == rflspec)
			psif->psif_flspecs = rflspec->rsvpfs_next;
	}
	rsvp_del_filters(psif, rflspec);

	switch (rflspec->rsvpfs_flspec.fs_tos) {
	case TOS_CNTRLD_LOAD:
		rate = rflspec->rsvpfs_flspec.fs_tspec.ts_c.c_rate;
		bkt  = rflspec->rsvpfs_flspec.fs_tspec.ts_c.c_bkt;
		psif->psif_currsvpbwidth -= calc_weighted_rate(rate, bkt);
		break;

	case TOS_GUARANTEED:
	case TOS_PREDICTIVE:
		/* fall through */
	default:
		cmn_err_tag(168,CE_WARN,
			    "RSVP: (delete_flspec) Invalid TOS %d",
			    rflspec->rsvpfs_flspec.fs_tos);
	}

	/*
	 * There may be some packets belonging to this flow in the 
	 * general cl priority queues.  We just let those packets go out
	 * but since we are deleting the flowspec, we will not accept
	 * any more new packets.
	 */
	kmem_free(rflspec, sizeof(struct rsvp_flspec));
}

static int
rsvp_delflow(struct psif *psif, struct ispsreq  *irp)
{
	struct rsvp_flspec *rflspec;
	int error=0;

	mutex_lock(&psif->psif_upperlk, PZERO);

	rflspec = rsvp_findflow(psif, irp);
	if (rflspec != NULL)
		delete_flspec(psif, rflspec);
	else
		error = EBDHDL;

	mutex_unlock(&psif->psif_upperlk);
	return (error);
}

static int
rsvp_modflow(struct psif *psif, struct ispsreq  *irp)
{
	struct rsvp_flspec *rflspec;
	int error;

	mutex_lock(&psif->psif_upperlk, PZERO);

	rflspec = rsvp_findflow(psif, irp);
	if (rflspec == NULL)
		error = EBDHDL;
	else if (!(error = admission_control(psif, &irp->iq_fs, rflspec)))
		rflspec->rsvpfs_flspec = irp->iq_fs;

	mutex_unlock(&psif->psif_upperlk);
	return (error);
}


/*
 * Called with psif_upperlk held.
 */
static int
rsvp_reset_locked(struct psif *psif)
{
    	struct rsvp_flspec *rflspec;

	while ((rflspec = psif->psif_flspecs) != NULL)
		delete_flspec(psif, rflspec);

	/*
	 * After all the flspecs are deleted, the current reserved
	 * bandwidth should be 0
	 */
#ifdef DEBUG
	if (psif->psif_currsvpbwidth != 0)
		printf("rsvp_reset_locked: all flowspecs deleted, resv bwd %d\n",
		       psif->psif_currsvpbwidth);
#endif
	psif->psif_currsvpbwidth = 0;

	return (0);
}


/*
 * Delete all flows from an interface.  This is used if the user
 * requests it or if packet scheduling has been turned off on the
 * interface.
 */
static int
rsvp_reset(struct psif *psif)
{
	int err;

	mutex_lock(&psif->psif_upperlk, PZERO); 
	err = rsvp_reset_locked(psif);
	mutex_unlock(&psif->psif_upperlk);
	return err;
}

/*
 * Notify the rsvpd that its reservations have been eliminated as a
 * result of a ps_reinit().
 * Must be called with psif_upperlk held.
 */
static void
rsvp_notify(struct psif *psif)
{
	/*
	 * XXX Needs to be written.
	 */
	psif = psif; /* keep compiler quiet until function is implemented */
}

/*
 * Set or return configuration info. on the packet scheduling interface.
 */
int
ps_config_info(struct psif *psif, struct ispsreq *req_p)
{
	psif_config_info_t *config_p;
	unsigned short function;

	function = (unsigned short) req_p->iq_function;
	config_p = (psif_config_info_t *) &req_p->iq_config;

	if (function == IF_GET_CONFIG) {
		config_p->int_bandwidth = psif->psif_bwidth;
		config_p->cur_resv_bandwidth = psif->psif_currsvpbwidth;
		config_p->max_resv_bandwidth = psif->psif_maxrsvpbwidth;
		config_p->num_batch_pkts = psif->psif_num_batch_pkts;
		config_p->mtu = psif->psif_ifp->if_mtu;
		config_p->flags = psif->psif_flags;

		config_p->drv_qlen = psif->psif_txfree;
		config_p->cl_qlen = psif->psif_clqlen;
		config_p->be_qlen = psif->psif_txfree;
		config_p->nc_qlen = NUM_NC_MBUFS;

	} else if (function == IF_SET_CONFIG) {
		mutex_lock(&psif->psif_upperlk, PZERO);

		psif->psif_maxrsvpbwidth = config_p->max_resv_bandwidth;
		psif->psif_num_batch_pkts = config_p->num_batch_pkts;
	        if (config_p->flags & PSIF_ADMIN_PSOFF)
		       psif->psif_flags |= PSIF_ADMIN_PSOFF;
		else
		       psif->psif_flags &= ~PSIF_ADMIN_PSOFF;

		mutex_unlock(&psif->psif_upperlk);

		if ((psif->psif_flags & PSIF_PSON) &&
		    (config_p->flags & PSIF_ADMIN_PSOFF)) {
			rsvp_reset(psif);
			ps_disable_q(psif);
		}
	}

	return 0;
}

/*
 * Return statistics on the packet scheduling interface.
 */
int
ps_get_stats(struct psif *psif, struct ispsreq *req_p)
{
	psif_stats_t *stats_p = (psif_stats_t *) &req_p->iq_stats;

	stats_p->txq_len = psif->psif_txqlen;
	stats_p->beq_len = psif->psif_qo->qo_be.psq_qlen;
	stats_p->clq_len = psif->psif_qo->qo_cl.psq_qlen;
	stats_p->ncq_len = psif->psif_qo->qo_nc.psq_qlen;
	stats_p->be_pkts = psif->psif_qo->qo_be.psq_pkts;
	stats_p->cl_pkts = psif->psif_qo->qo_cl.psq_pkts;
	stats_p->nc_pkts = psif->psif_qo->qo_nc.psq_pkts;

	return 0;
}


/*
 * Return all flows and filters for this psif.  We may not be
 * able to return them all because the caller doesn't know how
 * many we have.
 */
int
ps_get_flow_list(struct psif *psif, struct ispsreq *req_p)
{
	psif_flow_list_t *flow_list_p;
	psif_flow_info_t *flow_info_p;
	isps_filt_t *filt_p;
	isps_fs_t *flow_p;
	struct rsvp_filt *rfilt;
	struct rsvp_flspec *rflspec;
	int user_bufsize, kern_bufsize=0;
	char *user_bufaddr, *kern_bufaddr;
	int abi, kern_flows=0;
	int num_filters, flowsize, filtsize, more_space=1;

	flowsize = sizeof(psif_flow_info_t) + sizeof(isps_fs_t);
	filtsize = sizeof(isps_filt_t);

	abi = get_current_abi();
	if (ABI_IS_IRIX5_64(abi))
		return EINVAL;

	flow_list_p = (psif_flow_list_t *) &req_p->iq_flist;
	user_bufaddr = (char *)((ulong) flow_list_p->buf_addr);
	user_bufsize = flow_list_p->buf_size;

	kern_bufaddr = kmem_zalloc(user_bufsize, KM_SLEEP);
	ASSERT(kern_bufaddr);

	mutex_lock(&psif->psif_upperlk, PZERO);

	rflspec = psif->psif_flspecs;
	if (rflspec == NULL)
		goto list_done;

	flow_info_p = (psif_flow_info_t *) kern_bufaddr;
	do {
		if (kern_bufsize + flowsize <= user_bufsize) {
			num_filters = 0;
			rfilt = rflspec->rsvpfs_filters;
			while (rfilt) {
				rfilt = rfilt->fnext;
				num_filters++;
			}
			flow_info_p->num_filters = num_filters;
			flow_info_p->flow_handle = rflspec->rsvpfs_handle;
			flow_info_p++;

			flow_p = (isps_fs_t *) flow_info_p;
			/* copy the flowspec */
			*flow_p = rflspec->rsvpfs_flspec;
			flow_p++;

			kern_flows++;
			kern_bufsize += flowsize;
		} else {
			more_space = 0;
			break;
		}

		filt_p = (isps_filt_t *) flow_p;
		rfilt = rflspec->rsvpfs_filters;
		while (rfilt != NULL) {
			if (kern_bufsize + filtsize <= user_bufsize) {
				/* copy the filter */
				*filt_p = rfilt->filter;
				filt_p++;
				kern_bufsize += filtsize;
			} else {
				more_space = 0;
				break;
			}
			rfilt = rfilt->fnext;
		}

		flow_info_p = (psif_flow_info_t *) filt_p;
		rflspec = rflspec->rsvpfs_next;

	} while ((rflspec != psif->psif_flspecs) && more_space);

list_done:

	mutex_unlock(&psif->psif_upperlk);

	flow_list_p->num_flows = kern_flows;
	flow_list_p->more_info = more_space ? 0 : 1;

	copyout(kern_bufaddr, user_bufaddr, user_bufsize);
	kmem_free(kern_bufaddr, user_bufsize);
	  
	return 0;
}


/*
 * Return statistics for the requested flow.
 */
int
ps_get_flow_stats(struct psif *psif, struct ispsreq *irp)
{
	psif_flow_stats_t *fstats_p;
	struct rsvp_flspec *rflspec;

	mutex_lock(&psif->psif_upperlk, PZERO);

	rflspec = rsvp_findflow(psif, irp);
	if (rflspec == NULL) {
		mutex_unlock(&psif->psif_upperlk);
		return (EBDHDL);
	}

	fstats_p = (psif_flow_stats_t *) &irp->iq_fstats;

	fstats_p->cl_pkts = rflspec->rsvpfs_clpkts;
	fstats_p->nc_pkts = rflspec->rsvpfs_ncpkts;

	mutex_unlock(&psif->psif_upperlk);

	return 0;
}


/*
 * Callable only if you have the CAP_NETWORK_MGT capability or are root if
 * capabilities are not used (checked in ifioctl).
 */
int
rsvp_ioctl(caddr_t data)
{
	struct ispsreq  *irp;
    	struct psif *psif;
	unsigned short function;

	irp = (struct ispsreq  *)data;
	if ((psif = find_psif(irp->iq_name)) == NULL)
	    return (EINVAL);

	function = (unsigned short) irp->iq_function;

	switch (function) {
	case IF_ADDFLW:
	  	return (rsvp_addflow(psif, irp));
	case IF_DELFLW:
	  	return (rsvp_delflow(psif, irp));
	case IF_MODFLW:
	  	return (rsvp_modflow(psif, irp));
	case IF_SETFILT:
	  	return (rsvp_setfilt(psif, irp));
	case IF_RESET:
	  	return (rsvp_reset(psif));
	case IF_GET_CONFIG:
	case IF_SET_CONFIG:
		return (ps_config_info(psif, irp));
	case IF_GET_STATS:
		return (ps_get_stats(psif, irp));
	case IF_GET_FLOW_LIST:
		return (ps_get_flow_list(psif, irp));
	case IF_GET_FLOW_STATS:
		return (ps_get_flow_stats(psif, irp));
	default:
	      return (EINVAL);
	}
}


/*
 * Do one tick or one second maintenance stuff.
 */
void
rsvp_timer()
{
	int run_timer, refill_buckets=0;

	if (rsvp_timer_period == 1) {
		rsvp_timer_count++;
		if (rsvp_timer_count >= 100) {
			rsvp_timer_count = 0;
			refill_buckets = 1;
		}
	} else {
		refill_buckets = 1;
	}

	run_timer = ps_tick_maint(refill_buckets);


	if (run_timer) {
		itimeout(rsvp_timer, NULL, rsvp_timer_period, plbase);
	} else {
		mutex_lock(&rsvp_mutex, PZERO);
		rsvp_timer_running = 0;
		mutex_unlock(&rsvp_mutex);
	}
}


/*
 * =============================================================================
 *
 * packet scheduling functions
 *
 * =============================================================================
 */

/*
 * Must be called with IFNET_UPPERLOCK held.
 */
static void
ps_enable_q(struct psif *psif)
{
	mutex_lock(&psif->psif_upperlk, PZERO);

	/*
	 * txqlen = 0 isn't strictly correct, since when ps is first enabled,
	 * the lower layer driver may already have a number of packets
	 * queued for tx.  But this inaccuracy will be correct soon enough,
	 * and makes the locking a lot cleaner.
	 */
	psif->psif_txqlen = 0;
	psif->psif_flags |= PSIF_PSON;
	psqo_alloc_queue(psif->psif_qo, PSQO_QID_CL, psif->psif_clqlen);
	psqo_alloc_queue(psif->psif_qo, PSQO_QID_BE, psif->psif_txfree);
	psqo_alloc_queue(psif->psif_qo, PSQO_QID_NON_CONFORM, NUM_NC_MBUFS);
	psif->psif_ifp->if_output = ps_output;

	mutex_unlock(&psif->psif_upperlk);

	(*psif->psif_setstate)(psif->psif_ifp, QUEUE_ENABLED);

	/*
	 * If the timer is not running, start it.
	 */
	mutex_lock(&rsvp_mutex, PZERO);
	if (rsvp_timer_running == 0) {
		itimeout(rsvp_timer, NULL, rsvp_timer_period, plbase);
		rsvp_timer_running = 1;
	}
	mutex_unlock(&rsvp_mutex);
}


/*
 * Must be called with IFNET_UPPERLOCK held.
 */
static void
ps_disable_q(struct psif *psif)
{
	(*psif->psif_setstate)(psif->psif_ifp, QUEUE_DISABLED);

	mutex_lock(&psif->psif_upperlk, PZERO);

	if ((psif->psif_flags & PSIF_PSON) == 0) {
	        mutex_unlock(&psif->psif_upperlk);	
	        return;
	}

	psif->psif_ifp->if_output = psif->psif_output;
	ps_drop_all_queued(psif);
	psqo_free_queue(psif->psif_qo, PSQO_QID_CL);
	psqo_free_queue(psif->psif_qo, PSQO_QID_BE);
	psqo_free_queue(psif->psif_qo, PSQO_QID_NON_CONFORM);
	psif->psif_flags &= ~PSIF_PSON;

	mutex_unlock(&psif->psif_upperlk);
}

/*
 * drop all the mbufs from the queues.  This is called when
 * ps_reinit() was told to disable queueing.  A pretty drastic step....
 * This is called with IFNET_UPPERLOCK held.
 */
static void
ps_drop_all_queued(struct psif *psif)
{
	struct dest *dst;

	while (dst = psqo_get_next_pkt(psif->psif_qo, PSQO_QID_NON_CONFORM)) {
		m_freem(dst->d_m);
		if (dst->d_rte)
			rtfree_needlock(dst->d_rte);
		dst->d_m = NULL;
	}
}

/*
 * Re-initialize an interface.  Must be called after ps_init.  This is
 * used by devices that want to change the parms they passed into ps_init.
 * For example a device that supports multiple bandwidths can call ps_reinit()
 * to notify the packet scheduler that it changed bandwidth.
 * IFNET_UPPERLOCK must be acquired before calling this function.
 */
int
ps_reinit(struct ifnet *ifp, struct ps_parms *parms)
{
	struct psif *psif;
	int bandwidth, old_flags, obwdth, flags_changed=0;

        ASSERT(ifp->if_index < PSIF_MAX);

	psif = psifs[ifp->if_index];
	if (psif == NULL) {
#ifdef DEBUG
		printf("ps_reinit called on uninitialized psif %d\n",
		       ifp->if_index);
#endif
		return 0;
	}

	ASSERT(psif->psif_ifp == ifp);
	ASSERT(IFNET_ISLOCKED(ifp));

	mutex_lock(&psif->psif_upperlk, PZERO);

	/*
	 * Check for changes in device bandwidth
	 */
	obwdth = psif->psif_bwidth;
	bandwidth = parms->bandwidth / BITSPERBYTE;
	if (bandwidth != obwdth) {
		psif->psif_bwidth = bandwidth;
		set_speed_params(psif);
		psif->psif_maxrsvpbwidth = (bandwidth * ps_rsvp_bandwidth)/100;
		if (psif->psif_currsvpbwidth > psif->psif_maxrsvpbwidth) {
			/*
			 * We have a problem.  The bandwidth has been reduced
			 * so low that the reserved bandwidth is more than the
			 * amount available.  Cancel all the reservations and
			 * tell rsvpd about it.
			 */
			rsvp_reset_locked(psif);
			rsvp_notify(psif);
		}
	}

	/*
	 * Nothing special needs to be done with these.
	 */
	psif->psif_txlen = parms->txfree_func;
	psif->psif_setstate = parms->state_func;
	psif->psif_txfree = parms->txfree;

	/*
	 * Check for changes in flags
	 */
	if ((psif->psif_flags & PSIF_EXT_MASK) != (parms->flags & PSIF_EXT_MASK)) {
		old_flags = psif->psif_flags;
		psif->psif_flags = (parms->flags & PSIF_EXT_MASK) |
			           (old_flags & PSIF_INT_MASK);
		flags_changed = 1;
	}

	mutex_unlock(&psif->psif_upperlk);

	if (flags_changed) {
		if ((old_flags & PS_DISABLED) &&
		    (psif->psif_flags & PS_DISABLED) == 0 &&
		    (psif->psif_flags & PSIF_ADMIN_PSOFF) == 0 &&
		    (psif->psif_flspecs != NULL)) {
			/*
			 * We are enabling packet scheduling and have
			 * flows.  Notify the driver that we are now
			 * queueing.
			 */
			ps_enable_q(psif);

		} else if (((old_flags & PS_DISABLED) == 0) &&
			   (psif->psif_flags & PS_DISABLED) &&
			   (psif->psif_flspecs != NULL)) {
			/*
			 * We had packet scheduling enabled but we are
			 * now turning it off.  Delete all the flows, 
			 * which causes all packets on the queues to
			 * get dropped.
			 */
			rsvp_reset(psif);
			ps_disable_q(psif);
		}
	}

	return (0);
}

/*
 * bandwidth is the total banwidth available on the interface.  Must be called
 * after if_attach() since it relies on if_index.  It also uses if_unit so
 * it can't be called until if_unit is initialized.
 */
int
ps_init(struct ifnet *ifp, struct ps_parms *parms)
{
	struct psif *psif;
	int bandwidth;
	char lockname[IFNAMSIZ+8];

	/*
	 * Start the rsvp timer here to initialize ps_checkount.
	 * XXX small race condition if two CPU's are initializing two
	 * interfaces.  Should be OK since I think interface initialization
	 * is still single threaded.
	 */
	if (rsvp_inited == 0) {
		rsvp_inited = 1;
		rsvp_init();
	}

	if (ifp->if_index >= PSIF_MAX)
		return 0;

	if (psifs[ifp->if_index] != NULL) {
#ifdef DEBUG
		printf("ps_init called twice on unit %d\n", ifp->if_index);
#endif
		return 0;
	}

	if ((psif = kmem_zalloc(sizeof(struct psif), KM_SLEEP)) == 0)
		return (0);

	if ((psif->psif_qo = kmem_zalloc(sizeof(struct psqo), KM_SLEEP|KM_CACHEALIGN)) == 0) {
		kmem_free(psif, sizeof(struct psif));
		return 0;
	}

	bandwidth = parms->bandwidth / BITSPERBYTE;
	psif->psif_txfree = parms->txfree;
	psif->psif_txlen = parms->txfree_func;
	psif->psif_setstate = parms->state_func;
	psif->psif_flags = (parms->flags & PSIF_EXT_MASK);

	psif->psif_ifp = ifp;
	psif->psif_output = ifp->if_output;
	psif->psif_bwidth = bandwidth;
	psif->psif_maxrsvpbwidth = (bandwidth * ps_rsvp_bandwidth) / 100; 
	psif->psif_clqlen = (psif->psif_txfree * ps_rsvp_bandwidth) / 100;
	if (psif->psif_clqlen < MIN_CL_MBUFS)
		psif->psif_clqlen = MIN_CL_MBUFS;
	psif->psif_filters = NULL;
	sprintf(psif->psif_iname, "%s%d", ifp->if_name, ifp->if_unit);

	if ((strncmp(ifp->if_name, "xpi", 3) == 0) ||
	    (strncmp(ifp->if_name, "ipg", 3) == 0)) {
		psif->psif_flags |= PSIF_POLL_TXQ;
		psif->psif_tick_batch_pkts = DEFAULT_IPG_XPI_BATCH;
		psif->psif_tick_pkts = IPG_XPI_PKTS_PER_TICK;

	} else if (strncmp(ifp->if_name, "et", 2) == 0) {
		psif->psif_flags |= (PSIF_TICK_INTR|PSIF_POLL_TXQ|PSIF_TXFREE_TRUE);
		rsvp_timer_period = 1;
		psif->psif_tick_batch_pkts = DEFAULT_EP_ET_BATCH;

	} else if (strncmp(ifp->if_name, "ep", 2) == 0) {
		psif->psif_flags |= (PSIF_TICK_INTR|PSIF_POLL_TXQ);
		rsvp_timer_period = 1;
		psif->psif_tick_batch_pkts = DEFAULT_EP_ET_BATCH;
		psif->psif_tick_pkts = EP_PKTS_PER_TICK;
	};

	sprintf(lockname, "%squeuelk", psif->psif_iname);
	psqo_init(psif->psif_qo, lockname);

	sprintf(lockname, "%supperlk", psif->psif_iname);
	mutex_init(&psif->psif_upperlk, MUTEX_DEFAULT, lockname);

	sprintf(lockname, "%slowerlk", psif->psif_iname);
	mutex_init(&psif->psif_lowerlk, MUTEX_DEFAULT, lockname);

	mutex_lock(&psif->psif_upperlk, PZERO);
	set_speed_params(psif);
	mutex_unlock(&psif->psif_upperlk);

	psifs[ifp->if_index] = psif;

	if (showconfig) {
		printf("RSVP Packet Scheduling enabled on: %s\n",
		       psif->psif_iname);
		printf("tx q length %d\n", psif->psif_txfree);
		printf("num_batch_pkts %d\n", psif->psif_num_batch_pkts);
		printf("interface bandwidth %d\n",
		       psif->psif_bwidth * BITSPERBYTE);
		printf("max reservable bandwidth %d\n",
		       psif->psif_maxrsvpbwidth * BITSPERBYTE);
	}

	return (1);
}


/*
 * Called with psif_upperlk held.
 * Return the flowspec for this packet or NULL if the packet was
 * fragmented (non-classifyable), or has no filter (meaning BE).
 * XXX This should do something like a hash lookup rather than a linear
 * search through the filters.
 */
static struct rsvp_flspec *
match_ipfilter(struct psif *psif, struct ip *ip)
{
	struct udphdr *udp;
	ushort sport, dport;
	uint32_t ip_src, ip_dst;
	u_char ip_p;
	struct rsvp_filt *filt;

	/*
	 * TCP and UDP can have reservations so check.  Everything
	 * else is best effort.  Since the port numbers for
	 * TCP and UDP are in the same location we do both cases
	 * together using the udp header.
	 */
	ip_p = ip->ip_p;
	if (ip_p == IPPROTO_TCP || ip_p == IPPROTO_UDP) {
		/*
		 * It is a fragment.  Only the first fragment has the
		 * information we need and we may never see the first fragment
		 * so just stick it in the best effort queue.
		 * Note, this results in out of order transmition if there is
		 * a mixture of fragmented and non-fragmented packets going to
		 * the same destination (that has a reservation).  Since
		 * we rely on the port number to classify the packet there is
		 * nothing I can do.
		 */
		if (ip->ip_off & IP_MF)
			return (NULL);

		ip_src = ip->ip_src.s_addr;
		ip_dst = ip->ip_dst.s_addr;

		udp = (struct udphdr *)((char *)ip + (ip->ip_hl << 2));
		sport = udp->uh_sport;
		dport = udp->uh_dport;
		for (filt = psif->psif_filters; filt != NULL;
		  filt = filt->inext) {
			if (ip_dst != filt->filter.f_ipv4_destaddr.s_addr ||
			  dport != filt->filter.f_ipv4_destport ||
			  ip_p != filt->filter.f_ipv4_protocol)
			  	continue;
			if (filt->filter.f_ipv4_srcaddr.s_addr == INADDR_ANY)
				return (filt->flspec);
			if (ip_src == filt->filter.f_ipv4_srcaddr.s_addr &&
			  sport == filt->filter.f_ipv4_srcport)
				return (filt->flspec);
		}
	}

	/* no reservation.  best effort case */
	return (NULL);
}


/*
 * One tick or one second has passed.  
 * If one tick, adjust the q lengths on the drivers which don't
 * call ps_txq_stat() frequently enough.
 * If one second, refill the buckets of all the flows.
 * Return 0 if all the interfaces have packet scheduling turned off.
 * Otherwise, return 1.
 */
static int
ps_tick_maint(int refill_buckets)
{
	struct psif *psif;
	struct rsvp_flspec *flspec;
	int i, txfree, txqlen, send_down, run_timer=0;

	for (i=0; i < PSIF_MAX; i++) {
		psif = psifs[i];
		if (psif == NULL)
			continue;

		/*
		 * If any interface has pkt sched. turned on, keep
		 * running the timer.
		 * If this interface is not turned on, we don't have
		 * to do anything more.  Note I don't lock psif_upperlk
		 * here, which means pkt sched. could get turned off while
		 * we are executing below.  That should be OK.
		 */
		if (psif->psif_flags & PSIF_PSON)
			run_timer = 1;
		else
			continue;

		/*
		 * If this is a TICK_INTR driver, we may need to help
		 * the packet scheduler along a bit.
		 */
		if (psif->psif_flags & PSIF_TICK_INTR) {
			IFNET_UPPERLOCK(psif->psif_ifp);
			txfree = (*psif->psif_txlen)(psif->psif_ifp);
			IFNET_UPPERUNLOCK(psif->psif_ifp);

			mutex_lock(&psif->psif_lowerlk, PZERO);

			if (psif->psif_flags & PSIF_TXFREE_TRUE) {
				/*
				 * Driver reported qlen is always the
				 * most up-to-date, so use its number.
				 */
				psif->psif_txqlen = psif->psif_txfree -
					                              txfree;
				send_down = 1;

			} else if ((psif->psif_lowerfg & PSIFL_GOT_STAT) == 0) {
				/*
				 * Driver has not updated its own qlen in
				 * the last 10ms.  Use my own estimate.
				 */
				txqlen = psif->psif_txqlen;
				txqlen -= psif->psif_tick_pkts;
				if (txqlen < 0)
					txqlen = 0;
				psif->psif_txqlen = txqlen;
				send_down = 1;
			} else {
				/*
				 * Driver changed its txqlen, so we
				 * don't need to do anything here.
				 */
				send_down = 0;
			}

			psif->psif_tick_txfree = txfree;
			if (psif->psif_lowerfg & PSIFL_TXQ_FULL)
				psif->psif_lowerfg &= ~PSIFL_TXQ_FULL;
			if (psif->psif_lowerfg & PSIFL_GOT_STAT)
				psif->psif_lowerfg &= ~PSIFL_GOT_STAT;

			mutex_unlock(&psif->psif_lowerlk);

			if (send_down)
				ps_send_down(psif, UPDATE_FROM_TICK);
		}

		if (refill_buckets == 0)
			continue;

		mutex_lock(&psif->psif_upperlk, PZERO);

		if ((flspec = psif->psif_flspecs) == NULL) {
			mutex_unlock(&psif->psif_upperlk);
			continue;
		}

		do {
			flspec->rsvpfs_bktcur += flspec->rsvpfs_flspec.fs_tspec.ts_c.c_rate;
			if (flspec->rsvpfs_bktcur > flspec->rsvpfs_bktmax)
				flspec->rsvpfs_bktcur = flspec->rsvpfs_bktmax;
			flspec = flspec->rsvpfs_next;
		} while (flspec != psif->psif_flspecs);
		mutex_unlock(&psif->psif_upperlk);
	}
 
	return run_timer;
}


/*
 * This function assumes it can only be called if ps_init was called
 * for this interface.
 */
static int
ps_output(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst,
	struct rtentry *rte)
{
	struct psif *psif;
	struct ip *ip;
	struct rsvp_flspec *flspec;
	int mlen, error;
	struct mbuf *n;
	int qid;

	psif = psifs[ifp->if_index];

	/*
	 * If there is only the default flowspec and it is empty, disable
	 * rsvp and tell the driver.  We have to do this here rather than
	 * in rsvp_delflow because we can't bypass ps_output until
	 * after the default queue has drained.  This means an extra check
	 * every packet in here but allows us to completely bypass ps_output
	 * when rsvp is not being used.
	 */
	if (psif->psif_flspecs == NULL &&
	    psif->psif_qo->qo_num == 0) {
		ps_disable_q(psif);
		return (psif->psif_output(ifp, m, dst, rte));
	}

	/*
	 * This is the classifier portion:
	 * Look through the filters to see where to queue the packets.
	 * Some types of packets can bypass all packet scheduling, 
	 * but still bump ps_send_down to see if we can send down
	 * some more queued packets.
	 */
	if (dst->sa_family == AF_INET) {
		/*
		 * We want to give some packets priority.  For example
		 * rsvp has state that might time out.  If the rsvp
		 * packets have to fight for bandwidth with the best effort
		 * traffic they might be delayed for too long.  Priority
		 * packets get passed straight to the driver.
		 * XXX is this the right list?  Is this what we want to do?
		 * Maybe this should be handled as a special reservation.
		 */
		 ip = mtod(m, struct ip *);
		 if (ip->ip_p == IPPROTO_RSVP ||
		     ip->ip_p == IPPROTO_ICMP ||
		     ip->ip_p == IPPROTO_IGMP ||
		     ip->ip_p == IPPROTO_EGP) {
			 error = psif->psif_output(ifp, m, dst, rte);
			 ps_send_down(psif, UPDATE_FROM_IP);
			 return error;
		 } else {
			 mutex_lock(&psif->psif_upperlk, PZERO);
			 flspec = match_ipfilter(psif, ip);
			 mutex_unlock(&psif->psif_upperlk);
		 }
	} else {
		/*
		 * arpwhohas() takes this case.
		 * XXX What else can take this case?
		 */
		if (dst->sa_family == AF_UNSPEC) {
			error = psif->psif_output(ifp, m, dst, rte);
			ps_send_down(psif, UPDATE_FROM_IP);
			return error;
		} else {
			flspec = NULL;
		}
	}

	if (flspec == NULL) {
		qid = PSQO_QID_BE;
		goto rsvp_xmit;
	} else {
		qid = PSQO_QID_CL;
	}

	/*
	 * This is the policing section:
	 * Check size here while we are outside of the spl.
	 */
	for (mlen = 0, n = m; n; n = n->m_next)
		mlen += n->m_len;
	/*
	 * Enforce the controlled load service maximum packet size.
	 * Note, big packets go to the default queue which means they
	 * are delivered out of order.  This is allowed by the RFC.
	 * Since the app gave us the value of c_M, it is free to change
	 * c_M or send smaller packets if it doesn't like this behavior.
	 */
	ASSERT(flspec->rsvpfs_flspec.fs_tos == TOS_CNTRLD_LOAD);
	if (mlen > flspec->rsvpfs_flspec.fs_tspec.ts_c.c_M) {
		qid = PSQO_QID_NON_CONFORM;
		flspec->rsvpfs_ncpkts++;
		goto rsvp_xmit;
	}

	/*
	 * Enforce the controlled load service minimum packet size.
	 */
	if (mlen < flspec->rsvpfs_flspec.fs_tspec.ts_c.c_m)
		mlen = flspec->rsvpfs_flspec.fs_tspec.ts_c.c_m;

	/*
	 * If this flow has exceeded its reserved rate, put its
	 * packets on the non-conforming queue.
	 */
	if (flspec->rsvpfs_bktcur < 0) {
		flspec->rsvpfs_ncpkts++;
		qid = PSQO_QID_NON_CONFORM;
	} else {
		flspec->rsvpfs_bktcur -= mlen;
		flspec->rsvpfs_clpkts++;
	}

rsvp_xmit:
	/*
	 * This is the transmit section:
	 * Hold the routing entry so it doesn't get freed
	 * while this packet is stuck in the the packet scheduling queue.
	 * If the queue is full, call ps_send_down to send some packets down
	 * and try again.
	 */
	if (rte)
		RT_HOLD(rte);

	if (psqo_enq_pkt(psif->psif_qo, qid, m, dst, rte)) {
		ps_send_down(psif, UPDATE_FROM_IP|UPDATE_TXQLEN);
		if (psqo_enq_pkt(psif->psif_qo, qid, m, dst, rte)) {
			m_freem(m);
			if (rte)
				rtfree_needlock(rte);
			return (ENOBUFS);
		}
	}
		
	ps_send_down(psif, UPDATE_FROM_IP);

	return 0;
}


/*
 * Called from the driver when the transmit queue length has changed.
 * The driver must acquire IFNET lock (if it uses IFNET locking) 
 * before calling this. This is the old entry point.  We will turn around
 * and call the driver again for the current tx queue length.  A better way
 * is for the driver to tell us what the queue length is with this call.
 * See ps_txq_stat() below.
 */
void
ps_xmit_done(struct ifnet *ifp)
{
	struct psif *psif;
	int txfree;

	psif = psifs[ifp->if_index];
	if ((psif->psif_flags & PSIF_PSON) == 0) {
#ifdef DEBUG
		printf("%s called ps_xmit_done when ps is off\n", psif->psif_iname);
#endif
		return;
	}

	txfree = (*psif->psif_txlen)(psif->psif_ifp);

	mutex_lock(&psif->psif_lowerlk, PZERO);

	if (psif->psif_flags & PSIF_TICK_INTR)
		psif->psif_lowerfg |= PSIFL_GOT_STAT;
	if (psif->psif_lowerfg & PSIFL_TXQ_FULL)
		psif->psif_lowerfg &= ~PSIFL_TXQ_FULL;

	psif->psif_txqlen = psif->psif_txfree - txfree;

	mutex_unlock(&psif->psif_lowerlk);

	ps_send_down(psif, UPDATE_FROM_INTR);
}


void
ps_txq_stat(struct ifnet *ifp, int curr_tx_free)
{
	struct psif *psif;

	psif = psifs[ifp->if_index];
	if ((psif->psif_flags & PSIF_PSON) == 0) {
#ifdef DEBUG
		printf("%s called ps_txq_stat when ps is off\n", psif->psif_iname);
#endif
		return;
	}

	mutex_lock(&psif->psif_lowerlk, PZERO);

	if (psif->psif_flags & PSIF_TICK_INTR)
		psif->psif_lowerfg |= PSIFL_GOT_STAT;
	if (psif->psif_lowerfg & PSIFL_TXQ_FULL)
		psif->psif_lowerfg &= ~PSIFL_TXQ_FULL;

	psif->psif_txqlen = psif->psif_txfree - curr_tx_free;

	mutex_unlock(&psif->psif_lowerlk);

	ps_send_down(psif, UPDATE_FROM_INTR);
}


/*
 * A packet has come in from the top or an interrupt was
 * received by the lower level driver.  Try to send down more packets.
 * Except for ps_tick_maint, IFNET_UPPERLOCK must be acquired before
 * calling this function.
 */
static void
ps_send_down(struct psif *psif, int update_flags)
{
	struct dest *dest_p;
	struct psqo *psqo_p = psif->psif_qo;
	int orig_txqlen, txqlen, txfree, num_batch_pkts;

	mutex_lock(&psif->psif_lowerlk, PZERO);
	if (psif->psif_lowerfg & PSIFL_IN_SEND) {
		mutex_unlock(&psif->psif_lowerlk);
		return;
	}
	psif->psif_lowerfg |= PSIFL_IN_SEND;
	mutex_unlock(&psif->psif_lowerlk);

	/*
	 * Update only when our queue state is longer than num_batch_pkts
	 * and we have not seen a queue full condition before.  This prevents
	 * us from updating constantly when we already know the queue is
	 * full.
	 */
	if ((update_flags & UPDATE_FROM_IP) &&
	    (psif->psif_txqlen >= psif->psif_num_batch_pkts) &&
	    ((psif->psif_lowerfg & PSIFL_TXQ_FULL) == 0 ||
	          update_flags & UPDATE_TXQLEN ||
	          psif->psif_flags & PSIF_POLL_TXQ)) {

		txfree = (*psif->psif_txlen)(psif->psif_ifp);

		mutex_lock(&psif->psif_lowerlk, PZERO);
		psif->psif_txqlen = psif->psif_txfree - txfree;
		if (psif->psif_txqlen >= psif->psif_num_batch_pkts)
			psif->psif_lowerfg |= PSIFL_TXQ_FULL;
		else if (psif->psif_lowerfg & PSIFL_TXQ_FULL)
			psif->psif_lowerfg &= ~PSIFL_TXQ_FULL;
		if ((psif->psif_flags & PSIF_TICK_INTR) &&
		    (psif->psif_tick_txfree != txfree))
			psif->psif_lowerfg |= PSIFL_GOT_STAT;
		mutex_unlock(&psif->psif_lowerlk);
	}

	txfree = psif->psif_txfree;
	num_batch_pkts = psif->psif_num_batch_pkts;

	/*
	 * Send down as many controlled load packets as possible.
	 * Don't fill the txqueue completely, though, in case there are
	 * ICMP or other network control messages that needs to be sent.
	 * So leave MIN_TXFREE slots in the txfree queue.
	 */
restart_send_down:
	orig_txqlen = psif->psif_txqlen;
	txqlen = orig_txqlen;
	while (txqlen < txfree - MIN_TXFREE) {
		dest_p = psqo_get_next_pkt(psqo_p, PSQO_QID_CL);
		if (dest_p == NULL)
			break;
		TICK_LOCK(update_flags, psif->psif_ifp);
		psif->psif_output(psif->psif_ifp, dest_p->d_m,
				       &dest_p->d_addr, dest_p->d_rte);
		TICK_UNLOCK(update_flags, psif->psif_ifp);
		if (dest_p->d_rte)
			rtfree_needlock(dest_p->d_rte);
		dest_p->d_m = NULL;
		txqlen++;
		if (orig_txqlen != psif->psif_txqlen) {
			orig_txqlen = psif->psif_txqlen;
			txqlen = orig_txqlen;
		}
	}

	/*
	 * Then, send the BE and non-conform packets up to a total of
	 * ps_num_batch_pkts.  I don't want to send too many BE packets
	 * out at once, since that increases the delay of later CL packets.
	 */
	while (txqlen < num_batch_pkts) {
		dest_p = psqo_get_next_pkt(psqo_p, PSQO_QID_NON_CONFORM);
		if (dest_p == NULL)
			break;
		TICK_LOCK(update_flags, psif->psif_ifp);
		psif->psif_output(psif->psif_ifp, dest_p->d_m,
				       &dest_p->d_addr, dest_p->d_rte);
		TICK_UNLOCK(update_flags, psif->psif_ifp);
		if (dest_p->d_rte)
			rtfree_needlock(dest_p->d_rte);
		dest_p->d_m = NULL;
		txqlen++;
		if (orig_txqlen != psif->psif_txqlen)
			break;
	}

	mutex_lock(&psif->psif_lowerlk, PZERO);
	if (orig_txqlen != psif->psif_txqlen) {
		mutex_unlock(&psif->psif_lowerlk);
		goto restart_send_down;
	}
	psif->psif_txqlen = txqlen;
	psif->psif_lowerfg &= ~PSIFL_IN_SEND;
	mutex_unlock(&psif->psif_lowerlk);
}


