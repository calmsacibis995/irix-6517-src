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

#ident "$Header: /proj/irix6.5.7m/isms/irix/kern/bsd/sys/RCS/if_ef.h,v 1.24 1999/02/10 23:15:43 paulson Exp $"

#ifndef __IF_EF_H__
#define __IF_EF_H__
#ifdef __cplusplus
extern "C" {
#endif

#include <sys/mii.h>
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
 * Multicast logical address filter.
 */
struct lafilter {
	u_char	laf_vec[8];
};

/*
 * Principle (per-interface) structure.
 */
struct ef_info {
	struct etherif	ei_eif;			/* common ethernet interface */
	ioc3_eregs_t	*ei_regs;		/* ioc3 ether registers */
	volatile struct eftxd	*ei_txd;	/* tx descriptor ring */
	pciio_dmamap_t  ei_fastmap;             /* pciio fast dmamap handle */
	u_int		ei_txhead;		/* next txd to post */
	u_int		ei_txtail;		/* next txd to reclaim */
	struct mbuf	**ei_txm;		/* tx mbufs pending */
	u_int		ei_flags;		/* for various conditions */
	u_int		ei_intfreq;		/* freq. of RSVP intrs */

	u_int		ei_nrbuf;		/* # rbufs to keep posted */
	volatile __uint64_t	*ei_rxd;	/* rx descriptor ring */
	u_int		ei_rxhead;		/* next rxd to read */
	u_int		ei_rxtail;		/* next rxd to post */
	struct efrxbuf	*ei_rb;			/* quick ptr to head rbuf */
	struct mbuf	**ei_rxm;		/* posted rbufs */

	/* --- infrequently referenced fields below this line --- */

	vertex_hdl_t	ei_conn_vhdl;		/* vertex for requesting pci services */
	vertex_hdl_t	ei_ioc3_vhdl;		/* vertex for controlling ioc3 */
	vertex_hdl_t	ei_enet_vhdl;		/* vertex for ethernet private info */

	ioc3_mem_t	*ei_ioc3regs;		/* ioc3 registers */
	int             ei_chiprev;             /* ioc3 revision */
	pciio_intr_t	ei_intr;		/* interrupt handle */
	u_int		ei_phytype;		/* phy type code */
	int		ei_phyunit;		/* phy addr */
	u_int		ei_phyrev;		/* phy revision */

	struct lafilter	ei_laf;			/* logical address filter */
	u_int		ei_nmulti;		/* # multicast enabled */

	int		ei_speed100;		/* 0=10Mbps 1=100Mbps */
	int		ei_fullduplex;		/* 0=half 1=full */

	u_int		ei_ssram_bits;		/* emcr parity/bufsiz bits */

	u_int           ei_ipackets;            /* used for rxdelay tuning */
	u_int           ei_cpu_util;            /* used for rxdelay tuning */

	u_int		ei_remfaulting;		/* T/F phy remote fault */
	u_int		ei_jabbering;		/* T/F phy jabber fault */

	u_int		ei_defers;		/* # cumulative defers */
	u_int		ei_runt;		/* # runt packets */
	u_int		ei_crc;			/* # crc errors */
	u_int		ei_framerr;		/* # coding error */
	u_int		ei_giant;		/* # giant pkt errors */
	u_int		ei_idrop;		/* # dropped input packets */
	u_int		ei_ssramoflo;		/* # ssram overflow */
	u_int		ei_ssramuflo;		/* # ssram underflow */
	u_int		ei_memerr;		/* # rx/tx memory error */
	u_int		ei_parityerr;		/* # rx/tx parity error */
	u_int		ei_rtry;		/* # tx retry errors */
	u_int		ei_exdefer;		/* # excessive defers */
	u_int		ei_lcol;		/* # late collisions */
	u_int		ei_txgiant;		/* # tx giant packets */
	u_int		ei_linkdrop;		/* # tx dropped w/link down */
	u_int		ei_nombufs;		/* # tx m_get failures */
	u_int		ei_notxds;		/* # tx out of txds */
	u_int		ei_toobig;		/* # tx msg too big */
};

#define	ei_ac		ei_eif.eif_arpcom	/* common arp stuff */
#define	ei_if		ei_ac.ac_if		/* ifnet */
#define	ei_unit		ei_if.if_unit		/* logical unit # */
#define	ei_curaddr	ei_ac.ac_enaddr		/* current ether addr */
#define	ei_addr		ei_eif.eif_addr		/* i/f official addr */
#define	ei_rawif	ei_eif.eif_rawif	/* raw domain interface */
#define	eiftoei(eif)	((struct ef_info*)(eif)->eif_private)

/*
 * Flags used with ei_flags.
 */
#define	EIF_CKSUM_MASK	0x3		/* enable tx/rx hw checksum */
#define	EIF_CKSUM_RX	0x1		/* enable rx hw checksum */
#define	EIF_CKSUM_TX	0x2		/* enable tx hw checksum */
#define	EIF_LINKDOWN	0x4		/* T/F eisr link failure */
#define EIF_IN_ACTIVE	0x8		/* input recursion preventer */
#define	EIF_PSENABLED	0x10		/* packet scheduling is enabled */
#define	EIF_LOCK	0x1000		/* private driver bitlock */

#define	TXDSZ	(sizeof (struct eftxd))
#define	RXDSZ	(sizeof (__uint64_t))

#define	NTXDSML	128			/* # txd desktop configuration */
#define	NTXDBIG	512			/* # txd server configuration */
#define	NRXD	512			/* # rxd */

#define	GIANT_PACKET_SIZE	1536

/* PHY defines */
#define	PHY_QS6612X			0x0181441	/* Quality TX */
#define PHY_ICS1889			0x0015F41	/* ICS FX */
#define PHY_ICS1890			0x0015F42	/* ICS TX */
#define PHY_ICS1892			0x0015F43	/* ICS TX */
#define PHY_DP83840			0x20005C0	/* National TX */

#define ICS1890_R17_SPEED_100	0x8000
#define ICS1890_R17_FULL_DUPLEX	0x4000
#define ICS1890_R17_PD_FAULT	0x1000
#define ICS1890_R17_PD_MASK	0x3800

#define	DP83840_R25_SPEED_10	0x0040

#define	QS6612X_R31_OPMODE_MASK	0x001c
#define	QS6612X_R31_10		0x0004
#define	QS6612X_R31_100		0x0008
#define	QS6612X_R31_10FD	0x0014
#define	QS6612X_R31_100FD	0x0018

/* PHY errata structure */
struct phyerrata {
	__uint32_t	type;
	short		rev;
	unsigned short	reg, mask, val;
};

#define	NSC_OUI		0x00080017	/* National Semiconductor OUI */
#define	DP83840_R23_F_CONNECT	0x0020	/* bypass link disconnect function */

#ifdef __cplusplus
}
#endif
#endif /* __IF_EF_H__ */
