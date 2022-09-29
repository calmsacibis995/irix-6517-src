#ifndef ETHER_H
#define ETHER_H
/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Network interface for Ethernet.
 *
 * The etherif struct contains the following state:
 *	- an ifnet struct, containing generic operations pointers
 *	- common arp information which extends the ifnet struct, including
 *	  the current Ethernet address of this device
 *	- a pointer to device driver operations called by the generic ifnet
 *	  operations set up when an etherif is attached
 *	- the official Ethernet address of this device
 *	- a multicast type/destination filter for IFF_ALLMULTI mode
 *	- a raw domain interface struct for network monitoring
 *	- common flags and counters
 *
 * The ether_* functions maintain all of the above state except these
 * members of struct ifnet:
 *	- if_timer, maintained by device watchdog and interrupt code
 *	- if_metric and if_addrlist, maintained by generic ifnet code
 *	- if_snd, the deferred output queue, optionally used by drivers
 *	- if_ipackets, incremented by the driver when any packet is received
 *	- if_collisions, which the device driver must maintain
 * Drivers may need to increment if_ierrors and if_oerrors if they detect
 * errors but cannot call ether_input to receive a good packet.
 *
 * The Ethernet device driver must implement these functions:
 *	init(eif, flags)
 *		Given a struct etherif pointer and interface flags, the init
 *		routine sets reception mode (e.g. promiscuity) from flags,
 *		sets its address from eif_arpcom.ac_enaddr, sets any address
 *		filters from copies saved in driver private data, and starts
 *		the device.
 *
 *	reset(eif)
 *		Hardware reset, used to clear errors and start an interface
 *		which has been taken down (~IFF_UP).
 *
 *	watchdog(void *)
 *		Identical to struct ifnet's watchdog procedure.  Check unit
 *		and reset it if wedged, accounting for lost interrupts with
 *		eif_lostintrs.  Set if_timer appropriately.
 *
 *	transmit(eif, dhost, shost, type, m)
 *		Encapsulate the data in mbuf chain m with an Ethernet
 *		header formed by struct etheraddr pointers dhost and shost
 *		and u_short type (type is in network byte order).  Transmit
 *		disposes of m using m_freem unless it incurs a transmit error,
 *		in which case it returns an error number and leaves m alone.
 *		If the device cannot hear its own traffic, then its transmit
 *		routine should use ether_selfsnoop to monitor transmissions.
 *
 *	ioctl(eif, cmd, data)
 *		Perform device-specific i/o control operations.  Among
 *		the commands each device should support are SIOCADDMULTI
 *		and SIOCDELMULTI.  These commands take a struct mfreq
 *		(see below) rather than an mkey union.
 *
 * All but watchdog are called at splimp.  In addition to implementing
 * these operations, a driver must initialize its etherif structure with
 * ether_attach, and should provide an interrupt handler which calls
 * ether_input (see below).
 */
#ident "$Revision: 1.15 $"

#include "net/if.h"
#include "net/multi.h"
#include "net/raw.h"
#include "netinet/if_ether.h"

/*
 * Multicast filter request for struct etherif's ioctl operation.
 */
struct mfreq {
	union mkey	*mfr_key;	/* pointer to socket ioctl arg */
	mval_t		mfr_value;	/* associated value */
};

/*
 * Ethernet input packets enqueued on protocol interrupt queues begin
 * with a struct etherbufhead.  This structure contains an ifnet pointer,
 * raw domain information, and the packet's Ethernet header.
 */
#define	ETHERHDRPAD	RAW_HDRPAD(sizeof(struct ether_header))

struct etherbufhead {
	struct ifheader		ebh_ifh;
	struct snoopheader	ebh_snoop;
	char			ebh_pad[ETHERHDRPAD];
	struct ether_header	ebh_ether;
};

/*
 * Ethernet trailer minimum grain.
 */
#define	ETHERTRAILERLOG	9
#define	ETHERTRAILERMIN	(1<<ETHERTRAILERLOG)
#define	ETHERTRAILERMOD	(ETHERTRAILERMIN-1)

/*
 * Ethernet address structure, for by-value semantics.
 */
#define	ETHERADDRLEN	(sizeof ((struct ether_header *) 0)->ether_dhost)

struct etheraddr {
	u_char	ea_vec[ETHERADDRLEN];
};

/*
 * Ethernet network interface struct.
 * NB: eif_arpcom must be the first member for ifptoeif() to work.
 */
struct etherif {
	struct arpcom		eif_arpcom;	/* ifnet and current address */
	caddr_t			eif_private;	/* device driver private data */
	struct etherifops	*eif_ops;	/* device driver operations */
	struct etheraddr	eif_addr;	/* official address */
	struct mfilter		eif_mfilter;	/* multicast filter */
	struct rawif		eif_rawif;	/* raw domain interface */
	uint			eif_resets;	/* times device was reset */
	u_short			eif_lostintrs;	/* lost interrupts */
	short			eif_sick;	/* how sick is this device */
#ifdef INET6
	struct  in6_addr 	eif_llip6;      /* link-local address */
#endif
};

