/*
 * Generic ifnet operations for Ethernet device drivers.
 *
 * Copyright 1988, 1990, Silicon Graphics, Inc. 
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

#ident "$Revision: 2.61 $"

#include "sys/types.h"
#include "sys/param.h"
#include "sys/sema.h"
#include "sys/hashing.h"

#include "sys/cmn_err.h"
#include "sys/systm.h"
#include "sys/debug.h"
#include "sys/errno.h"
#include "sys/hwgraph.h"
#include "sys/invent.h"
#include "sys/iograph.h"
#include "sys/mbuf.h"
#include "sys/kmem.h"
#include "sys/socket.h"
#include "net/if.h"
#include "net/if_types.h"
#include "net/netisr.h"
#include "net/soioctl.h"
#include "net/route.h"
#include "netinet/in.h"
#include "netinet/in_systm.h"
#include "netinet/in_var.h"
#include "netinet/ip.h"
#ifdef INET6
#include <netinet/in6_var.h>
#include <netinet/ip6.h>
#endif
#include "ether.h"
#ifdef INET6
#include <netinet/if_ether6.h>
#include <netinet/if_ndp6.h>
#endif
#include "bstring.h"

#include "sys/llc.h"
#include "sys/dlsap_register.h"

#ifdef DEBUG
int kdbx_on = 0;
#endif

/*
 * External Procedures
 */
extern int looutput(struct ifnet *, struct mbuf *, struct sockaddr *,
	struct rtentry *);
extern int loioctl(struct ifnet *ifp, int cmd, caddr_t data);

/*
 * External global data structures
 */
extern struct ifnet loif;
extern int maxethermtu;

/*
 * Forward references
 */
static int ether_output(struct ifnet *, struct mbuf *, struct sockaddr *,
	struct rtentry *rte);
static int ether_ioctl(struct ifnet *, int, caddr_t);
static int ether_delmulti(struct etherif *, union mkey *);
static int ether_start(struct etherif *, int);
static int ether_addmulti(struct etherif *, union mkey *);

#define	EIF	struct etherif*
#define	EAP	struct etheraddr*
static struct etherifops ether_deadops = {
	(int(*)(EIF, int)) nodev,
	(void(*)(EIF)) nodev,
	(void(*)(struct ifnet *)) nodev,
	(int(*)(EIF, EAP, EAP, u_short, struct mbuf*)) nodev,
	(int(*)(EIF, int, void*)) nodev
};
#undef EIF
#undef EAP

