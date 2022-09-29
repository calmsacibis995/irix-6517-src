/*
 * Raw protocol family operations and utilities.
 *
 * $Revision: 1.75 $
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
#include <bstring.h>
#include <string.h>
#include "sys/param.h"
#include "sys/debug.h"
#include "sys/domain.h"
#include "sys/errno.h"
#include "sys/mbuf.h"
#include "sys/protosw.h"
#include "sys/socket.h"
#include "sys/systm.h"
#include "raw_cb.h"
#include "raw.h"
#include "if.h"
#include "if_dl.h"
#include "if_types.h"
#include "soioctl.h"

#include "netinet/in_systm.h"
#include "net/route.h"
#include "netinet/in.h"
#include "netinet/ip.h"
#include "netinet/in_pcb.h"


static struct rawif	*rawif;

/*
 * Raw input protocols must initialize local and foreign sockaddr_raw
 * structs and a sockproto and pass them to raw_input.  These structures
 * are allocated and partially initialized statically.
 */
#ifdef _HAVE_SA_LEN
static struct sockaddr rawdst0 = {sizeof(rawdst0), AF_RAW};
static struct sockaddr_raw rawsrc0 = {sizeof(rawsrc0), AF_RAW};
static struct sockproto rawproto0 = {sizeof(rawproto0), PF_RAW};
#else
static struct sockaddr rawdst0 = {{{AF_RAW}}};
static struct sockaddr_raw rawsrc0 = {{AF_RAW}};
static struct sockproto rawproto0 = {PF_RAW};
#endif

extern struct ifaddr *ifa_ifwithlink_ifp(struct ifnet *);

void rawif_fixlink(struct ifnet *);

/*
 * Attach rif to the raw interface list and initialize non-zero members.
 */
void
rawif_attach(struct rawif *rif,
	     struct ifnet *ifp,
	     caddr_t addr,
	     caddr_t broadaddr,
	     int addrlen,
	     int hdrlen,
	     int srcoff,
	     int dstoff)
{
	int len;
	struct rawif **rifp;

	ASSERT(addrlen <= RAW_MAXADDRLEN);
	rif->rif_ifp = ifp;
	rif->rif_name.sr_family = AF_RAW;

	ASSERT(RAW_IFNAMSIZ <= IFNAMSIZ);
	(void) strncpy(rif->rif_name.sr_ifname, ifp->if_name, RAW_IFNAMSIZ);
	len = strlen(ifp->if_name);
	if (len > RAW_IFNAMSIZ - 1)
		len = RAW_IFNAMSIZ - 1;
		/* XXX Decrementing by one will not be enough if the unit
		 * number is more than a single digit (> 9).
		 */
	sprintf(&rif->rif_name.sr_ifname[len], "%d", ifp->if_unit);

	rif->rif_addr = addr;
	rif->rif_broadaddr = broadaddr;
	rif->rif_addrlen = addrlen;
	rif->rif_hdrlen = hdrlen;
	rif->rif_padhdrlen = roundup(hdrlen, RAW_ALIGNGRAIN);
	rif->rif_hdrpad = rif->rif_padhdrlen - rif->rif_hdrlen;
	rif->rif_hdrgrains = rif->rif_padhdrlen >> RAW_ALIGNSHIFT;
	rif->rif_srcoff = srcoff;
	rif->rif_dstoff = dstoff;
	rif->rif_snooplen = ifp->if_mtu;

	for (rifp = &rawif; *rifp; rifp = &(*rifp)->rif_next)
		;
	rif->rif_next = *rifp;
	*rifp = rif;

	/*
	 * This is from the 4.4BSD-Lite ether_ifattach()
	 */
	ASSERT(rif->rif_addrlen <= 6);	/* XXX ifattach() guesses this */

	ifp->if_addrlen = rif->rif_addrlen;
	ifp->if_hdrlen = rif->rif_hdrlen;

	/* Update MAC level address if necessary */
	rawif_fixlink(ifp);
	return;
}
		
/*
 * Put the MAC address into the sockaddr_dl, either when attaching the
 * interface at first (via if_attach or raw_attch) or later after an address
 * change or when the interface did not know its address when it was attached.
 */