#define	ifptoeif(ifp)		((struct etherif *) (ifp))
#define	eiftoifp(eif)		(&(eif)->eif_arpcom.ac_if)

#define	IFF_ALIVE		(IFF_UP|IFF_RUNNING)
#define	iff_alive(flags)	(((flags) & IFF_ALIVE) == IFF_ALIVE)
#define iff_dead(flags)		(((flags) & IFF_ALIVE) != IFF_ALIVE)

struct etherifops {
#define	EIF	struct etherif *
#define	EAP	struct etheraddr *
	int	(*eio_init)(EIF, int);		/* initialize device */
	void	(*eio_reset)(EIF);		/* reset hardware */
	void	(*eio_watchdog)(struct ifnet *);/* watchdog routine */
	int	(*eio_transmit)(EIF, EAP, EAP, u_short, struct mbuf*);
						/* transmit from mbufs */
	int	(*eio_ioctl)(EIF, int, void*);	/* i/o control */
#undef EIF
#undef EAP
};

#define	eif_init(eif, flags) \
	((*(eif)->eif_ops->eio_init)(eif, flags))
#define	eif_reset(eif) \
	((*(eif)->eif_ops->eio_reset)(eif))
#define	eif_transmit(eif, dhost, shost, type, m) \
	((*(eif)->eif_ops->eio_transmit)(eif, dhost, shost, type, m))
#define	eif_ioctl(eif, cmd, data) \
	((*(eif)->eif_ops->eio_ioctl)(eif, cmd, data))

/*
 * Call ether_attach thus to set up common Ethernet interface state,
 * assuming the etherif struct is allocated as a member of a per-unit
 * device information structure (the eninfo vector):
 *
 *	ether_attach(&eninfo[unit].etherif, "en", unit,
 *		     (caddr_t) &eninfo[unit], &enops, &enaddr,
 *		     INV_ETHER_EN, 0);
 *
 * This example shows how an interface named "en", of inventory controller
 * type INV_ETHER_EN, and with official Ethernet address stored in enaddr,
 * would attach its etherif struct.  The etherif's private data pointer is
 * set to &eninfo[unit], and its operations are fetched from the statically
 * initialized struct etherifops, enops.  The last argument is the hardware
 * inventory state of this device.
 */
void	ether_attach(struct etherif *, char *, int,
		     caddr_t, struct etherifops *, struct etheraddr *,
		     int, int);

/*
 * ether_detach() and ether_reattach() are for loadable ifnet
 * driver support.  ether_detach() quiesces the etherif and
 * associated ifnet structures prior to driver unload.
 * The etherif and ifnet structures *are not unloaded*.
 * The driver is responsible for detecting subsequent reload,
 * initializing any other driver-private data, and calling
 * ether_reattach() instead of ether_attach().
 */ 
void	ether_detach(struct etherif *);
void	ether_reattach(struct etherif *,
		       caddr_t, struct etherifops *);


/*
 * On receive interrupt, the driver maps any errors into snoopflags and
 * calls this function:
 *
 *	ether_input(&eninfo[unit].etherif, snoopflags, m)
 *
 * The m argument points to an mbuf cluster containing an etherbufhead and
 * the packet data as it came off the wire.
 */
void	ether_input(struct etherif *, int, struct mbuf *);

/*
 * Call this function from eif_transmit if the Ethernet hardware cannot hear
 * itself when receiving promiscuously.  The eif_transmit code should first
 * check for promiscuous snooping and a snoop match:
 *
 *	if (RAWIF_SNOOPING(&eif->eif_rawif)
 *	    && snoop_match(&eif->eif_rawif, (caddr_t) eh, len)) {
 *		ether_selfsnoop(eif, eh, m, off, len);
 *	}
 *
 * See net/raw.h for information about snoop_match.  The off argument to
 * ether_selfsnoop tells the number of bytes from mtod(m, caddr_t) to the
 * start of the Ethernet packet's data.  For some drivers off will be zero
 * (i.e. the mbuf chain m is the same as that passed to if_output), while
 * others may wish to adjust or copy m, to prepend an Ethernet header and
 * to meet minimum gather requirements.
 */
void	ether_selfsnoop(struct etherif *, struct ether_header *,
			struct mbuf *, int, int);

/*
 * To set interface flags when going down, notify protocols that we are
 * no longer up, and call eif_reset, call this function:
 *
 *	ether_stop(&eninfo[unit].etherif, flags)
 *
 * The flags argument contains if_flag bits defined in net/if.h.
 */
void	ether_stop(struct etherif *, int);

#endif
