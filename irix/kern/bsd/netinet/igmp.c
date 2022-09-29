/*
 * Internet Group Management Protocol (IGMP) routines.
 *
 * Written by Steve Deering, Stanford, May 1988.
 * Modified by Rosen Sharma, Stanford, Aug 1994.
 * Modified by Bill Fenner, Xerox PARC, Feb 1995.
 *
 * MULTICAST 3.3.1.2
 */

#include <sys/param.h>
#include <sys/sema.h>
#include <sys/hashing.h>

#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include "sys/socketvar.h"
#include "sys/tcpipstats.h"
#include "sys/mac_label.h"
#include "sys/systm.h"
#include "sys/sesmgr.h"

#include "net/if.h"
#include "net/route.h"
#include "net/raw.h"

#include "netinet/in.h"
#include "in_var.h"
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include "igmp.h"
#include "igmp_var.h"
#include <sys/sema.h>
#include "ip_mroute.h"
#include "tcp-param.h"

/*
 * Forward referenced procedures
 */
int igmp_update_host_membership(struct in_addr addr, struct ifnet *ifp);
int igmp_update_host_new_membership(struct in_addr addr, struct ifnet *ifp);
void ip_mroute_slowtimo(void);	/* need to stub from ip_mroute.o */
void ip_mroute_init(void);

static void igmp_sendpkt(struct in_multi *, int type);

/*
 * External Procedures
 */
extern struct hashinfo hashinfo_inaddr;
extern int in_multi_match();

/*
 * External data structures
 */
extern struct ifnet loif;

/*
 * Module global data structures
 */
static struct sockproto   igmpproto = { AF_INET, IPPROTO_IGMP };
static struct sockaddr_in igmpsrc   = { AF_INET };
static struct sockaddr_in igmpdst   = { AF_INET };

int igmp_timers_are_running;
struct router_info *igmp_router_info_head = 0;

#define igmp_all_hosts_group INADDR_ALLHOSTS_GROUP
#define igmp_local_group 0xe0000000  /* 224.0.0.0 */
#define igmp_local_group_mask 0xffffff00

/*
 * Queue of timer-generated packets, and flag for them.
 */
static struct mbuf *igmpq_head;
static struct mbuf *igmpq_tail;

#define IGMP_QFLAG (1 << 16) /* anything bigger than 8 bits */

/*
 * The igmp_lock is used to protect the router_info list AND to serialize
 * the updating of the in_multi struture found via the hash lookup.
 * On uni-processor machines the lock is at splnet to inhibit the software
 * interrupts from happening during this critical section.
 */
mutex_t	igmp_lock;
#define IGMP_LOCKINIT()	mutex_init(&igmp_lock, MUTEX_DEFAULT, "igmplk");
#define IGMP_LOCK()	mutex_lock(&igmp_lock, PZERO-1)
#define IGMP_UNLOCK()	mutex_unlock(&igmp_lock)

void
igmp_init()
{
	igmp_timers_are_running = 0;
	igmp_router_info_head = (struct router_info *) 0;
	/* the random function is initialized earlier */
	IGMP_LOCKINIT();
	ip_mroute_init();
	return;
}

/*
 * Search for a router info structure which matches the interface address
 * in the in_multi structure. If one is found then fill in the router_info
 * address in the in_multi structure and return the membership report type.
 * If one isn't found allocate one and initialize it with the information
 * from the in_multi structure.
 * NOTE: This procedure assumes the global igmp lock is held on entry.
 */