/*ARGSUSED*/
void
ether_attach(struct etherif *eif,
	     char *name,
	     int unit,
	     caddr_t eif_private,
	     struct etherifops *ops,
	     struct etheraddr *ea,
	     int controller,
	     int state)
{
	struct ifnet *ifp;

	ASSERT(unit >= 0);

	/*
	 * Initialize our generic network interface structure.
	 */
	ifp = eiftoifp(eif);
	ifp->if_name = name;
	ifp->if_unit = unit;
	ifp->if_type = IFT_ETHER;
	if (ETHERMIN <= maxethermtu && maxethermtu <= ETHERMTU)
		ifp->if_mtu = maxethermtu;
	else
		ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST|IFF_MULTICAST|IFF_IPALIAS;
	ifp->if_baudrate.ifs_value = 10*1000*1000;
	ifp->if_output = ether_output;
	ifp->if_ioctl = (int (*)(struct ifnet*,int,void*))ether_ioctl;
	ifp->if_watchdog = ops->eio_watchdog;
#ifdef INET6
	ifp->if_6llocal = &eif->eif_llip6;
	/*
	 * The ethernet link-local addr is FE80::interface-ID
	 */
	ifp->if_6llocal->s6_addr32[0] = 0xfe800000;

	/*
	 * Interface id is: [ ea0^0x2 ea1 ea2 ff fe ea3 ea4 ea5 ]
	 * The 0x2 in the first byte is to flip the universal/local bit
	 * to indicate global scope.
	 */
	ifp->if_6llocal->s6_addr8[8] = ea->ea_vec[0] ^ 0x2;
	ifp->if_6llocal->s6_addr8[9] = ea->ea_vec[1];
	ifp->if_6llocal->s6_addr8[10] = ea->ea_vec[2];
	ifp->if_6llocal->s6_addr8[11] = 0xff;
	ifp->if_6llocal->s6_addr8[12] = 0xfe;
	ifp->if_6llocal->s6_addr8[13] = ea->ea_vec[3];
	ifp->if_6llocal->s6_addr8[14] = ea->ea_vec[4];
	ifp->if_6llocal->s6_addr8[15] = ea->ea_vec[5];

	ifp->if_6l2addr = (char *)&eif->eif_arpcom.ac_enaddr[0];
	ifp->if_ndtype = (IFND6_IEEE | IFND6_INLL | IFND6_ADDRES);
#endif
	/*
	 * we use arp, so we need to use the arp route installation function
	 */
	ifp->if_rtrequest = arp_rtrequest;

	if_attach(ifp);

	/*
	 * Initialize current and official Ethernet addresses.
	 */
	ether_copy(ea->ea_vec, eif->eif_arpcom.ac_enaddr);
	eif->eif_addr = *ea;

	/*
	 * Initialize private data and operations pointers.
	 */
	eif->eif_private = eif_private;
	eif->eif_ops = ops;

	/*
	 * Allocate a multicast filter table with an initial size of 10.
	 */
	if (!mfnew(&eif->eif_mfilter, 10)) {
		cmn_err_tag(277,CE_PANIC, "%s%d: no memory for multicast filter\n",
			name, unit);
	}

	/*
	 * Initialize the raw domain interface.
	 */
	rawif_attach(&eif->eif_rawif, ifp,
		     (caddr_t) eif->eif_arpcom.ac_enaddr,
		     (caddr_t) etherbroadcastaddr,
		     sizeof eif->eif_arpcom.ac_enaddr,
		     sizeof(struct ether_header),
		     structoff(ether_header, ether_shost[0]),
		     structoff(ether_header, ether_dhost[0]));
}

void
ether_detach(struct etherif *eif)
{
	struct ifnet *ifp;
	char *name;

	ifp = eiftoifp(eif);

	ASSERT(iff_dead(ifp->if_flags));

	eif->eif_ops = &ether_deadops;
	eif->eif_private = NULL;
	ifp->if_watchdog = NULL;

	name = (char*) kmem_alloc(IFNAMSIZ, KM_SLEEP);
	strcpy(name, ifp->if_name);
	ifp->if_name = name;
}

void
ether_reattach(struct etherif *eif,
	     caddr_t eif_private,
	     struct etherifops *ops)
{
	eif->eif_private = eif_private;
	eif->eif_ops = ops;
	eiftoifp(eif)->if_watchdog = ops->eio_watchdog;
}

/*
 * m contains etherbufhead and packet data, not including CRC.
 */