void
rawif_fixlink(struct ifnet *ifp)
{
	struct ifaddr *ifa;
	struct sockaddr_dl *sdl;
	struct rawif *rif;
	extern void bitswapcopy(void *, void *, int);

	for (rif = rawif; ; rif = rif->rif_next) {
		if (!rif)
			return;
		if (rif->rif_ifp == ifp)
			break;
	}

	if (ifp->if_addrlen == 0) /* too early so bail out */
		return;

	if (ifa = ifa_ifwithlink_ifp(ifp)) { /* found AF_LINK record */

		sdl = (struct sockaddr_dl *)ifa->ifa_addr;

		if (!sdl
		    || sdl->sdl_family != AF_LINK
		    || (sdl->sdl_alen != 0
			&&sdl->sdl_alen < ifp->if_addrlen)
#ifdef _HAVE_SA_LEN
		    || sdl->sdl_nlen+ifp->if_addrlen > sdl->sdl_len
#else
		    || sdl->sdl_nlen+ifp->if_addrlen >
			_FAKE_SA_LEN_DST((struct sockaddr*)sdl)
#endif
		    || sdl->sdl_slen != 0)
			return;

		/*
		 * Use MAC level address stored in rawif structure
		 * Update type, address length and copy MAC address
		 * into AF_LINK structure of type sockaddr_dl
		 */
		sdl->sdl_type = ifp->if_type;
		sdl->sdl_alen = ifp->if_addrlen;
		if (ifp->if_type == IFT_FDDI) {
			/* so that everything reports the same address */
			bitswapcopy(rif->rif_addr, LLADDR(sdl), 
				ifp->if_addrlen);
		} else {
			bcopy(rif->rif_addr, LLADDR(sdl), ifp->if_addrlen);
		}
	}
	return;
}

/*
 * Find a raw interface by its network interface name.
 */
struct rawif *
rawif_lookup(const char *ifname)
{
	struct rawif *rif;

	for (rif = rawif; rif; rif = rif->rif_next) {
		if (!strncmp(rif->rif_name.sr_ifname, (char *)ifname, RAW_IFNAMSIZ))
			return rif;
	}
	return 0;
}

/*
 * Find a raw control block given a rawif and a raw port.
 */
static struct rawcb *
rawcb_lookup(struct rawif *rif, u_int protocol, u_int port)
{
	struct rawcb *rp;

	ASSERT(RAWCBLIST_ISMRLOCKED(MR_ACCESS|MR_UPDATE));
	for (rp = rawcb.rcb_next; rp != &rawcb; rp = rp->rcb_next) {
		if (rptorawif(rp) == rif
		    && rp->rcb_proto.sp_protocol == protocol
		    && rptorawsa(rp)->sr_port == port) {
			return rp;
		}
	}
	return 0;
}

/*
 * Associate a raw control block with the named raw interface, or the
 * first IFF_UP interface if sr is zeroed.
 */
int
rawopen(struct rawcb *rp, struct sockaddr_raw *sr)
{
	struct rawif *rif;
	u_int protocol;

	ASSERT(RAWCBLIST_ISMRLOCKED(MR_ACCESS|MR_UPDATE));
	/*
	 * If the interface name is non-null, find the named rawif.
	 * Otherwise use the primary interface.
	 */
	
	if (sr->sr_ifname[0]) {
		rif = rawif_lookup(sr->sr_ifname);
		if (rif == 0)
			return EADDRNOTAVAIL;
	} else {
		for (rif = rawif; ; rif = rif->rif_next) {
			if (rif == 0)
				return EADDRNOTAVAIL;
			if (rif->rif_ifp->if_flags & IFF_UP)
				break;
		}
		bcopy(rif->rif_name.sr_ifname, sr->sr_ifname,
		      sizeof sr->sr_ifname);
	}

	/*
	 * If a snoop port is non-zero, check whether it's already in use.
	 * Otherwise find an unused port.
	 */
	protocol = rp->rcb_proto.sp_protocol;
	switch (protocol) {
	  case RAWPROTO_RAW:
		protocol = rp->rcb_proto.sp_protocol = RAWPROTO_SNOOP;
		/* FALL THROUGH */

	  case RAWPROTO_SNOOP:
		if (sr->sr_port) {
			if (sr->sr_port < SNOOP_MINPORT
			    || SNOOP_MAXPORT < sr->sr_port) {
				return EADDRNOTAVAIL;
			}
			if (rawcb_lookup(rif, protocol, sr->sr_port))
				return EADDRINUSE;
		} else {
			u_int port;

			for (port = SNOOP_MINPORT; ; port++) {
				if (port > SNOOP_MAXPORT)
					return EADDRNOTAVAIL;
				if (rawcb_lookup(rif, protocol, port) == 0)
					break;
			}
			sr->sr_port = port;
		}
		break;
	}
	sr->sr_family = AF_RAW;

	/*
	 * Add an open socket reference to rif.
	 */
	IFNET_LOCK(rif->rif_ifp);	
	rp->rcb_pcb = (caddr_t) rif;
	rif->rif_open[protocol]++;
	IFNET_UNLOCK(rif->rif_ifp);
	return 0;
}