int
fill_rti(struct in_multi *inm)
{
#ifdef LATER
	register struct router_info *rti;
	int type;

	rti = (struct router_info *)inm->inm_ifp->rti;
	if (rti) {
		/* save address of rti in the in_multi struct */
		inm->inm_rti = rti;

		type = (rti->type == IGMP_OLD_ROUTER) 
				? IGMP_HOST_MEMBERSHIP_REPORT
				: IGMP_HOST_NEW_MEMBERSHIP_REPORT;
		return type;
	}
	rti = (struct router_info *)mcb_get(M_WAIT,
				sizeof(struct router_info), MT_MCAST_RINFO);

	inm->inm_ifp->rti = (caddr_t)rti;
	rti->ifp = inm->inm_ifp;
	rti->type = IGMP_NEW_ROUTER;
	rti->time = IGMP_AGE_THRESHOLD;
	inm->inm_rti = rti;
#else
	register struct router_info *rti = igmp_router_info_head;
	int type;

	while (rti) {
		if (rti->ifp == inm->inm_ifp) {

			/* save address of rti in in_multi struct */
			inm->inm_rti = rti;

			type = (rti->type == IGMP_OLD_ROUTER) 
				? IGMP_HOST_MEMBERSHIP_REPORT
				: IGMP_HOST_NEW_MEMBERSHIP_REPORT;
			return type;
		}
		rti = rti->next;
	}
	rti = (struct router_info *)mcb_get(M_WAIT,
				sizeof(struct router_info), MT_MCAST_RINFO);

	rti->ifp = inm->inm_ifp;
	rti->type = IGMP_NEW_ROUTER;
	rti->time = IGMP_AGE_THRESHOLD;

	rti->next = igmp_router_info_head;
	igmp_router_info_head = rti;	
	inm->inm_rti = rti;

#endif /* LATER */
	return IGMP_HOST_NEW_MEMBERSHIP_REPORT;
}

/*
 * Search for a router info structure which matches the interface address
 * supplied. If one is found then return it's address, if not found then
 * found allocate one and link it onto the local list of router info
 * structures. Then initialize the structure using the interface
 * address parameter supplied and the default information for a new router.
 *
 * NOTE: This procedure assumes the global igmp lock is held on entry.
 * This procedure can NOT block waiting for memory allocation if a router_info\
 * structure can't be allocated because we are called from the input packet
 * processsing thread. We return NULL on failure.
 */
struct router_info *
find_rti(struct ifnet *ifp)
{
#ifdef LATER
        register struct router_info *rti = (struct router_info *)(ifp->rti);

	if (rti == 0) { /* allocate a router info for this interface */

		rti = (struct router_info *)mcb_get(M_DONTWAIT,
				sizeof(struct router_info), MT_MCAST_RINFO);
		if (rti) {
			rti->ifp = ifp;
		        rti->type = IGMP_NEW_ROUTER;
		        rti->time = IGMP_AGE_THRESHOLD;
		}
	}
        return rti;
#else
        register struct router_info *rti = igmp_router_info_head;

        while (rti) {
                if (rti->ifp == ifp) {
                        return rti;
                }
                rti = rti->next;
        }
	/*
	 * Allocate a node to hold the router info structure
	 */
	rti = (struct router_info *)mcb_get(M_DONTWAIT,
				sizeof(struct router_info), MT_MCAST_RINFO);
	if (rti) {
		rti->ifp = ifp;
	        rti->type = IGMP_NEW_ROUTER;
	        rti->time = IGMP_AGE_THRESHOLD;

	        rti->next = igmp_router_info_head;
	        igmp_router_info_head = rti;
	}
        return rti;
#endif /* LATER */
}

#ifdef LATER
/*
 * Search for a router info structure which matches the interface address
 * in the in_multi structure. If one is found then return it's address
 * AFTER removing it from the global list. If none is found we return null.
 */
struct router_info *
remove_rti(struct in_multi *inm)
{
	register struct router_info *rti, *rti_last;

	IGMP_LOCK();
	rti = rti_last = igmp_router_info_head;

	while (rti) {

		if (rti->ifp == inm->inm_ifp) { /* found */

			if (rti == igmp_router_info_head) {
				/* remove from head of list */
				igmp_router_info_head = rti->next;
			} else {
				rti_last->next = rti->next;
			}
			IGMP_UNLOCK();
			return rti;
		}
		rti_last = rti;
		rti = rti->next;
	}
	IGMP_UNLOCK();
        return rti;
}
#endif /* LATER */

/*
 * igmp_checktimer_enummatch(struct in_multi *inm, struct in_addr *key
 *	struct ifnet *arg1, caddr_t arg2);
 *
 * NOTE: This procedure is called with only hash bucket entries of the
 * specified type on the enumerate call. Here that is HTF_MULTICAST types.
 */
