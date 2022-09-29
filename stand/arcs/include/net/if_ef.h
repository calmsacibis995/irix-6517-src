/*
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

/*
 * IOC3 10/100 Mbps Fast Ethernet Device Driver definitions
 */

#ident "$Header: /proj/irix6.5.7m/isms/stand/arcs/include/net/RCS/if_ef.h,v 1.8 1999/02/10 23:15:48 paulson Exp $"

#ifndef __IF_EF_H__
#define __IF_EF_H__

#include <sys/mii.h>

/*
 * RAW_HDRPAD yields the byte-padding needed to ensure that the end of the
 * link-layer header is aligned.  RAW_BUFPAD tells how much space to reserve
 * in input buffers after the ifqueue header and before the link-layer header.
 * Given a RAW_ALIGNGRAIN-congruent address and a header type, RAW_HDR returns
 * the header pointer.
 */
#define	RAW_ALIGNSHIFT	2
#define	RAW_ALIGNGRAIN	(1<<RAW_ALIGNSHIFT)
#define	RAW_ALIGNMASK	(RAW_ALIGNGRAIN-1)
#define	RAW_HDRPAD(hdrsize) \
	((hdrsize & RAW_ALIGNMASK) == 0 ? 0 \
	 : RAW_ALIGNGRAIN - ((hdrsize) & RAW_ALIGNMASK))

/*
 * Ethernet input packets enqueued on protocol interrupt queues begin
 * with a struct etherbufhead.  This structure contains an ifnet pointer,
 * raw domain information, and the packet's Ethernet header.
 */
#define	ETHERHDRPAD	RAW_HDRPAD(sizeof(struct ether_header))
#define IFHDRPAD        8
#define SNOOPPAD        16

struct etherbufhead {
        char		        ebh_ifh[IFHDRPAD];     /* not used */
	char	                ebh_snoop[SNOOPPAD];   /* not used */
	char			ebh_pad[ETHERHDRPAD];  /* not used */
	struct ether_header	ebh_ether;
};

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
	struct etheraddr	eif_addr;	/* official address */
};


/*
 * IOC3 10/100 Mbit Fast Ethernet device driver
 */

/*
 * RX buffer
 */
#define	EFRDATASZ	(2048 - sizeof (struct ioc3_erxbuf) \
			 - sizeof(struct etherbufhead))
struct efrxbuf {
	struct ioc3_erxbuf	rb_rxb;
	struct etherbufhead	rb_ebh;
	u_char			rb_data[EFRDATASZ];
};

/*
 * TX descriptor
 */
#define	TXDATASZ	88
struct eftxd {
	__uint32_t	cmd;
	__uint32_t	bufcnt;
	__uint64_t	p1;
	__uint64_t	p2;
	struct ether_header	eh;
	u_char		data[TXDATASZ];
};

/*
 * Principle (per-interface) structure.
 */
struct ef_info {
	struct etherif	ei_eif;			/* common ethernet interface */
	ioc3_eregs_t	*ei_regs;		/* ioc3 ether registers */
	ioc3_mem_t	*ei_ioc3regs;		/* ioc3 registers */

	int		ei_ntxd;		/* # tx descriptors */
	volatile struct eftxd	*ei_txd;	/* tx descriptor ring */
	int		ei_txhead;		/* next txd to post */
	int		ei_txtail;		/* next txd to reclaim */
	struct mbuf	**ei_txm;		/* tx mbufs pending */
	int		ei_linkdown;		/* T/F eisr link failure */

	int		ei_nrbuf;		/* # rbufs to keep posted */
	volatile __uint64_t	*ei_rxd;	/* rx descriptor ring */
	int		ei_rxhead;		/* next rxd to read */
	int		ei_rxtail;		/* next rxd to post */
	struct efrxbuf	*ei_rb;			/* quick ptr to head rbuf */
	int		ei_in_active;		/* input recursion preventer */
	struct mbuf	**ei_rxm;		/* posted rbufs */

        int             ei_chiprev;             /* ioc3 revision */
        int             ei_phytype;             /* phy type code */
        int             ei_phyunit;             /* phy addr */
        int             ei_phyrev;              /* phy revision */

	int		ei_speed100;		/* 0=10Mbps 1=100Mbps */
	int		ei_fullduplex;		/* 0=half 1=full */

	int		ei_erbar;		/* store into erbar reg */
	int		ei_ssram_bits;		/* emcr parity/bufsiz bits */

	int		ei_remfaulting;		/* T/F phy remote fault */
	int		ei_jabbering;		/* T/F phy jabber fault */

	int		ei_defers;		/* # cumulative defers */
	int		ei_runt;		/* # runt packets */
	int		ei_crc;			/* # crc errors */
	int		ei_code;		/* # coding error */
	int		ei_giant;		/* # giant pkt errors */
	int		ei_drop;		/* # dropped input packets */
	int		ei_ssramoflo;		/* # ssram overflow */
	int		ei_ssramuflo;		/* # ssram underflow */
	int		ei_memerr;		/* # rx/tx memory error */
	int		ei_parityerr;		/* # rx/tx parity error */
	int		ei_rtry;		/* # tx retry errors */
	int		ei_exdefer;		/* # excessive defers */
	int		ei_lcol;		/* # late collisions */
	int		ei_txgiant;		/* # tx giant packets */
	volatile __uint32_t	*ei_ssram;	/* diag access to ssram */
};

#define	ei_ac		ei_eif.eif_arpcom	/* common arp stuff */
#define	ei_if		ei_ac.ac_if		/* ifnet */
#define	ei_unit		ei_if.if_unit		/* logical unit # */
#define	ei_curaddr	ei_ac.ac_enaddr		/* current ether addr */
#define	ei_addr		ei_eif.eif_addr		/* i/f official addr */
#define	ei_rawif	ei_eif.eif_rawif	/* raw domain interface */
#define	eiftoei(eif)	((struct ef_info*)(eif)->eif_private)

#define	TXDSZ	(sizeof (struct eftxd))
#define	RXDSZ	(sizeof (__uint64_t))

#define	NTXDSML	128			/* # txd desktop configuration */
#define	NTXDBIG	512			/* # txd server configuration */
#define	NRXD	512			/* # rxd */

#define	GIANT_PACKET_SIZE	1536

/* PHY defines */
#define PHY_QS6612X                     0x0181440       /* Quality TX */
#define PHY_ICS1890                     0x0015F40       /* ICS TX */
#define PHY_ICS1892                     0x0015F41       /* ICS TX */
#define PHY_DP83840                     0x20005C0       /* National TX */

#define ICS1890_R0_LOOPBACK     0x4000
#define ICS1890_R17_SPEED_100   0x8000
#define ICS1890_R17_FULL_DUPLEX 0x4000
#define ICS1890_R17_PD_MASK     0x3800
#define ICS1890_R17_PD_FAULT    0x1000

#define DP83840_R25_SPEED_10    0x0040

#define QS6612X_R31_OPMODE_MASK 0x001c
#define QS6612X_R31_10          0x0004
#define QS6612X_R31_100         0x0008
#define QS6612X_R31_10FD        0x0014
#define QS6612X_R31_100FD       0x0018

#endif /* __IF_EF_H__ */



