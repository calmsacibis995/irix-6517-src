#ifndef __net_raw__
#define __net_raw__
/*
 * Constants and structures for the raw protocol family.
 *
 * $Revision: 1.2 $
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
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
/* Trusted IRIX */
#include <sys/mac_label.h>

struct mbuf;
struct socket;
struct rawcb;

/*
 * There are two protocols, snoop and drain.
 *
 * The snoop protocol captures link-layer packets which match a bitfield
 * filter and transports them to that filter's socket.  Snoop prepends a
 * header containing packet state, sequence, and a timestamp.  The drain
 * protocol receives input packets with network-layer type codes selected
 * from among encapsulations not implemented by the kernel.
 *
 * Snoop enables promiscuous reception to capture packets not destined
 * for this host if an active snoop filter matches such packets, while
 * drain receives non-promiscuously (in the absence of a snooper on the
 * same network interface), filtering packets received by the hardware
 * according to their type codes.
 *
 * Both protocols transmit packets with a link-layer header fetched from
 * the beginning of the user's write buffer.  They ignore any destination
 * address supplied to sendto or connect.  However, connecting restricts
 * input to packets originating from the connected address.  Therefore,
 * write(2) is the likeliest system call to use for raw output.
 */
#define	RAWPROTO_RAW	0	/* wild-card, defaults to snoop */
#define	RAWPROTO_DRAIN	1
#define	RAWPROTO_SNOOP	2
#define	RAWPROTO_MAX	3	/* actually, number of protocols */

/*
 * Raw family address format differs between local and foreign usage.
 * A local AF_RAW sockaddr contains a port, identifying the socket to
 * which this address is bound, and the zero-padded name of a network
 * interface.  A foreign AF_RAW sockaddr contains an opaque link-layer
 * destination address.
 */
#define	RAW_MAXADDRLEN	(sizeof ((struct sockaddr *) 0)->sa_data)
#define	RAW_IFNAMSIZ	(RAW_MAXADDRLEN - sizeof(u_short))

struct sockaddr_raw {
	u_short		sr_family;			/* AF_RAW */
	union {
	    struct {
		u_short srl_port;			/* socket id */
		char	srl_ifname[RAW_IFNAMSIZ];	/* interface name */
	    } sru_local;
	    struct {
		char	srf_addr[RAW_MAXADDRLEN];	/* raw address */
	    } sru_foreign;
	} sr_u;
};

#define	sr_port		sr_u.sru_local.srl_port
#define	sr_ifname	sr_u.sru_local.srl_ifname
#define	sr_addr		sr_u.sru_foreign.srf_addr

/*
 * All raw domain input protocols guarantee that the offset of packet
 * data from the beginning of the user's read buffer is congruent with
 * RAW_ALIGNGRAIN.
 */
#define	RAW_ALIGNSHIFT	2
#define	RAW_ALIGNGRAIN	(1<<RAW_ALIGNSHIFT)
#define	RAW_ALIGNMASK	(RAW_ALIGNGRAIN-1)

/*
 * RAW_HDRPAD yields the byte-padding needed to ensure that the end of the
 * link-layer header is aligned.  RAW_BUFPAD tells how much space to reserve
 * in input buffers after the ifqueue header and before the link-layer header.
 * Given a RAW_ALIGNGRAIN-congruent address and a header type, RAW_HDR returns
 * the header pointer.
 */
#ifdef lint
#define	RAW_HDRPAD(hdrsize) (hdrsize)
#else
#define	RAW_HDRPAD(hdrsize) \
	((hdrsize & RAW_ALIGNMASK) == 0 ? 0 \
	 : RAW_ALIGNGRAIN - ((hdrsize) & RAW_ALIGNMASK))
#endif
#define	RAW_BUFPAD(hdrsize) \
	(sizeof(struct snoopheader) + RAW_HDRPAD(hdrsize))
#define	RAW_HDR(p, hdrtype) \
	((hdrtype *) ((caddr_t)(p) + RAW_HDRPAD(sizeof(hdrtype))))

/*
 * The snoop protocol filters at most 36 bytes of packet header by masking
 * "don't-care" bits and matching the interesting ones.  The mask and match
 * are done using long ints for efficiency.  Filter bytes must be justified
 * so that the link-layer header ends on a RAW_ALIGNGRAIN boundary.
 */
#define	SNOOP_FILTERLEN		9