/* ARGSUSED */
static int
igmp_checktimer_enummatch(struct hashbucket *h, caddr_t key,
	caddr_t arg1, caddr_t arg2)
{
	struct in_multi *inm = (struct in_multi *)h;
	struct ifnet *ifp = (struct ifnet *)arg1;

	if (inm->inm_ifp == ifp && inm->inm_timer == 0 &&
	   (inm->inm_addr.s_addr & igmp_local_group_mask) != igmp_local_group){

	    	inm->inm_state = IGMP_DELAYING_MEMBER;
	    	inm->inm_timer =
		    IGMP_RANDOM_DELAY(IGMP_MAX_HOST_REPORT_DELAY * PR_FASTHZ);
		igmp_timers_are_running = 1;
        }
	/* cause all entries to be enumerated */
	return 0;
}

/*
 * igmp_checkstate_enummatch(struct in_multi *inm, struct ip *ip,
 *	struct ifnet *ifp, struct igmp *igmp)
 *
 * NOTE: This procedure is called with only hash bucket entries of the
 * specified type on the enumerate call. Here that is HTF_MULTICAST types.
 */
static int
igmp_checkstate_enummatch(struct hashbucket *h, caddr_t key,
	caddr_t arg1, caddr_t arg2)
{
	struct in_multi *inm = (struct in_multi *)h;
	struct ip *ip = (struct ip *)key;
	struct ifnet *ifp = (struct ifnet *)arg1;
	struct igmp *igmp = (struct igmp *)arg2;
	int timer; /** timer value in the igmp query header **/

	if (inm->inm_ifp == ifp &&
	   (inm->inm_addr.s_addr & igmp_local_group_mask)!= igmp_local_group &&
	   (ip->ip_dst.s_addr == igmp_all_hosts_group ||
	   (ip->ip_dst.s_addr == inm->inm_addr.s_addr))) {

		timer = igmp->igmp_code * PR_FASTHZ / IGMP_TIMER_SCALE;

		switch (inm->inm_state) {

		case IGMP_IDLE_MEMBER:
		case IGMP_LAZY_MEMBER:
		case IGMP_AWAKENING_MEMBER:
			inm->inm_timer = IGMP_RANDOM_DELAY(timer);
			igmp_timers_are_running = 1;
			inm->inm_state = IGMP_DELAYING_MEMBER;
			break;

		case IGMP_DELAYING_MEMBER:
			if (inm->inm_timer > timer) {
				inm->inm_timer = IGMP_RANDOM_DELAY(timer);
				igmp_timers_are_running = 1;
				inm->inm_state = IGMP_DELAYING_MEMBER;
			}
			break;

		case IGMP_SLEEPING_MEMBER:
			inm->inm_state = IGMP_AWAKENING_MEMBER;
			break;
		}
	}
	/* return zero so all multicast entries will be enumerated */
	return 0;
}

/*
 * igmp_fasttimer_enummatch(struct in_multi *inm, struct in_addr *key
 *	struct ifnet *ifp, struct igmp *igmp)
 *
 * NOTE: This procedure is called with only hash bucket entries of the
 * specified type on the enumerate call. Here that is HTF_MULTICAST types.
 */
/* ARGSUSED */
int
igmp_fasttimer_enummatch(struct hashbucket *h, caddr_t key,
	caddr_t arg1, caddr_t arg2)
{
	struct in_multi *inm = (struct in_multi *)h;
	int type;

	if (inm->inm_ifp == 0 || inm->inm_timer == 0) {
		/*
		 * nothing to do for this entry but enumerate all
		 * multicast entries by returning zero
		 */
		return 0;
	}
	if (--inm->inm_timer == 0) {
		if (inm->inm_state == IGMP_DELAYING_MEMBER) {
			type = (inm->inm_rti->type == IGMP_OLD_ROUTER)
				? IGMP_HOST_MEMBERSHIP_REPORT
				: IGMP_HOST_NEW_MEMBERSHIP_REPORT;

			igmp_sendpkt(inm, type | IGMP_QFLAG);
			inm->inm_state = IGMP_IDLE_MEMBER;
		}
	} else {
		igmp_timers_are_running = 1;
	}
	return 0; /* keep enumeration going */
}

