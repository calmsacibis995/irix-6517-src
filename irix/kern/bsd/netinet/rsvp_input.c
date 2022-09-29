/*
 * Procedures for the kernel part of Multicast Routing.
 * (See RFC-1075.)
 *
 * Written by David Waitzman, BBN Labs, August 1988.
 * Modified by Steve Deering, Stanford, February 1989.
 * Modified by Mark J. Steiglitz, Stanford, May, 1991
 * Modified by Van Jacobson, LBL, January 1993
 * Modified by Ajit Thyagarajan, PARC, August 1993
 * Modified by Bill Fenenr, PARC, April 1995
 *
 * MROUTING 3.5.1.1
 */

#include "tcp-param.h"
#include "sys/param.h"
#include "sys/sema.h"
#include "sys/hashing.h"
#include "sys/kmem.h"

#include "sys/debug.h"
#include "sys/mbuf.h"
#include "sys/socket.h"
#include "sys/socketvar.h"
#include "sys/protosw.h"
#include "sys/errno.h"
#include "sys/time.h"
#include "sys/cmn_err.h"
#include "sys/systm.h"
#include "sys/sesmgr.h"
#include "net/soioctl.h"
#include "net/if.h"
#include "net/route.h"
#include "net/raw.h"

#include "in.h"
#include "in_systm.h"
#include "ip.h"
#include "in_pcb.h"
#include "in_var.h"
#include "ip_var.h"
#include "udp.h"
#include "igmp.h"
#include "igmp_var.h"
#include "ip_mroute.h"

vifi_t numvifs = 0;
struct vif viftable[MAXVIFS];

#ifdef RSVP_ISI
u_int rsvpdebug = 0;
extern int rsvp_on;
extern struct socket *ip_rsvpd;

/*
 * sort of like raw_input - used for the upcalls to mrouted
 * Except returns -1 on error (usually socket overflow) or
 * a potential MP deadlock.
 */
int
socket_send(struct socket *s,
	struct mbuf *mm,
	struct sockaddr_in *src)
{

    if (s) {
	if (!SOCKET_TRYLOCK(s)) {
		m_freem(mm);
		return -1;
	}
	if (sbappendaddr(&s->so_rcv, (struct sockaddr *)src,
		mm, (struct mbuf *)0, 0) != 0) {
	    sorwakeup(s, NETEVENT_RAWUP);
	    SOCKET_UNLOCK(s);
	    return 0;
	}
	SOCKET_UNLOCK(s);
    }
    m_freem(mm);
    return -1;
}


void
rsvp_input(struct mbuf *m, struct ifnet *ifp, struct ipsec *ipsec)
{
    int vifi;
    register struct ip *ip = mtod(m, struct ip *);
    static struct sockaddr_in rsvp_src = { AF_INET };
    register int s;

    _SESMGR_SOATTR_FREE(ipsec);
    if (rsvpdebug)
	printf("rsvp_input: rsvp_on %d\n",rsvp_on);

    /* Can still get packets with rsvp_on = 0 if there is a local member
     * of the group to which the RSVP packet is addressed.  But in this
     * case we want to throw the packet away.
     */
    if (!rsvp_on) {
	if (ip_rsvpd) {
		if (rsvpdebug)
		    printf("Sending packet up old-style socket\n");
#ifdef INET6
		rip_input(m, ifp, NULL, NULL);
#else
		rip_input(m, ifp, NULL);
#endif
		return;
	}
	m_freem(m);
	return;
    }

    s = splnet();

    if (rsvpdebug)
	printf("rsvp_input: check vifs\n");

    /* Find which vif the packet arrived on. */
    for (vifi = 0; vifi < numvifs; vifi++) {
	if (viftable[vifi].v_ifp == ifp)
	    break;
    }

    if (vifi == numvifs) {
	/* Can't find vif packet arrived on. Drop packet. */
	if (rsvpdebug)
	    printf("rsvp_input: Can't find vif for packet...dropping it.\n");
	m_freem(m);
	splx(s);
	return;
    }

    if (rsvpdebug)
	printf("rsvp_input: check socket\n");

    if (viftable[vifi].v_rsvpd == NULL) {
	/* drop packet, since there is no specific socket for this
	 * interface */
	if (rsvpdebug)
	    printf("rsvp_input: No socket defined for vif %d\n",vifi);
	m_freem(m);
	splx(s);
	return;
    }

    rsvp_src.sin_addr = ip->ip_src;

    if (rsvpdebug && m)
	printf("rsvp_input: m->m_len = %d, sbspace() = %d\n",
	       m->m_len,sbspace(&(viftable[vifi].v_rsvpd->so_rcv)));

    if (socket_send(viftable[vifi].v_rsvpd, m, &rsvp_src) < 0)
	if (rsvpdebug)
	    printf("rsvp_input: Failed to append to socket\n");
    else
	if (rsvpdebug)
	    printf("rsvp_input: send packet up\n");
    
    splx(s);
}
#endif /* RSVP_ISI */