/*
 * There may be up to four snoop filters per interface.  Snoop allocates
 * filters using a linear first-free search.
 */
#define	SNOOP_MAXFILTSHIFT	2
#define	SNOOP_MAXFILTERS	(1<<SNOOP_MAXFILTSHIFT)
#define	SNOOP_MAXFILTMASK	(SNOOP_MAXFILTERS-1)
#define	SNOOP_MINPORT		1
#define	SNOOP_MAXPORT		4

struct snoopfilter {
	u_long	sf_mask[SNOOP_FILTERLEN];	/* which header bits to match */
	u_long	sf_match[SNOOP_FILTERLEN];	/* and the matching values */
	u_short	sf_allocated:1,			/* whether filter is in use */
		sf_active:1,			/* whether filter is "on" */
		sf_promisc:1,			/* is filter promiscuous? */
		sf_allmulti:1,			/* or logical-promiscuous? */
		sf_index:SNOOP_MAXFILTSHIFT;	/* index in filter table */
	u_short	sf_port;			/* socket owning this filter */
};

/*
 * Snoop applies filters to a packet in order, stopping on the first match.
 * It then prepends this header to the alignment-padded link-layer header.
 * A snoopheader contains a reception sequence number, error/filter flags,
 * length of link-layer packet excluding frame check, preamble, etc., and
 * a reception timestamp.
 */
struct snoopheader {
	u_long		snoop_seq;		/* sequence number per ifnet */
	u_short		snoop_flags;		/* packet flags - see below */
	u_short		snoop_packetlen;	/* packet length on wire */
	struct timeval	snoop_timestamp;	/* time of receive interrupt */
};

/* snoop header flags */
#define	SN_PROMISC	0x8000	/* pkt not destined for this interface */
#define	SN_ERROR	0x4000	/* receive error specified by SNERR_* flags */
#define	SN_TRAILER	0x2000	/* network-layer protocol trailer prepended */
#define	SNERR_FRAME	0x0001	/* pkt received, but with framing error */
#define	SNERR_CHECKSUM	0x0002	/* pkt received, but with CRC error */
#define	SNERR_TOOBIG	0x0004	/* pkt received, truncated to fit buffer */
#define	SNERR_TOOSMALL	0x0008	/* pkt may be received, buffer < minimum */
#define	SNERR_NOBUFS	0x0010	/* no pkt received, out of buffers */
#define	SNERR_OVERFLOW	0x0020	/* no pkt received, input silo overflow */
#define	SNERR_MEMORY	0x0040	/* no pkt received, memory error */

/*
 * Mask to extract matching filter index from snoop_flags if !SN_ERROR.
 */
#define	SN_FILTINDEX	SNOOP_MAXFILTMASK

/*
 * Pseudo-header prepended to network-layer packets snooped from the
 * loopback interface, which lack a snoopable link-layer header that
 * contains a network-layer type discriminant.
 */
struct loopheader {
	u_short	loop_family;	/* address family, e.g. AF_INET */
	u_short	loop_spare;	/* spare member for padding */
};

/*
 * Drain input is demultiplexed based on a 16-bit port, which for Ethernet
 * is the ether_type.  A range of types may be mapped to one port by setting
 * this structure in a raw interface with SIOCDRAINMAP.
 */
struct drainmap {
	u_short	dm_minport;	/* lowest port in mapped range */
	u_short	dm_maxport;	/* and highest port */
	u_short	dm_toport;	/* port to which range maps */
};

/*
 * Raw protocol statistics.
 */
struct snoopstats {
	u_long	ss_seq;		/* current sequence number */
	u_long	ss_ifdrops;	/* drops at/below interface queue */
	u_long	ss_sbdrops;	/* drops at socket buffer */
};

struct drainstats {
	u_long	ds_ifdrops;	/* drops at/below interface queue */
	u_long	ds_sbdrops;	/* drops at socket buffer */
};

struct rawstats {
	struct snoopstats rs_snoop;
	struct drainstats rs_drain;
};

/*
 * Raw domain ioctl commands.
 */