/*ARGSUSED*/
void
#ifdef INET6
igmp_input(struct mbuf *m, struct ifnet *ifp, struct ipsec *ipsec,
  struct mbuf *unused)
#else
igmp_input(struct mbuf *m, struct ifnet *ifp, struct ipsec *ipsec)
#endif
{
	register struct igmp *igmp;
	register struct ip *ip;
	register int igmplen;
	register int iphlen;
	register int minlen;
	struct in_ifaddr *ia;

	struct router_info *rti;

	IGMPSTAT(igps_rcv_total);

	/* Trix: no policy enforced on igmp traffic */
	_SESMGR_SOATTR_FREE(ipsec);

	ip = mtod(m, struct ip *);
	iphlen = ip->ip_hl << 2;
	igmplen = ip->ip_len;

	/*
	 * Validate lengths
	 */
	if (igmplen < IGMP_MINLEN) {
		IGMPSTAT(igps_rcv_tooshort);
		m_freem(m);
		return;
	}
	minlen = iphlen + IGMP_MINLEN;
	if ((m->m_off > MMAXOFF || m->m_len < minlen) &&
	    (m = m_pullup(m, minlen)) == 0) {
		IGMPSTAT(igps_rcv_tooshort);
		return;
	}

	/*
	 * Validate checksum
	 */
	m->m_off += iphlen;
	m->m_len -= iphlen;

	igmp = mtod(m, struct igmp *);

	if (in_cksum(m, igmplen)) {
		IGMPSTAT(igps_rcv_badsum);
		m_freem(m);
		return;
	}
	m->m_off -= iphlen;
	m->m_len += iphlen;
	ip = mtod(m, struct ip *);

	switch (igmp->igmp_type) {

	case IGMP_HOST_MEMBERSHIP_QUERY:
		IGMPSTAT(igps_rcv_queries);

		if (ifp == &loif)
			break;

		if (igmp->igmp_code == 0) {
#ifdef LATER
		    rti = find_rti(ifp);

		    if (rti == 0) { /* failed allocating router_info struct */
			IGMPSTAT(igps_rcv_badsum);
			m_freem(m);
			return;
		    }
		    rti->type = IGMP_OLD_ROUTER;
		    rti->time = 0;
#else
		    IGMP_LOCK();
		    rti = find_rti(ifp);

		    if (rti == 0) { /* failed allocating router_info struct */
			IGMP_UNLOCK();
			IGMPSTAT(igps_rcv_badsum);
			m_freem(m);
			return;
		    }

		    rti->type = IGMP_OLD_ROUTER;
		    rti->time = 0;
		    IGMP_UNLOCK();
#endif /* LATER */

		    /* 
		    ** Do exactly as RFC 1112 says
		    */
		    if (ip->ip_dst.s_addr != igmp_all_hosts_group) {
			IGMPSTAT(igps_rcv_badqueries);
			m_freem(m);
			return;
		    }

		    /*
		     * Start the timers in all of our membership records for
		     * the interface on which the query arrived, except those
		     * "local" group (224.0.0.X).
		     */
		     (void) hash_enum(&hashinfo_inaddr,
				igmp_checktimer_enummatch, HTF_MULTICAST,
				(caddr_t)ip, (caddr_t)ifp, (caddr_t)igmp);

		} /* end of if */
		else {
		    /*
		    ** New Router
		    */
		    if (!IN_MULTICAST(ip->ip_dst.s_addr)) {
			    IGMPSTAT(igps_rcv_badqueries);
			    IGMP_UNLOCK();
			    m_freem(m);
			    return;
		    }

		    /*
		     * - Start the timers in all of our membership records
		     *   that the query applies to for the interface on
		     *   which the query arrived excl. those that belong
		     *   to a "local" group (224.0.0.X)
		     * - For timers already running check if they need to
		     *   be reset.
		     * - Use the igmp->igmp_code field as the maximum 
		     *   delay possible
		     */
		    (void) hash_enum(&hashinfo_inaddr,
				igmp_checkstate_enummatch, HTF_MULTICAST,
				(caddr_t)ip, (caddr_t)ifp, (caddr_t)igmp);
		} /* end of else */
		break;

	      case IGMP_HOST_MEMBERSHIP_REPORT:
		/*
		 *  an old report
		 */
		IGMPSTAT(igps_rcv_reports);
		
		if (ifp == &loif)
			break;

		if (!IN_MULTICAST(ntohl(igmp->igmp_group.s_addr)) ||
			 igmp->igmp_group.s_addr != ip->ip_dst.s_addr) {
			IGMPSTAT(igps_rcv_badreports);
			m_freem(m);
			return;
		}

		/*
		 * KLUDGE: if the IP source address of the report has an
		 * unspecified (i.e., zero) subnet number, as is allowed for
		 * a booting host, replace it with the correct subnet number
		 * so that a process-level multicast routing demon can
		 * determine which subnet it arrived from.  This is necessary
		 * to compensate for the lack of any way for a process to
		 * determine the arrival interface of an incoming packet.
		 */
		if ((ntohl(ip->ip_src.s_addr) & IN_CLASSA_NET) == 0) {
			IFP_TO_IA(ifp, ia);
			if (ia) ip->ip_src.s_addr = htonl(ia->ia_subnet);
		}

		/*
		 * If we belong to the group being reported, stop
		 * our timer for that group.
		 */
		igmp_update_host_membership(igmp->igmp_group, ifp);
		break;

	      case IGMP_HOST_NEW_MEMBERSHIP_REPORT:
		/*
		 * We can get confused and think there's someone else out
		 * there if we are a multicast router. For fast leave to
		 * work, we have to know that we are the only member.
		 */
		IFP_TO_IA(ifp, ia);
		if (ia && ip->ip_src.s_addr == IA_SIN(ia)->sin_addr.s_addr)
			break;
		/*
		 * a new report
		 */
		IGMPSTAT(igps_rcv_reports);
    
		if (ifp == &loif)
			break;
		
		if (!IN_MULTICAST(ntohl(igmp->igmp_group.s_addr)) ||
		    igmp->igmp_group.s_addr != ip->ip_dst.s_addr) {

			IGMPSTAT(igps_rcv_badreports);
			m_freem(m);
			return;
		}
		
		/*
		 * KLUDGE: if the IP source address of the report has an
		 * unspecified (i.e., zero) subnet number, as is allowed for
		 * a booting host, replace it with the correct subnet number
		 * so that a process-level multicast routing demon can
		 * determine which subnet it arrived from.  This is necessary
		 * to compensate for the lack of any way for a process to
		 * determine the arrival interface of an incoming packet.
		 */
		if ((ntohl(ip->ip_src.s_addr) & IN_CLASSA_NET) == 0) {
			IFP_TO_IA(ifp, ia);
			if (ia) ip->ip_src.s_addr = htonl(ia->ia_subnet);
		}
		igmp_update_host_new_membership(igmp->igmp_group, ifp);
	}
		
	/*
	 * Pass all valid IGMP packets up to any process(es) listening
	 * on a raw IGMP socket.
	 */
	igmpsrc.sin_addr = ip->ip_src;
	igmpdst.sin_addr = ip->ip_dst;

	raw_input(m, &igmpproto,
		  (struct sockaddr *)&igmpsrc,
		  (struct sockaddr *)&igmpdst,
		  ifp);

	return;
}