void
ether_input(struct etherif *eif, int snoopflags, struct mbuf *m)
{
	struct ether_header *eh;
	int len, more;
	u_short type, etype;
	dlsap_family_t *dlp;
	u_short		encap;
	struct snoopfilter *sf = 0;
	struct ifnet *ifp = eiftoifp(eif);

	ASSERT(IFNET_ISLOCKED(ifp));
	/*
	 * Make sure the mbuf is okay - we don't want it trampled by
	 * DMA problems.
	 */	
	GOODMP(m);
	GOODMT(m->m_type);

	more = snoopflags & SN_MORETOCOME;
	snoopflags &= ~SN_MORETOCOME;

	/*
	 * If bad packet, notice an input error.  If no snoopers or not
	 * snooping for this kind of error, bail out.
	 */
	if (snoopflags & SN_ERROR) {
		ifp->if_ierrors++;
		if (!RAWIF_SNOOPING(&eif->eif_rawif)
		    || !(eif->eif_rawif.rif_errsnoop & snoopflags)) {
			m_freem(m);
			return;
		}
	}

	eh = &mtod(m, struct etherbufhead *)->ebh_ether;
	len = m->m_len - sizeof(struct etherbufhead);

	/*
	 * If len is zero or smaller, we must have a bad packet which is
	 * interesting to the error snooper.
	 */
	if (len <= 0) {
	        if (ifp->if_flags & IFF_DRVRLOCK)
		        snoop_input2(&eif->eif_rawif, snoopflags, (caddr_t)eh, 
				    m, len, (struct snoopfilter *)0);
		else
		        snoop_input(&eif->eif_rawif, snoopflags, (caddr_t)eh, 
				    m, len);
		return;
	}

	/*
	 * Check for a promiscuous or logical promiscuous packet imperfectly
	 * filtered by hardware.  Use eif_mfilter to reject multicast packets
	 * not destined for a hostgroup containing this host.  Apply snoop
	 * filters to any packets that pass eif_mfilter.
	 */
	type = ntohs(eh->ether_type);
	if (!(snoopflags & SN_ERROR)
	    && ETHER_NEEDSFILTER(eh->ether_dhost, &eif->eif_arpcom)
	    && !mfethermatch(&eif->eif_mfilter, eh->ether_dhost, 0)) {
		if (!RAWIF_SNOOPING(&eif->eif_rawif)
		    || !(sf = snoop_match(&eif->eif_rawif, (caddr_t) eh, len)))
		  {
			m_freem(m);
			return;
		}
		snoopflags |= SN_PROMISC;
	}

	/*
	 * If snooping, process snoop input.  If snoopflags is non-zero,
	 * this packet is promiscuous or erroneous, and we return now to
	 * avoid normal protocol input.
	 */
	if (RAWIF_SNOOPING(&eif->eif_rawif)) {
	        if (ifp->if_flags & IFF_DRVRLOCK)
		        snoop_input2(&eif->eif_rawif, snoopflags, (caddr_t)eh,
				     m, len, sf);
		else
		        snoop_input(&eif->eif_rawif, snoopflags, (caddr_t)eh, 
				    m, len);
		if (snoopflags)
			return;
	}

	/*
	 * Set the broadcast flag in the mbuf flags and count logically
	 * addressed input packets.
	 */
	if (ETHER_ISGROUP(eh->ether_dhost)) {
		ifp->if_imcasts++;
		if (ETHER_ISBROAD(eh->ether_dhost))
			m->m_flags |= M_BCAST;
		else
			m->m_flags |= M_MCAST;
	}

	/*
	 * Pass decapsulated packet to network layer.  Currently kernel
	 * IP and ARP are coded to use a 'fast path'.  There is no reason
	 * why these protocols cannot be registered and this switch would
	 * then disappear.  The code in the default case would then be all
	 * that was required.
	 */
	switch (type) {
#	  ifdef	STP_SUPPORT
	  case ETHERTYPE_STP: {
		network_input(m, AF_STP, more);
		return;
	  } 
#	  endif	/* STP_SUPPORT */

	  case ETHERTYPE_IP:
		network_input(m, AF_INET, more);
		return;

#ifdef INET6
	  case ETHERTYPE_IPV6:
		network_input(m, AF_INET6, more);
		return;
#endif

	  case ETHERTYPE_ARP:
		arpinput(&eif->eif_arpcom, m);
		return;

	  default:
		/*
		 * Simple check to see if 802.3 LLC packet has been
		 * received.  If this packet contains an 802.2 packet
		 * the eh->ether_type is really the length.  len is 
		 * the 'on wire' length and this is NOT equal to the
		 * 802.3 length if padding was added by the sender.
		 */
		if (type <= IEEE_8023_LEN) {
			/*
			 * The following test will allow SNAP 0 encapsulation
			 * to be handled for drivers not using the STREAMS DLPI
			 * based LLC/SNAP. If an llc packet is recieved with
			 * less than sizeof(struct llc) bytes it cannot be SNAP.
			 */
			u_short		hlen;
			struct llc	*llc;

			hlen = MIN(type, sizeof(struct llc));
			hlen += mtod(m, struct ifheader *)->ifh_hdrlen;
			if (m->m_len < hlen) {
				m = m_pullup(m, hlen);
				if (!m) {
					ifp->if_ierrors++;
					return;
				}
			}

			hlen = mtod(m, struct ifheader *)->ifh_hdrlen;
			llc = (struct llc *)(mtod(m, caddr_t) + hlen);

			if (type > sizeof(struct llc) &&
			    llc->llc_c1 == RFC1042_C1 &&
			    llc->llc_c2 == RFC1042_C2) {
				etype = llc->llc_etype;
				encap = DL_SNAP0_ENCAP;
			} else {
				etype = llc->llc_dsap;
				encap = DL_LLC_ENCAP;
			}
		} else {
			etype = type;
			encap = DL_ETHER_ENCAP;
		}

		/*
		 * Input routines takes complete responsibiltiy for
		 * freeing the mbuf.  If the packet is to be queued
		 * and/or network_input needs to be called this is again
		 * the responsibiltiy of the input routine.  If the
		 * routine returns zero then the packet will be passed
		 * to drain (this interface is for internal use only).
		 * All customers drivers MUST return a non-zero value.
		 */
		dlp = dlsap_find(etype, encap);
		if (dlp && (*dlp->dl_infunc)(dlp, ifp, m, eh))
			return;

		if (RAWIF_DRAINING(&eif->eif_rawif)) {
			drain_input(&eif->eif_rawif, type,
				    (caddr_t)eh->ether_shost, m);
		} else {
			m_freem(m);
			eiftoifp(eif)->if_noproto++;
		}
		return;
	}
}