#ifdef sun
#define	SIOCRAWSTATS	_IOR(R, 1, struct rawstats)	/* get rawproto stats */
#define	SIOCADDSNOOP	_IOWR(R, 2, struct snoopfilter)/* add snoop filter */
#define	SIOCDELSNOOP	_IOW(R, 3, int)		/* delete filter by # */
#define	SIOCSNOOPLEN	_IOW(R, 4, int)		/* set snoop length */
#define	SIOCSNOOPING	_IOW(R, 5, int)		/* start/stop capture */
#define	SIOCERRSNOOP	_IOW(R, 6, int)		/* set error snooper */
#define	SIOCDRAINMAP	_IOW(R, 7, struct drainmap)	/* set drain portmap */
#else
#define	SIOCRAWSTATS	_IOR('R', 1, struct rawstats)	/* get rawproto stats */
#define	SIOCADDSNOOP	_IOWR('R', 2, struct snoopfilter)/* add snoop filter */
#define	SIOCDELSNOOP	_IOW('R', 3, int)		/* delete filter by # */
#define	SIOCSNOOPLEN	_IOW('R', 4, int)		/* set snoop length */
#define	SIOCSNOOPING	_IOW('R', 5, int)		/* start/stop capture */
#define	SIOCERRSNOOP	_IOW('R', 6, int)		/* set error snooper */
#define	SIOCDRAINMAP	_IOW('R', 7, struct drainmap)	/* set drain portmap */
#endif

#ifdef	_KERNEL
/*
 * The raw domain's network interface structure.
 */
struct rawif {
	/* statically configured information */
	struct rawif		*rif_next;	/* other rawifs */
	struct ifnet		*rif_ifp;	/* link to associated ifnet */
	struct sockaddr_raw	rif_name;	/* family and interface name */
	caddr_t			rif_addr;	/* ptr to link-layer address */
	caddr_t			rif_broadaddr;	/* broadcast address or null */
	u_char			rif_addrlen;	/* link-layer address length */
	u_char			rif_hdrlen;	/* bytes per link header */
	u_char			rif_hdrpad;	/* pad bytes before header */
	u_char			rif_padhdrlen;	/* bytes per padded header */
	u_char			rif_hdrgrains;	/* long ints per link header */
	u_char			rif_srcoff;	/* source address offset */
	u_char			rif_dstoff;	/* destination offset */

	/* statistics and counters */
	struct rawstats		rif_stats;	/* pkt counters and stats */
	u_char			rif_open[RAWPROTO_MAX];
						/* open socket counts */

	/* snoop protocol state */
	u_short			rif_snooplen;	/* data length to capture */
	u_short			rif_errsnoop;	/* error flags to match */
	u_char			rif_errport;	/* port snooping for errors */
	u_char			rif_promisc;	/* promiscuous filter count */
	u_char			rif_allmulti;	/* all-multicast filter count */
	u_char			rif_sfveclen;	/* filter vector and length */
	struct snoopfilter	rif_sfvec[SNOOP_MAXFILTERS];
	struct snoopfilter	*rif_matched;	/* filter matching last pkt */

	/* drain protocol state */
	struct drainmap		rif_drainmap;	/* drain port mapping */
};

#ifdef CIPSO
/* Trusted IRIX - added two arguments. */
extern int raw_input(struct mbuf *m0, struct sockproto *proto,
		    struct sockaddr *src, struct sockaddr *dst,
		    struct mac_label * dlbl, uid_t duid);
#else
extern int raw_input(struct mbuf *m0, struct sockproto *proto,
		    struct sockaddr *src, struct sockaddr *dst);
#endif

/*
 * Macros to access private data pointers and blocks appropriately.
 */
#define	rptorawsa(rp)	((struct sockaddr_raw *) &(rp)->rcb_laddr)
#define	sotorawsa(so)	rptorawsa(sotorawcb(so))
#define	rptorawif(rp)	((struct rawif *) (rp)->rcb_pcb)
#define	sotorawif(so)	rptorawif(sotorawcb(so))

#define	structoff(s,m)	((char *) &((struct s *) 0)->m - (char *) 0)

/*
 * Macros to detect whether any snoopers or drainers are running.
 */
#define	RAWIF_SNOOPING(rif)	((rif)->rif_open[RAWPROTO_SNOOP])
#define	RAWIF_DRAINING(rif)	((rif)->rif_open[RAWPROTO_DRAIN])

/*
 * Raw interface and raw domain control block operations.
 *
 * The rawif_attach function is analogous to if_attach.  Here's how to call
 * it for Ethernet, assuming the interface device state structure is pointed
 * at by ei:
 *
 * 	rawif_attach(&ei->rawif, &ei->arpcom.ac_if,
 *		     ei->arpcom.ac_enaddr, etherbroadcastaddr,
 *		     sizeof ei->arpcom.ac_enaddr, sizeof(struct ether_header),
 *		     structoff(ether_header, ether_shost[0]),
 *		     structoff(ether_header, ether_dhost[0]));
 *
 * A point-to-point interface may pass null for its physical and broadcast
 * addresses, provided it passed zero for the address length.  An interface
 * may pass zero for the header length and address offsets, provided it
 * includes the link-layer header length in snoop_input's len argument.
 */