/*
 * Break the association between a raw domain socket and its interface.
 * Delete rp's filters.  On last close, reset protocol state.
 */
void
rawclose(struct rawcb *rp)
{
	struct rawif *rif;
	u_int protocol;
	int count;

	rif = rptorawif(rp);
	if (rif == 0)
		return;
	ASSERT(rp->rcb_proto.sp_family == PF_RAW);

	protocol = rp->rcb_proto.sp_protocol;
	IFNET_LOCK(rif->rif_ifp);
	count = --rif->rif_open[protocol];
	switch (protocol) {
	  case RAWPROTO_SNOOP: {
		u_int port;
		int i;
		struct snoopfilter *sf;

		port = rptorawsa(rp)->sr_port;
		if (rif->rif_errport == port) {
			rif->rif_errsnoop = 0;
			rif->rif_errport = 0;
		}
		i = rif->rif_sfveclen;
		for (sf = rif->rif_sfvec; --i >= 0; sf++) {
			u_int index;

			if (!sf->sf_allocated || sf->sf_port != port)
				continue;
			index = sf->sf_index;
			IFNET_UNLOCK(rif->rif_ifp);
			(void) rawioctl(rp->rcb_socket, SIOCDELSNOOP,
					(caddr_t) &index);
			IFNET_LOCK(rif->rif_ifp);
		}
		if (count == 0) {
			ASSERT(rif->rif_promisc == 0);
			ASSERT(rif->rif_allmulti == 0);
			ASSERT(rif->rif_sfveclen == 0);
			bzero((caddr_t) &rif->rif_stats.rs_snoop,
			      sizeof rif->rif_stats.rs_snoop);
			rif->rif_snooplen = rif->rif_ifp->if_mtu;
		}
		break;
	  }
	  case RAWPROTO_DRAIN:
		if (count == 0) {
			bzero((caddr_t) &rif->rif_stats.rs_drain,
			      sizeof rif->rif_stats.rs_drain);
		}
	}
	IFNET_UNLOCK(rif->rif_ifp);
}

/*
 * Notice raw packets dropped because of socket buffer overflow.
 */
void
rawsbdrop(struct rawcb *rp)
{
	struct rawif *rif;

	rif = rptorawif(rp);
	if (rif == 0)
		return;
	ASSERT(rp->rcb_proto.sp_family == PF_RAW);
	IFNET_LOCK(rif->rif_ifp);
	switch (rp->rcb_proto.sp_protocol) {
	  case RAWPROTO_SNOOP:
		rif->rif_stats.rs_snoop.ss_sbdrops++;
		break;
	  case RAWPROTO_DRAIN:
		rif->rif_stats.rs_drain.ds_sbdrops++;
	}
	IFNET_UNLOCK(rif->rif_ifp);
}

/*
 * Canonicalize a snoopfilter.
 *
 * Clear any bits left set by the user in match which are not also set in
 * mask, so that we can filter packets with ((header & mask) == match).
 */
static void
sfcanon(struct snoopfilter *sf)
{
	__uint32_t *mask, *match;
	int len;

	mask = sf->sf_mask, match = sf->sf_match;
	len = SNOOP_FILTERLEN;
	while (--len >= 0)
		*match++ &= *mask++;
}