/*
 * Send m to the Ethernet address associated with dst on interface ifp.
 * We assume the device cannot hear its own broadcasts, so we duplicate
 * IP broadcast messages and send them to the loopback interface.
 *
 * This code still requires the switch statement becasue the demultiplexing
 * is done using the sa_family and not ethertype.  To remove the switch
 * arp would need to be handled differently.  At the moment the output
 * routine in the dlsap_register is not used and all transmission must
 * be done with AF_SDL.
 */
static int
ether_output(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst,
	struct rtentry *rte)
{
	struct etherif *eif;
	struct etheraddr *esrc, *edst;
	struct mbuf *mloop;
	struct etheraddr ea;
	u_short type;
	int error;
	int isgroup = 0;
	int dstvalid = 1;

	ASSERT(IFNET_ISLOCKED(ifp) || kdbx_on);
	eif = ifptoeif(ifp);
	esrc = (struct etheraddr *) eif->eif_arpcom.ac_enaddr;
	mloop = 0;

	if (iff_dead(ifp->if_flags)) {
		error = EHOSTDOWN;
		goto bad;
	}
#ifdef INET6
	switch (((struct sockaddr_new *)dst)->sa_family) {
#else
	switch (dst->sa_family) {
#endif
	 
#ifdef	STP_SUPPORT
	  case AF_STP: {
		struct in_addr ipdst;

		ipdst = ((struct sockaddr_in *)dst)->sin_addr;
		edst = &ea;

		{
			extern char *inet_ntoa(struct in_addr);
			/****************
			dprintf(10, "Sending packet (sz %d) to %s\n",
				m->m_len, inet_ntoa(ipdst));
			****************/

		}

		if (!ip_arpresolve(&eif->eif_arpcom, m, &ipdst, edst->ea_vec)) {
			/*
			 * Address not yet resolved: arp will hang onto m.
			 */
			return 0;
		}

		if (ETHER_ISBROAD(ea.ea_vec)) {
			mloop = m_copy(m, 0, M_COPYALL);
			if (mloop == 0) {
				error = ENOBUFS;
				goto bad;
			}
		}

		type = ETHERTYPE_STP;

		break;
	  }
#endif	/* STP_SUPPORT */

	  case AF_INET: {
		struct in_addr ipdst;

		ipdst = ((struct sockaddr_in *)dst)->sin_addr;
		edst = &ea;
		if (!arpresolve(&eif->eif_arpcom, rte, m, &ipdst, edst->ea_vec,
			&error)) {
			/*
			 * Address not yet resolved: arp will hang onto m or free it
			 * on error.
			 */
			return error;
		}
		if (ETHER_ISBROAD(ea.ea_vec)) {
			mloop = m_copy(m, 0, M_COPYALL);
			if (mloop == 0) {
				error = ENOBUFS;
				goto bad;
			}
		}
		type = ETHERTYPE_IP;
		break;
	  }
#ifdef INET6
	  case AF_INET6: {
		register struct rtentry *rt;
		/*
		 * We take the lock for reading here to get parallelism.
		 * If we need to acquire it for write mode, it will happen
		 * inside of rtalloc1(), in the RTM_RESOLVE case.
		 * Normal rtalloc() calls won't have to promote the lock.
		 * In order to allow for multiple readers potentially messing
		 * with the same arp/route entry, we use swap_ptr() to
		 * manipulate the held message.
		 */
		ROUTE_RDLOCK();
		if (rt = rte) {
			if (rt->rt_flags & RTF_GATEWAY) {
				if (rt->rt_gwroute == 0)
					goto lookup;
				if (((rt = rt->rt_gwroute)->rt_flags & RTF_UP)
				  == 0) {
					rtfree_needpromote(rt);
					rt = rte;
lookup:
					rt->rt_gwroute =
					  rtalloc1(rt->rt_gateway, 1);
					if ((rt = rt->rt_gwroute) == 0) {
						ROUTE_UNLOCK();
						error = EHOSTUNREACH;
						goto bad;
					}
				}
			}
			if ((rt->rt_flags & RTF_REJECT) &&
				((rt->rt_rmx.rmx_expire == 0) ||
				(time < rt->rt_rmx.rmx_expire))) {
					ROUTE_UNLOCK();
					if (rt == rte)
						error = EHOSTDOWN;
					else
						error = EHOSTUNREACH;
					goto bad;
			}
		}
		ROUTE_UNLOCK();
		if (rt && (rt->rt_flags & RTF_REJECT)) {
			if (rt == rte)
				error = EHOSTDOWN;
			else
				error = EHOSTUNREACH;
			goto bad;
		}
		/*
		 * ndp6_resolve can call icmp6_send which will call ip6_output
		 * so we have to drop the IFNET lock.
		 */
		edst = &ea;
		IFNET_UPPERUNLOCK(ifp);
		if (!ndp6_resolve(ifp, rt, m, dst, edst->ea_vec)) {
			IFNET_UPPERLOCK(ifp);
			return (0);     /* if not yet resolved */
		}
		IFNET_UPPERLOCK(ifp);
		m->m_flags &= ~M_NOPROBE;
		type = ETHERTYPE_IPV6;
		break;
	  }
#endif

	  case AF_SDL: {
		struct sockaddr_sdl *sdl;
	
		sdl = (struct sockaddr_sdl *)dst;
		if (sdl->ssdl_addr_len != ETHERADDRLEN) {
			error = EAFNOSUPPORT;
			goto bad;
		}
		edst = (struct etheraddr *)sdl->ssdl_addr;
		type = (u_short)sdl->ssdl_ethersap;
		break;
	  }

	  case AF_UNSPEC: {
		struct ether_header *eh;

		eh = (struct ether_header*) dst->sa_data;
		edst = (struct etheraddr *) eh->ether_dhost;
		type = eh->ether_type;
		break;
	  }

	  case AF_RAW: {
		struct ether_header *eh;

		/*
		 * The mbuf m begins with an Ethernet header: note that
		 * dst->sa_data is not used.
		 */
		if (m->m_len < sizeof *eh) {
			error = EIO;
			goto bad;
		}
		eh = mtod(m, struct ether_header *);
		edst = (struct etheraddr *) eh->ether_dhost;
		if (ETHER_ISGROUP(edst->ea_vec)) {
			isgroup = 1;
		}
		dstvalid = 0;
		esrc = (struct etheraddr *) eh->ether_shost;
		type = eh->ether_type;
		M_ADJ(m, sizeof *eh);
		if (m == NULL)
		    goto bad;
		break;
	  }

	  default:
		error = EAFNOSUPPORT;
		goto bad;
	}

	/*
	 * Transmit m with the header parameters edst, esrc, and type.
	 */
	ifp->if_opackets++;
	error = eif_transmit(eif, edst, esrc, htons(type), m);
	if (error) {
		ifp->if_oerrors++;
		ifp->if_odrops++;
		m_freem(mloop);
		return error;
	}

	/*
	 * Now send to ourself, if necessary.
	 */
	if (mloop != 0) {
		ifp->if_omcasts++;
		error = looutput(&loif, mloop, dst, rte);
	} else if ((dstvalid && ETHER_ISGROUP(edst->ea_vec)) || isgroup) {
		ifp->if_omcasts++;
	}

	return error;

bad:
	/*
	 * Account for output errors and free mbuf chains.
	 */
	ifp->if_oerrors++;
	m_freem(m);
	m_freem(mloop);
	return error;
}

/*
 * Called by transmit for hardware which cannot hear itself when promiscuous.
 * Get a variable length cluster and copy from eh and m into it.  Give this
 * mbuf to the snooper.
 *
 * We cannot use m_copy to clone m because the mbuf dup function might add
 * references to cluster memory which are removed only by a read from the
 * snoop socket.  Consider paging in a remote snoop program that is capturing
 * NFS traffic: the snooper sleeps waiting for an NFS read RPC call to finish
 * transmitting, by waiting for an mbuf copy-reference count to go to zero.
 * If the driver uses m_copy to make the snooper's copy, the reference count
 * cannot go to zero until the snooper pagein completes and it wakes up and
 * reads its copy!
 */
void
ether_selfsnoop(
	struct etherif *eif,
	struct ether_header *eh,
	struct mbuf *m,
	int off,
	int len)
{
	struct mbuf *n;
	struct etherbufhead *ebh;

	n = m_vget(M_DONTWAIT, sizeof *ebh + len, MT_DATA);
	if (n == 0) {
		snoop_drop(&eif->eif_rawif, SN_PROMISC, (caddr_t) eh, len);
		return;
	}
	ebh = mtod(n, struct etherbufhead *);
	IF_INITHEADER(ebh, eiftoifp(eif), sizeof *ebh);
	(void) m_datacopy(m, off, len, (caddr_t)(ebh + 1));
	ebh->ebh_ether = *eh;
	snoop_input(&eif->eif_rawif, SN_PROMISC, (caddr_t) &ebh->ebh_ether, n,
		    (len < ETHERMIN) ? ETHERMIN : len);
}

static int
ether_ioctl(struct ifnet *ifp, int cmd, caddr_t data)
{
	struct etherif *eif;
	int error;

	ASSERT(IFNET_ISLOCKED(ifp) || kdbx_on);
	eif = ifptoeif(ifp);
	error = 0;

	switch (cmd) {
	  case SIOCAIFADDR: {		/* Add interface alias address */
		struct sockaddr *addr = _IFADDR_SA(data);
#ifdef INET6
		/*
		 * arp is only for v4.  So we only call arpwhohas if there is
		 * an ipv4 addr.
		 */
		if (ifp->in_ifaddr != 0)
			arprequest(&eif->eif_arpcom, 
				&((struct sockaddr_in*)addr)->sin_addr,
				&((struct sockaddr_in*)addr)->sin_addr,
				(&eif->eif_arpcom)->ac_enaddr);
#else
		arprequest(&eif->eif_arpcom, 
			&((struct sockaddr_in*)addr)->sin_addr,
			&((struct sockaddr_in*)addr)->sin_addr,
			(&eif->eif_arpcom)->ac_enaddr);
#endif
		
		break;
	  }

	  case SIOCDIFADDR:		/* Delete interface alias address */
		/* currently nothing needs to be done */
		break;

	  case SIOCSIFADDR: {
		struct sockaddr *addr = _IFADDR_SA(data);

#ifdef INET6
		switch (((struct sockaddr_new *)addr)->sa_family) {
#else
		switch (addr->sa_family) {
#endif
		  case AF_INET:
			ether_stop(eif, ifp->if_flags);
			eif->eif_arpcom.ac_ipaddr = ((struct sockaddr_in*)addr)->sin_addr;
			error = ether_start(eif, ifp->if_flags);
			break;
#ifdef INET6
		  case AF_INET6:
			ether_stop(eif, ifp->if_flags);
			ndp6_ifinit(ifp, (struct ifaddr *)data);
			error = ether_start(eif, ifp->if_flags);
			break;
#endif

		  case AF_RAW:
			ether_copy(addr->sa_data, eif->eif_arpcom.ac_enaddr);
			error = ether_start(eif, ifp->if_flags);
			break;
		}
		break;
	  }

	  case SIOCSIFFLAGS: {
		struct ifreq *ifr = (struct ifreq *)data;

		if (ifr->ifr_flags & IFF_UP)
			error = ether_start(eif, ifr->ifr_flags);
		else
			ether_stop(eif, ifr->ifr_flags);
		break;
	  }

	  case SIOCADDMULTI: {
		int allmulti;

		error = ether_cvtmulti((struct sockaddr *)data, &allmulti);
		if (error)
			break;
		if (allmulti) {
			ifp->if_flags |= IFF_ALLMULTI;
			error = eif_init(eif, ifp->if_flags);
		} else {
			error = ether_addmulti(eif, satomk(data));
		}
		break;
	  }

	  case SIOCDELMULTI: {
		int allmulti;

		error = ether_cvtmulti((struct sockaddr *)data, &allmulti);
		if (error)
			break;
		if (allmulti) {
			if (!(ifp->if_flags & IFF_ALLMULTI))
				break;
			ifp->if_flags &= ~IFF_ALLMULTI;
			error = eif_init(eif, ifp->if_flags);
		} else {
			error = ether_delmulti(eif, satomk(data));
		}
		break;
	  }

#define	sf	((struct snoopfilter *) data)
	  case SIOCADDSNOOP:
	  case SIOCDELSNOOP: {
		struct ether_header *eh;
		union mkey key;

		eh = RAW_HDR(sf->sf_mask, struct ether_header);
		if (!ETHER_ISBROAD(eh->ether_dhost)) {
			error = EINVAL;
			break;
		}
		eh = RAW_HDR(sf->sf_match, struct ether_header);
		if (!ETHER_ISMULTI(eh->ether_dhost)) {
			error = EINVAL;
			break;
		}
		key.mk_family = AF_UNSPEC;
		ether_copy(eh->ether_dhost, key.mk_dhost);
		if (cmd == SIOCADDSNOOP)
			error = ether_addmulti(eif, &key);
		else
			error = ether_delmulti(eif, &key);
		break;
	  }
#undef sf

	  default:
		error = eif_ioctl(eif, cmd, data);
	}
	return (error);
}

/*
 * Add multicast address.  If key is already associated, do nothing.
 * Otherwise tell driver to add a hardware address filter. 
 * If adding the software filter fails, remove the hw filter.
 */
static int
ether_addmulti(struct etherif *eif, union mkey *key)
{
	struct mfreq mfr;
	int error;

	ASSERT(IFNET_ISLOCKED(eiftoifp(eif)));
	if (mfmatchcnt(&eif->eif_mfilter, 1, key, 0))
		return 0;
	mfr.mfr_key = key;
	error = eif_ioctl(eif, SIOCADDMULTI, (caddr_t) &mfr);
	if (error)
		return error;
	if (!mfadd(&eif->eif_mfilter, key, mfr.mfr_value)) {
		(void) eif_ioctl(eif, SIOCDELMULTI, (caddr_t) &mfr);
		return ENOMEM;
	}
	return error;
}

/*
 * Delete multicast address.  If key is unassociated, do nothing.
 * Otherwise delete software filter first, then hardware filter.
 * This order allows the device to use mfhasvalue to detect last
 * use of a shared address filter.
 */
static int
ether_delmulti(struct etherif *eif, union mkey *key)
{
	struct mfreq mfr;
	int error;

	ASSERT(IFNET_ISLOCKED(eiftoifp(eif)));
	if (!mfmatchcnt(&eif->eif_mfilter, -1, key, &mfr.mfr_value))
		return 0;
	mfdel(&eif->eif_mfilter, key);
	mfr.mfr_key = key;
	error = eif_ioctl(eif, SIOCDELMULTI, (caddr_t) &mfr);
	if (error)
		(void) mfadd(&eif->eif_mfilter, key, mfr.mfr_value);
	return error;
}

/*
 * Set the interface running.  Must be called with interface locked
 */
static int
ether_start(struct etherif *eif, int flags)
{
	int error;

	ASSERT(IFNET_ISLOCKED(eiftoifp(eif)) || kdbx_on);
	eif->eif_resets = 0;
	eiftoifp(eif)->if_flags = flags;
	error = eif_init(eif, flags);

#ifdef INET6
	if (error || (eiftoifp(eif)->in_ifaddr == 0 && 
	  eiftoifp(eif)->in6_ifaddr == 0))
#else
	if (error || eiftoifp(eif)->in_ifaddr == 0)
#endif
		return error;

	eiftoifp(eif)->if_flags |= IFF_ALIVE;
#ifdef INET6
	/*
	 * arp is only for v4.  So we only call arpwhohas if there is
	 * an ipv4 addr.
	 */
	if (eiftoifp(eif)->in_ifaddr != 0)
		ARP_WHOHAS(&eif->eif_arpcom, &eif->eif_arpcom.ac_ipaddr);
#else
	ARP_WHOHAS(&eif->eif_arpcom, &eif->eif_arpcom.ac_ipaddr);
#endif
	return 0;
}

void
ether_stop(struct etherif *eif, int flags)
{
	ASSERT(IFNET_ISLOCKED(eiftoifp(eif)) || kdbx_on);
	eiftoifp(eif)->if_flags = flags & ~IFF_ALIVE;
	if_down(eiftoifp(eif));
	eif_reset(eif);
}

/*
 *  Return the first ethernet address found.  Could easily be
 *  generalized to return all or more than one of them , but until
 *  someone cares, don't bother.  Return value is number found.
 */
int
get_eaddr (char eaddr [])
{
	struct	ifnet	*ifp;
	struct	etherif	*eif;
		int	nfound = 0;

	IFHEAD_LOCK ();
        for (ifp = ifnet; ifp; ifp = ifp->if_next) {
		IFNET_LOCK (ifp);
		if (ifp->if_type == IFT_ETHER) {
			eif = ifptoeif (ifp);
			ether_copy (eif->eif_arpcom.ac_enaddr, eaddr);
			IFNET_UNLOCK (ifp);
			nfound++;
			break;	/* just get the first one found */
		}
		IFNET_UNLOCK (ifp);
	}
	IFHEAD_UNLOCK ();
	return nfound;
}

/*
 * Registration code.
 * I think "DLSAP" stands for something like Data Link Service
 * Access Point - ISO jargon?
 *
 * Hash tables of linked list of families that have been registered.
 * dlsap_families is indexd by type.
 */
dlsap_family_t *dlsap_families[256];

/*
 * Using this simple hash function there are 55 clashes
 * with a max depth of 3 using the 224 registered
 * ethertypes.  This hash is also used for llc dsap's
 * which are just an index into the dlsap_families array.
 */
#define DLSAP_HASH(x) ((((x) >> 8) ^ (x)) & 0xff)

/*
 * Call this routine XXX with MP locking that works!!!
 */
dlsap_family_t *
dlsap_insert(dlsap_family_t *dp, ushort key, ushort encap)
{
	dlsap_family_t  *p, *q = 0;
	int		i;

	i = DLSAP_HASH(key);
	p = dlsap_families[i];

	while (p) {
		if (p->dl_ethersap == key && p->dl_encap == encap)
			return(p);
		q = p;
		p = p->dl_next;
	}

	if (q)
		q->dl_next = dp;
	else
		dlsap_families[i] = dp;
	return(0);
}

dlsap_family_t *
dlsap_find(ushort key, ushort encap)
{
	dlsap_family_t  *p;

	p = dlsap_families[DLSAP_HASH(key)];

	while (p && (p->dl_ethersap != key || p->dl_encap != encap))
		p = p->dl_next;

	return(p);
}

/*
 * This routine returns DLSAP_BUSY if 
 * someone has already registered this protocol.
 * It will return 0 if everything is ok.
 */
int
register_dlsap(dlsap_family_t *dfp, dlsap_family_t **first)
{
	/* NOT MP SAFE?? */
	/*
	 * If dl_ethersap has been registered once already
	 * just bump up ref_count.  This is used for DLPI
	 * where the sap must be registered for each interface.
	 */
	*first = dlsap_insert(dfp, dfp->dl_ethersap, dfp->dl_encap);
	if (*first) {
		(*first)->ref_count++;
		return(DLSAP_BUSY);
	}
	/*
	 * Not sure if anyone uses this mechanism any more?
	 */
	ASSERT(dfp->dl_netisr== NULL);
	return(0);
}

int
unregister_dlsap(dlsap_family_t *remove)
{
	dlsap_family_t	*dfp;
	dlsap_family_t	*prev = 0;
	int		i;

	/* NOT MP SAFE? */
	i = DLSAP_HASH(remove->dl_ethersap);
	dfp = dlsap_families[i];

	for (; dfp; prev = dfp, dfp = dfp->dl_next) {
		if (dfp == remove) {
			if (dfp->ref_count) {
				dfp->ref_count--;
				return(0);
			}

			if (prev)
				prev->dl_next = dfp->dl_next;
			else
				dlsap_families[i] = dfp->dl_next;
			return(0);
		}
	}
	return(1);
}