void	rawif_attach(struct rawif *, struct ifnet *, caddr_t, caddr_t, int,
		     int, int, int);
int	rawioctl(struct socket *, int, caddr_t);
int	rawopen(struct rawcb *, struct sockaddr_raw *);
void	rawclose(struct rawcb *);
void	rawsbdrop(struct rawcb *);
struct  rawif *rawif_lookup(const char *);


/*
 * The snoopflagmap struct maps device-dependent error bits to snoop flags.
 * Given a vector of these and the device bits, snoop_mapflags returns the
 * snoop_flags member of a snoopheader.  The vector should terminate with
 * an all-zero struct.  Snoop_mapflags adds SN_ERROR to the mapped flags to
 * indicate that bad packets are for the snooper only.
 *
 *	struct snoopflagmap errorflagmap[] = {
 *		{ DEVERR_FRAME,	SNERR_FRAME },
 *		. . .
 *		{ 0,		0 }
 *	};
 *	u_short errorflags, snoopflags;
 *
 *	errorflags = . . .;
 *	snoopflags = snoop_mapflags(errorflagmap, errorflags);
 */
struct snoopflagmap {
	u_short	sfm_devkey;		/* flag bits in device error flags */
	u_short	sfm_snoopval;		/* corresponding snoop flag value */
};

/*
 * Network interface receive code should test RAWIF_SNOOPING(&ei->rawif)
 * and call snoop_match, snoop_input, and snoop_drop.  Snoop_match tests
 * whether a promiscuous or erroneous packet should be passed to snoop_input.
 * Any packets received but not decapsulated should be noted with snoop_drop.
 * These routines are called thus:
 *
 *	snoop_match(&ei->rawif, hp, len);
 *	snoop_input(&ei->rawif, snoopflags, hp, m, len, [tp, tlen]);
 *	snoop_drop(&ei->rawif, snoopflags, hp, len);
 *
 * The hp argument should point at the link-layer header in a receive buffer.
 * Receive buffers must contain enough space for an ifheader, snoopheader,
 * possibly padding, and the link-layer header.  The mbuf pointed at by m
 * must contain at least the if and snoop headers.  The len argument tells
 * the number of data bytes in the packet; it does not include the link-layer
 * header length.
 *
 * If snoopflags is non-zero, snoop_input consumes m, and its caller should
 * avoid normal input processing code.  If snoopflags is zero, then normal
 * packet demultiplexing should be done.  Snoop_input returns 0 if it can't
 * allocate mbufs to copy the packet when snoopflags is zero, or if it can't
 * enqueue the packet on the raw input queue; otherwise it returns 1.
 */
int	snoop_mapflags(struct snoopflagmap *, int);
int	snoop_match(struct rawif *, caddr_t, int);
int	snoop_input(struct rawif *, int, caddr_t, struct mbuf *, int);
void	snoop_drop(struct rawif *, int, caddr_t, int);


/*
 * Call drain_input to pass packets of unknown type to drain sockets thus:
 *
 *	drain_input(&ei->rawif, port, srcaddr, m);
 *
 * Port should name a port associated with the unknown packet type or types;
 * srcaddr is the link-layer source address in the packet; m is the same as
 * for snoop_input, but drain_input always consumes m.  It returns 0 if the
 * raw input queue is full, and 1 otherwise.
 *
 * Call drain_drop if RAWIF_DRAINING and a packet of unknown type cannot be
 * decapsulated.
 *
 *	drain_drop(&ei->rawif, port);
 */
int	drain_input(struct rawif *, u_int, caddr_t, struct mbuf *);
void	drain_drop(struct rawif *, u_int);

#endif	/* _KERNEL */


/* Addresses used by DLPI when sending via if_ drivers */
struct sockaddr_sdl {
	u_short ssdl_family;		/* AF_SDL (Sgi Data Link) */
	u_short ssdl_addr_len;
	u_long  ssdl_macpri;
	u_long  ssdl_ethersap;
	u_char  ssdl_addr[32];
};

#endif	/* !__net_raw__ */