static u_char sfallones[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

/*
 * If sf's destination address mask isn't all-ones and the destination to
 * match isn't rif's physical or broadcast address,
 * or if sf's source address mask isn't all-ones and the source to
 * match isn't rif's physical address,
 * then sf requires that the interface receive promiscuously.
 */
static int
sfispromisc(struct snoopfilter *sf, struct rawif *rif)
{
	int off;
	u_char *mask;
	caddr_t addr;
	int len;

	ASSERT(rif->rif_addrlen <= sizeof (sfallones));

	off = rif->rif_hdrpad + rif->rif_dstoff;
	mask = (u_char *)sf->sf_mask + off;
	len = rif->rif_addrlen;
	addr = (caddr_t)sf->sf_match + off;

	if (!bcmp(mask, sfallones, len)) {
		if (!bcmp(addr, rif->rif_addr, len))
			return (0);
		if (rif->rif_broadaddr && !bcmp(addr, rif->rif_broadaddr, len))
			return 0;
	}

	off = rif->rif_hdrpad + rif->rif_srcoff;
	mask = (u_char *)sf->sf_mask + off;
	addr = (caddr_t)sf->sf_match + off;

	if (!bcmp(mask, sfallones, len))
		if (!bcmp(addr, rif->rif_addr, len))
			return (0);

	return (1);
}

/*
 * Match the cnt words of packet headers pointed at by hp against sf.
 */
int
sfmatch(struct snoopfilter *sf, __uint32_t *hp, int cnt)
{
	__uint32_t *mask, *match;

	mask = sf->sf_mask;
	match = sf->sf_match;
	while (--cnt >= 0) {
		if ((*hp & *mask) != *match)
			return 0;
		hp++, mask++, match++;
	}
	return 1;
}

/*
 * Raw domain inquiry and control ops.
 */
int
rawioctl(struct socket *so, __psint_t cmd, caddr_t data)
{
	struct rawif *rif;
	struct ifaddr ia;
	int error = 0;

	ASSERT(SOCKET_ISLOCKED(so));
	rif = sotorawif(so);
	if (rif == 0) {
		/*
		 * Socket not bound: argument must be an ifreq naming an
		 * interface and its link-layer address.
		 */
		if (cmd != SIOCGIFADDR && cmd != SIOCSIFADDR)
			return EINVAL;
		rif = rawif_lookup(((struct ifreq *)data)->ifr_name);
		if (rif == 0)
			return EADDRNOTAVAIL;
	}
	IFNET_LOCK(rif->rif_ifp); /* XXX huy these should be nop for UP */

	switch (cmd) {
	  case SIOCGIFADDR: {
		/*
		 * Get interface address.
		 */
		struct sockaddr_raw *sr;
		int len, rem;

		sr = (struct sockaddr_raw *) &((struct ifreq *)data)->ifr_addr;
		sr->sr_family = AF_RAW;
		len = rif->rif_addrlen;
		bcopy(rif->rif_addr, sr->sr_addr, len);
		rem = RAW_MAXADDRLEN - len;
		if (rem > 0)
			bzero(sr->sr_addr + len, rem);
		break;
	  }

	  case SIOCSIFADDR: {
		struct ifnet *ifp;

		ifp = rif->rif_ifp;
		if (ifp->if_ioctl == 0) {
			error = EOPNOTSUPP;
			break;
		}
		ia.ifa_addr = &((struct ifreq *)data)->ifr_addr;
		error = (*ifp->if_ioctl)(ifp, cmd, &ia);
		if (!error) 
			rawif_fixlink(ifp);
		break;
	  }

	  case SIOCRAWSTATS:
		/*
		 * Get raw interface statistics.
		 */
		bcopy((caddr_t) &rif->rif_stats, data, sizeof rif->rif_stats);
		break;

	  case SIOCADDSNOOP: {
		/*
		 * Add a snoop filter to rif.
		 * NB: we assume no if_ioctl calls made below will sleep.
		 */
		u_int index;
		struct snoopfilter *sf;
		struct ifnet *ifp;

		if (so->so_proto->pr_protocol != RAWPROTO_SNOOP) {
			error = EOPNOTSUPP;
			break;
		}

		ASSERT(rif->rif_sfveclen <= SNOOP_MAXFILTERS);
		for (index = 0; index < rif->rif_sfveclen; index++) {
			if (!rif->rif_sfvec[index].sf_allocated)
				break;
		}
		if (index == SNOOP_MAXFILTERS) {
			error = ENOMEM;
			break;
		}

		/*
		 * Canonicalize the filter and set its attributes in-place
		 * for copying back to the user.
		 */
		sf = (struct snoopfilter *) data;
		sfcanon(sf);
		sf->sf_allocated = 1;
		sf->sf_active = 0;
		sf->sf_promisc = 0;
		sf->sf_allmulti = 0;
		sf->sf_index = index;
		sf->sf_port = sotorawsa(so)->sr_port;

		/*
		 * See if the interface can do this filter.  If it looks
		 * promiscuous or logical-promiscuous, the interface should
		 * set sf_promisc or sf_allmulti.
		 */
		ifp = rif->rif_ifp;
		if ((ifp->if_ioctl == 0
		    || (*ifp->if_ioctl)(ifp, cmd, (caddr_t) sf) != 0)
		    && sfispromisc(sf, rif)) {
			sf->sf_promisc = 1;
		}

		/*
		 * If there are novel promiscuity flags (i.e. ones which
		 * have not yet been counted), pass them to the interface.
		 * Notice that we still haven't installed the filter, so
		 * we can bail out with an early return on error.
		 */
		if (sf->sf_promisc && rif->rif_promisc == 0
		    || sf->sf_allmulti && rif->rif_allmulti == 0) {
			struct ifreq ifr;

			if (ifp->if_ioctl == 0) {
				error = EINVAL;
				break;
			}
			ifr.ifr_flags = ifp->if_flags
			    | (sf->sf_promisc ? IFF_PROMISC : IFF_ALLMULTI);
			error = (*ifp->if_ioctl)(ifp, SIOCSIFFLAGS,
				(caddr_t) &ifr);
			if (error)
				break;
		}

		/*
		 * Finally, install the (inactive) filter.  Check whether
		 * rif_sfveclen needs to be bumped.  Don't need to splimp
		 * assuming atomic store of rif_sfveclen.
		 */
		if (sf->sf_promisc)
			rif->rif_promisc++;
		if (sf->sf_allmulti)
			rif->rif_allmulti++;
		bcopy((caddr_t) sf, (caddr_t) &rif->rif_sfvec[index],
		      sizeof *sf);
		if (index == rif->rif_sfveclen)
			rif->rif_sfveclen++;
		break;
	  }

	  case SIOCDELSNOOP: {
		/*
		 * Delete snoop filter.
		 */
		u_int index;
		struct snoopfilter *sf;
		struct ifnet *ifp;

		if (so->so_proto->pr_protocol != RAWPROTO_SNOOP) {
			error = EOPNOTSUPP;
			break;
		}

		index = *(u_int *)data;
		if (index >= rif->rif_sfveclen) {
			error = EINVAL;
			break;
		}
		sf = &rif->rif_sfvec[index];
		if (sf->sf_port != sotorawsa(so)->sr_port) {
			error = EACCES;
			break;
		}

		/*
		 * Tell the interface to delete this filter, but ignore any
		 * error (including EINVAL, meaning the interface doesn't
		 * understand raw filter commands).  If this is the last
		 * filter of a given promiscuity type, clear the filter's
		 * interface flags.
		 */
		ifp = rif->rif_ifp;
		if (ifp->if_ioctl)
			(void) (*ifp->if_ioctl)(ifp, cmd, (caddr_t) sf);
		if (sf->sf_promisc && rif->rif_promisc == 1
		    || sf->sf_allmulti && rif->rif_allmulti == 1) {
			struct ifreq ifr;

			ASSERT(ifp->if_ioctl);
			ifr.ifr_flags = ifp->if_flags
			    & (sf->sf_promisc ? ~IFF_PROMISC : ~IFF_ALLMULTI);
			(void) (*ifp->if_ioctl)(ifp, SIOCSIFFLAGS,
				(caddr_t) &ifr);
		}

		/*
		 * Deactivate and free the filter, adjusting rif_sfveclen
		 * downward.  Note that we don't need to splimp here.
		 */
		sf->sf_allocated = sf->sf_active = 0;
		if (sf->sf_promisc)
			--rif->rif_promisc;
		if (sf->sf_allmulti)
			--rif->rif_allmulti;
		if (index == rif->rif_sfveclen - 1) {
			while (index
			    && !rif->rif_sfvec[index-1].sf_allocated) {
				--index;
			}
			rif->rif_sfveclen = index;
		}
		break;
	  }

	  case SIOCSNOOPLEN: {
		/*
		 * Set snoop capture length.
		 */
		int snooplen;

		if (so->so_proto->pr_protocol != RAWPROTO_SNOOP) {
			error = EOPNOTSUPP;
			break;
		}

		snooplen = *(int *)data;
		if (snooplen < 0 || rif->rif_ifp->if_mtu < snooplen) {
			error = EINVAL;
			break;
		}
		/*
		 * Change snooplen only if we are the only snooper, or if we
		 * are increasing it for one of many snoopers.
		 */
		if (rif->rif_open[RAWPROTO_SNOOP] == 1
		    || snooplen > rif->rif_snooplen) {
			rif->rif_snooplen = snooplen;
		}
		break;
	  }

	  case SIOCSNOOPING: {
		/*
		 * Turn snooping on or off for this port.
		 */
		u_int port;
		int i;
		struct snoopfilter *sf;

		if (so->so_proto->pr_protocol != RAWPROTO_SNOOP) {
			error = EOPNOTSUPP;
			break;
		}

		port = sotorawsa(so)->sr_port;
		i = rif->rif_sfveclen;
		for (sf = rif->rif_sfvec; --i >= 0; sf++) {
			if (!sf->sf_allocated || sf->sf_port != port)
				continue;
			sf->sf_active = (*(int *)data != 0);
		}
		break;
	  }

	  case SIOCERRSNOOP: {
		u_int port;
		int flags;

		if (so->so_proto->pr_protocol != RAWPROTO_SNOOP) {
			error = EOPNOTSUPP;
			break;
		}
		port = sotorawsa(so)->sr_port;
		if (rif->rif_errport && rif->rif_errport != port) {
			error = EBUSY;
			break;
		}
		flags = *(int *)data & ~SN_PROMISC;
		rif->rif_errsnoop = flags;
		rif->rif_errport = (flags == 0) ? 0 : port;
		break;
	  }

	  case SIOCDRAINMAP:
		if (so->so_proto->pr_protocol != RAWPROTO_DRAIN) {
			error = EOPNOTSUPP;
			break;
		}
		rif->rif_drainmap = *(struct drainmap *)data;
		break;

	  default: {
		struct ifnet *ifp;

		ifp = rif->rif_ifp;
		if (ifp->if_ioctl == 0) {
			error = EOPNOTSUPP;
			break;
		}
		error = (*ifp->if_ioctl)(ifp, cmd, data);
	  }
	}
	IFNET_UNLOCK(rif->rif_ifp);
	return(error);
}

/*
 * The message chain m0 begins with a link-layer header.  Therefore we
 * must increase if_mtu by rif_hdrlen when checking for overlong messages.
 * If an if_output routine uses if_mtu to bound transmitted data length,
 * then it should allow for the link-layer header in the AF_RAW case.
 */
int
raw_output(struct mbuf *m0, struct socket *so)
{
	struct rawif *rif;
	int len, rem;
	struct mbuf *m;
	struct ifnet *ifp;
	int error;

	ASSERT(SOCKET_ISLOCKED(so));
	rif = sotorawif(so);
	if (rif == 0) {
		struct sockaddr_raw zerosr;
		int error;

		bzero((caddr_t) &zerosr, sizeof zerosr);
		/* XXX huy doesn't use the new found port#? */
		RAWCBLIST_MRWLOCK();
		error = rawopen(sotorawcb(so), &zerosr);
		RAWCBLIST_MRUNLOCK();
		if (error)
			return error;
		rif = sotorawif(so);
	}
	len = 0;
	for (m = m0; m; m = m->m_next)
		len += m->m_len;
	ifp = rif->rif_ifp;
	rem = rif->rif_hdrlen + ifp->if_mtu - len;
	if (rem < 0)
		m_adj(m0, rem);
	/* huy XXX check for loif? */
	/* work around looutput() potentially changing the address family */
#ifdef _HAVE_SA_LEN
	rawdst0.sa_len = sizeof(rawdst0);
	rawdst0.sa_family = AF_RAW;
#else
	rawdst0.sa_family = AF_RAW;
#endif
	IFNET_UPPERLOCK(ifp);
	error = (*ifp->if_output)(ifp, m0, &rawdst0, NULL);
	IFNET_UPPERUNLOCK(ifp);
	return error;
}

/*
 * Translate device error state into snoop header flags.
 */
int
snoop_mapflags(struct snoopflagmap *sfm, int devflags)
{
	int snoopflags;

	for (snoopflags = SN_ERROR; sfm->sfm_devkey; sfm++) {
		if (sfm->sfm_devkey & devflags)
			snoopflags |= sfm->sfm_snoopval;
	}
	return snoopflags;
}

/*
 * Match len bytes of packet data beginning at hp against rif's filters.
 * NB: (hp - rif->rif_hdrpad) must be addressable as a long int vector.
 */
struct snoopfilter *
snoop_match(struct rawif *rif, caddr_t hp, int len)
{
	struct snoopfilter *sf;
	int i;

	ASSERT(IFNET_ISLOCKED(rif->rif_ifp));
	hp -= rif->rif_hdrpad;
	len >>= RAW_ALIGNSHIFT;
	len += rif->rif_hdrgrains;
	if (len > SNOOP_FILTERLEN)
		len = SNOOP_FILTERLEN;

	i = rif->rif_sfveclen;
	for (sf = rif->rif_sfvec; --i >= 0; sf++) {
		if (!sf->sf_active)
			continue;
		if (sfmatch(sf, (__uint32_t *) hp, len)) {
			if ((rif->rif_ifp->if_flags & IFF_DRVRLOCK) == 0)
				rif->rif_matched = sf;
			return sf;
		}
	}
	return NULL;
}

/*
 * Snoop protocol input.
 *
 * The mbuf pointed to by m contains at least the ifqueue, snoop, and raw
 * headers.  The ifh_hdrlen member of struct ifheader tells the total bytes
 * allocated to these headers in m.  There may be alignment padding between
 * the snoop header and hp, which points to at most SNOOP_FILTERLEN words
 * of link layer packet.  Len tells the packet length on the wire excluding
 * header and CRC.
 */

/* New drivers are encouraged to use snoop_input2() rather than
 * snoop_input(). Use the return value of snoop_match() to pass
 * down to snoop_input2().
 */

int 
snoop_input(struct rawif *rif, int snoopflags, caddr_t hp,
	    struct mbuf *m, int len) 
{
        int rv;
	rv = snoop_input2(rif, snoopflags, hp, m, len, rif->rif_matched);
	rif->rif_matched = 0;
	return rv;
}

int
snoop_input2(struct rawif *rif, int snoopflags, caddr_t hp,
	    struct mbuf *m, int len, struct snoopfilter *sfm)
{
	int maxlen, hdrlen, hdroff;
	struct ifheader *ifh;
	struct snoopheader *sh;
	struct mbuf *n;
	struct sockaddr_raw rawsrc = rawsrc0;
	struct sockaddr_raw rawdst = rif->rif_name;
	struct sockproto rawproto = rawproto0;

	ASSERT(IFNET_ISLOCKED(rif->rif_ifp));

	/*
	 * If we just don't want this packet, drop it without complaint
	 */
	if (!(rif->rif_errsnoop & snoopflags)
	    && sfm == 0 && !(sfm = snoop_match(rif, hp, len))) {
		if (snoopflags)
			m_freem(m);
		return 1;
	}
	/*
	 * Trim or copy snoop data.  If snoopflags, we can adjust m to skip
	 * stuff before the snoopheader, and steal the chain.
	 */
	maxlen = rif->rif_snooplen;
	hdrlen = sizeof *sh + rif->rif_padhdrlen;
	ifh = mtod(m, struct ifheader *);
	hdroff = ifh->ifh_hdrlen - hdrlen;
	if (m->m_len < ifh->ifh_hdrlen || hdroff < sizeof *ifh)
		panic("snoop_input");
	if (snoopflags) {
		M_ADJ(m, hdroff);
		if (len > maxlen)
			m_adj(m, maxlen - len);
	} else {
		int totlen, dupoff;

		/*
		 * Alas, we must really copy enough of the chain to avoid
		 * sharing the front of m's memory, over which IP scribbles
		 * fragment reassembly linkage and the pseudoheader.
		 */
		n = m_get(M_DONTWAIT, MT_DATA);
		if (n == 0) {
			snoop_drop2(rif, snoopflags, hp, len, sfm);
			return 0;
		}
		totlen = hdrlen + MIN(len, maxlen);
		dupoff = m_datacopy(m, hdroff, MIN(totlen, MLEN),
				    mtod(n, caddr_t));
		n->m_len = dupoff;
		totlen -= dupoff;
		if (totlen > 0) {
			n->m_next = m_copy(m, hdroff + dupoff, totlen);
			if (n->m_next == 0) {
				m_free(n);
				snoop_drop2(rif, snoopflags, hp, len, sfm);
				return 0;
			}
		}
		m = n;
	}

	/*
	 * Make a snoop header after the ifqueue header and in front
	 * of the padded raw header.
	 */
	sh = mtod(m, struct snoopheader *);
	sh->snoop_seq = rif->rif_stats.rs_snoop.ss_seq;
	sh->snoop_flags = snoopflags;
	sh->snoop_packetlen = rif->rif_hdrlen + len;
#if _MIPS_SIM == _ABI64
	irix5_microtime(&sh->snoop_timestamp);
#else
	microtime(&sh->snoop_timestamp);
#endif

	/*
	 * Set up foreign address needed for calls to raw_input.
	 */
	rawproto.sp_protocol = RAWPROTO_SNOOP;
	bcopy(hp + rif->rif_srcoff, rawsrc.sr_addr, rif->rif_addrlen);
	maxlen = RAW_MAXADDRLEN - rif->rif_addrlen;
	if (maxlen > 0)
		bzero(rawsrc.sr_addr + rif->rif_addrlen, maxlen);


	/*
	 * If the packet is erroneous, give it to the error snooper only.
	 * Otherwise give it to all matching sockets.
	 */
	if (snoopflags & SN_ERROR) {
		rawdst.sr_port = rif->rif_errport;
		if (raw_input(m, &rawproto, (struct sockaddr *)&rawsrc,
			      (struct sockaddr *)&rawdst,
			      rif->rif_ifp))
			rif->rif_stats.rs_snoop.ss_ifdrops++;
	} else {
		struct snoopfilter *sf;
		u_int portset, portbit;

		ASSERT(sfm);
		hdrlen = 0;
		sf = &rif->rif_sfvec[SNOOP_MAXFILTERS];
		portset = 0;
		while (--sf >= sfm) {
			/*
			 * Don't deliver duplicate packets to a socket.
			 */
			portbit = 1 << (sf->sf_port - SNOOP_MINPORT);
			if (portset & portbit)
				continue;

			/*
			 * If only one filter matched, deliver m.  Otherwise,
			 * verify a second match and copy m.
			 */
			if (sf == sfm) {
				n = m;
				sh->snoop_flags |= sf->sf_index;
			} else {
				if (!sf->sf_active)
					continue;
				/*
				 * Adjust hp and compute hdrlen only if there
				 * might be more than one match.
				 */
				if (hdrlen == 0) {
					hp -= rif->rif_hdrpad;
					hdrlen = rif->rif_hdrgrains
						 + (len >> RAW_ALIGNSHIFT);
					if (hdrlen > SNOOP_FILTERLEN)
						hdrlen = SNOOP_FILTERLEN;
				}
				if (!sfmatch(sf, (__uint32_t *)hp, hdrlen))
					continue;
				n = m_copy(m, 0, M_COPYALL);
				if (n == 0) {
					rif->rif_stats.rs_snoop.ss_ifdrops++;
					continue;
				}
				mtod(n, struct snoopheader *)->snoop_flags
					|= sf->sf_index;
			}

			/*
			 * Set the local address's port to sf's port and
			 * deliver the packet.
			 */
			rawdst.sr_port = sf->sf_port;
			
			if (raw_input(n, &rawproto, (struct sockaddr *)&rawsrc,
				      (struct sockaddr *)&rawdst,
				      rif->rif_ifp))
				rif->rif_stats.rs_snoop.ss_ifdrops++;
			portset |= portbit;
		}
	}

	rif->rif_stats.rs_snoop.ss_seq++;
	return 1;
}

/*
 * Record a drop at or below the raw interrupt queue.
 */
void
snoop_drop(struct rawif *rif, int snoopflags, caddr_t hp, int len)
{
	snoop_drop2(rif, snoopflags, hp, len, rif->rif_matched);
}
	
void
snoop_drop2(struct rawif *rif, int snoopflags, caddr_t hp, int len, struct snoopfilter *sf)
{
	ASSERT(IFNET_ISLOCKED(rif->rif_ifp));
	if (!(rif->rif_errsnoop & snoopflags)
	    && sf == 0 && !snoop_match(rif, hp, len)) {
		return;
	}
	rif->rif_stats.rs_snoop.ss_seq++;
	rif->rif_stats.rs_snoop.ss_ifdrops++;
}

/*
 * Drain protocol input.
 */
int
drain_input(struct rawif *rif, u_int port, caddr_t srcaddr, struct mbuf *m)
{
	int len, rem;
	struct ifheader *ifh;
	struct sockaddr_raw rawsrc = { AF_RAW };
	struct sockproto rawproto = { PF_RAW };

	ASSERT(IFNET_ISLOCKED(rif->rif_ifp));
	rawproto.sp_protocol = RAWPROTO_DRAIN;
	len = rif->rif_addrlen;
	bcopy(srcaddr, rawsrc.sr_addr, len);
	rem = RAW_MAXADDRLEN - len;
	if (rem > 0)
		bzero(rawsrc.sr_addr + len, rem);
	if (rif->rif_drainmap.dm_minport <= port
	    && port <= rif->rif_drainmap.dm_maxport) {
		port = rif->rif_drainmap.dm_toport;
	}
	rif->rif_name.sr_port = port;
	ifh = mtod(m, struct ifheader *);
	rem = ifh->ifh_hdrlen - rif->rif_padhdrlen;
	if (m->m_len < ifh->ifh_hdrlen || rem < sizeof *ifh)
		panic("drain_input");
	M_ADJ(m, rem);
	if (raw_input(m, &rawproto, (struct sockaddr *)&rawsrc,
		      (struct sockaddr *)&rif->rif_name, rif->rif_ifp)) {
		drain_drop(rif, port);
		return 0;
	}
	return 1;
}

/*
 * Notice a dropped packet destined for a drain port.
 */
/* ARGSUSED */
void
drain_drop(struct rawif *rif, u_int port)
{
	ASSERT(IFNET_ISLOCKED(rif->rif_ifp));
	rif->rif_stats.rs_drain.ds_ifdrops++;
}