/*
 * This procedure is called from the in coded to send the proper igmp
 * packet to notify that we've joined the group and to allocate the
 * the router info structure which is pointed to by the in_multi structure.
 * Hence we ASSUNME that the thread can block waiting for memory allocation
 * for the area if reuqired.
 * NOTE: This assumption is NOT true for the 'find_rti' procedure which
 * gets called from the input packet processsing thread.
 */
void
igmp_joingroup(struct in_multi *inm)
{

	IGMP_LOCK();
	inm->inm_state = IGMP_IDLE_MEMBER;

	if ((inm->inm_addr.s_addr & igmp_local_group_mask) == igmp_local_group
	    || inm->inm_ifp == &loif) {
		inm->inm_timer = 0;
	} else {
		igmp_sendpkt(inm, fill_rti(inm));

		inm->inm_timer =
		     IGMP_RANDOM_DELAY(IGMP_MAX_HOST_REPORT_DELAY*PR_FASTHZ);
		inm->inm_state = IGMP_DELAYING_MEMBER;
		igmp_timers_are_running = 1;
	}
	IGMP_UNLOCK();
	return;
}

void
igmp_leavegroup(struct in_multi *inm)
{
#ifdef LATER
	struct router_info *rti;
#endif
	switch (inm->inm_state) {

	case IGMP_DELAYING_MEMBER:
	case IGMP_IDLE_MEMBER:
		if ((inm->inm_addr.s_addr & igmp_local_group_mask) !=
		     igmp_local_group && inm->inm_ifp != &loif) {

			if (inm->inm_rti->type != IGMP_OLD_ROUTER) {
				igmp_sendpkt(inm, IGMP_HOST_LEAVE_MESSAGE);
			}
		}
		break;

	case IGMP_LAZY_MEMBER:
	case IGMP_AWAKENING_MEMBER:
	case IGMP_SLEEPING_MEMBER:
		break;
	}

#ifdef LATER
	/*
	 * Can't remove rti structure until last multi group is deleteed
	 * and this will require reference counting the rti entries
	 * so this will have to wait until this feature is required
	 * say to reclaim memory or if the list gets too long.
	 */
	rti = remove_rti(inm);
	if (rti) {
		mcb_free(rti, sizeof(struct router_info), MT_DATA);
	}
#endif /* LATER */

	return;
}

void
igmp_fasttimo()
{
	struct mbuf *m, *mnext, *mopts;

	/*
	 * Quick check to see if any work needs to be done, in order
	 * to minimize the overhead of fasttimo processing.
	 */
	if (!igmp_timers_are_running)
		return;

	IGMP_LOCK();
	igmp_timers_are_running = 0;
	IGMP_UNLOCK();

	igmpq_head = NULL;
	igmpq_tail = NULL;
	(void)hash_enum(&hashinfo_inaddr, igmp_fasttimer_enummatch,
		HTF_MULTICAST, (caddr_t)0, (caddr_t)0, (caddr_t)0);
	for (m = igmpq_head; m; m = mnext) {
		mnext = m->m_act;
		mopts = m;
		m = m->m_next;
		mopts->m_next = 0;
		ip_output(m, (struct mbuf *)0, (struct route *)0,
	 		IP_MULTICASTOPTS, mopts, NULL);
		m_free(mopts);
		
	}
	return;
}

void
igmp_slowtimo()
{
	register struct router_info *rti;

	IGMP_LOCK();
	rti = igmp_router_info_head;

	while (rti) {
		rti->time ++;
		if (rti->time >= IGMP_AGE_THRESHOLD){
			rti->type = IGMP_NEW_ROUTER;
			rti->time = IGMP_AGE_THRESHOLD;
		}
		rti = rti->next;
	}
	IGMP_UNLOCK();

	ip_mroute_slowtimo();
	return;
}

static void
igmp_sendpkt(struct in_multi *inm, int type)
{
        struct mbuf *m;
        struct igmp *igmp;
        struct ip *ip;
        struct mbuf *mopts;
        struct ip_moptions *imo;
        extern struct socket *ip_mrouter;

        MGET(m, M_DONTWAIT, MT_HEADER);
        if (m == NULL)
                return;

        MGET(mopts, M_DONTWAIT, MT_IPMOPTS);
        if (mopts == NULL) {
                m_free(m);
                return;
        }
        m->m_off = roundup(MMINOFF, 8) + 16 + sizeof(struct ip);
        m->m_len = IGMP_MINLEN;

        igmp = mtod(m, struct igmp *);
        igmp->igmp_type   = type & ~IGMP_QFLAG;
        igmp->igmp_code   = 0;
        igmp->igmp_group  = inm->inm_addr;
        igmp->igmp_cksum  = 0;
        igmp->igmp_cksum  = in_cksum(m, IGMP_MINLEN);

        m->m_off -= sizeof(struct ip);
        m->m_len += sizeof(struct ip);

        ip = mtod(m, struct ip *);
        ip->ip_tos        = 0;
        ip->ip_len        = sizeof(struct ip) + IGMP_MINLEN;
        ip->ip_off        = 0;
        ip->ip_p          = IPPROTO_IGMP;
        ip->ip_src	  = IA_SIN(inm->inm_ifp->in_ifaddr)->sin_addr;
        ip->ip_dst        = igmp->igmp_group;

        imo = mtod(mopts, struct ip_moptions *);
        imo->imo_multicast_ifp  = inm->inm_ifp;
        imo->imo_multicast_ttl  = 1;
#ifdef RSVP_ISI
	imo->imo_multicast_vif	= -1;
#endif
        /*
         * Request loopback of the report if we are acting as a multicast
         * router, so that the process-level routing demon can hear it.
         */
        imo->imo_multicast_loop = (ip_mrouter != NULL);

	if (type & IGMP_QFLAG) {
		/*
		 * Queue up the options and IP packet for future sending
		 * once we drop the locks - to avoid double trip.
		 */
		mopts->m_next = m;
		mopts->m_act = NULL;
		if (igmpq_head == NULL) {
			igmpq_head = mopts;
		} else {
			igmpq_tail->m_act = mopts;
		}
		igmpq_tail = mopts;
	} else {
		ip_output(m, (struct mbuf *)0, (struct route *)0,
	 		IP_MULTICASTOPTS, mopts, NULL);
		m_free(mopts);
	}
        IGMPSTAT(igps_snd_reports);
	return;
}

int
igmp_update_host_membership(struct in_addr addr, struct ifnet *ifp)
{
	register struct in_multi *inm;

	inm = (struct in_multi *)hash_lookup(&hashinfo_inaddr,
		in_multi_match,
		(caddr_t)&addr,
		(caddr_t)ifp,
		(caddr_t)0);
	/*
	 * If we belong to the group being reported, stop
	 * our timer for that group.
	 */
	if (inm) {
		IGMPSTAT(igps_rcv_ourreports);

		IGMP_LOCK();
		inm->inm_timer = 0;
		  
		switch(inm->inm_state){
			case IGMP_IDLE_MEMBER:
			case IGMP_LAZY_MEMBER:
			case IGMP_AWAKENING_MEMBER:
			case IGMP_SLEEPING_MEMBER:
				inm->inm_state = IGMP_SLEEPING_MEMBER;
				break;

			case IGMP_DELAYING_MEMBER:
				inm->inm_state =
					(inm->inm_rti->type == IGMP_OLD_ROUTER)
					 ? IGMP_LAZY_MEMBER
					 : IGMP_SLEEPING_MEMBER;
				break;
			default:
				break;
			}
		IGMP_UNLOCK();
	}
	return ((inm) ? 1 : 0);
}

int
igmp_update_host_new_membership(struct in_addr addr, struct ifnet *ifp)
{
	register struct in_multi *inm;

	inm = (struct in_multi *)hash_lookup(&hashinfo_inaddr,
		in_multi_match,
		(caddr_t)&addr,
		(caddr_t)ifp,
		(caddr_t)0);
	/*
	 * If we belong to the group being reported,
	 * stop our timer for that group.
	 */
	if (inm) {
		IGMPSTAT(igps_rcv_ourreports);
		  
		IGMP_LOCK();
		inm->inm_timer = 0;

		switch(inm->inm_state){

		case IGMP_DELAYING_MEMBER:
		case IGMP_IDLE_MEMBER:
			inm->inm_state = IGMP_LAZY_MEMBER;
			break;
		case IGMP_AWAKENING_MEMBER:
			inm->inm_state = IGMP_LAZY_MEMBER;
			break;
		case IGMP_LAZY_MEMBER:
		case IGMP_SLEEPING_MEMBER:
			break;
		default:
			break;
		}
		IGMP_UNLOCK();
	}
	return ((inm) ? 1 : 0);
}

