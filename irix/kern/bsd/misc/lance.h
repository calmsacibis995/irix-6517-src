#ifndef LANCE_H
#define LANCE_H
/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Definitions for the AMD 7990 LANCE chip.
 */
#ident "$Revision: 1.7 $"

#include "ether.h"

#define CRC_LEN		4		/* bytes/ethernet CRC */
#define EHDR_LEN	14

/* maximum & minimum packet sizes */
#define MAX_TPKT	(EHDR_LEN+1500)	    /* max xmit len (7990 adds crc) */
#define MIN_TPKT_U	60		    /* min xmit len, unchained pkts */
#define MIN_TPKT_C	100		    /* min xmit len, chained pkts */
#define MAX_RPKT	(MAX_TPKT+CRC_LEN)  /* max receive length */
#define	MAX_RDAT	(MAX_RPKT-EHDR_LEN) /* max recv data length, with crc */
#define MIN_RPKT	(60+CRC_LEN)	    /* min ethernet receive length */

/* decompose a 24-bit on-chip address */
#define LO_ADDR(a)	((u_int)(a) & 0xffff)
#define HI_ADDR(a)	((((u_int)(a)) >> 16) & 0xff)

/* combine parts of an on-board address */
#define MK_ADDR(lo, hi)	((lo) + ((hi)<<16))

/* 7990 CSR0 & CSR3 values */
#define CSR0_ERR	0x8000		/* any error */
#define CSR0_BABL	0x4000		/* babble */
#define CSR0_CERR	0x2000		/* collision error */
#define CSR0_MISS	0x1000		/* missed packet */
#define CSR0_MERR	0x0800		/* memory error */
#define CSR0_RINT	0x0400		/* receiver interrupt */
#define CSR0_TINT	0x0200		/* transmitter interrupt */
#define CSR0_IDON	0x0100		/* initialization done */
#define CSR0_INTR	0x0080		/* any interrupt */
#define CSR0_INEA	0x0040		/* interrupt enable */
#define CSR0_RXON	0x0020		/* receiver on */
#define CSR0_TXON	0x0010		/* transmitter on */
#define CSR0_TDMD	0x0008		/* transmit demand */
#define CSR0_STOP	0x0004		/* stop everything */
#define CSR0_STRT	0x0002		/* start everything */
#define CSR0_INIT	0x0001		/* initialize */
#define CSR3 		0x4		/* BSWP=1 ACON=0 BCON=0 */

/* bits in the 7990 initialization block mode word */
#define IB_PROM		0x8000		/* promiscuous mode */
#define IB_INTL		0x0040		/* internal loop back */
#define IB_DRTY		0x0020		/* disable retries */
#define IB_COLL		0x0010		/* force collision */
#define IB_DTCR		0x0008		/* disable transmit CRC */
#define IB_LOOP		0x0004		/* enable loopback */
#define IB_DTX		0x0002		/* disable transmitter */
#define IB_DRX		0x0001		/* disable receiver */

/* 7990 logical address filter is a bit-set of address hash values */
struct lafilter {
	u_short		laf_vec[4];
};

#define LAF_TSTBIT(laf, bit) \
	((laf)->laf_vec[(bit) >> 4] & (1 << ((bit) & 0xf)))
#define LAF_SETBIT(laf, bit) \
	((laf)->laf_vec[(bit) >> 4] |= (1 << ((bit) & 0xf)))
#define LAF_CLRBIT(laf, bit) \
	((laf)->laf_vec[(bit) >> 4] &= ~(1 << ((bit) & 0xf)))
#define LAF_SETALL(laf) \
	((laf)->laf_vec[0] = (laf)->laf_vec[1] = \
	 (laf)->laf_vec[2] = (laf)->laf_vec[3] = 0xFFFF)

/* 7990 initialization block */
struct ib {
	u_short		ib_mode;
	u_char		ib_padr[6];	/* shuffled ethernet address */
	struct lafilter	ib_laf;		/* logical address filter */
	u_short		ib_lrdra;	/* lsb receive ring address */
	u_short		ib_rlen:3,	/* receive ring address */
			ib_rpad:5,
			ib_hrdra:8;	/* msb receive ring address */
	u_short		ib_ltdra;	/* lsb transmit ring address */
	u_short		ib_tlen:3,	/* transmit ring length */
			ib_tpad:5,
			ib_htdra:8;	/* msb transmit ring address */
};

/* defines used out of message descriptor 1 */
#define	STP		0x02		/* start of packet */
#define ENP		0x01		/* end of packet */

/* receive descriptors, 8 byte aligned */
struct rmd {
	u_short		rmd_ladr;	/* lo buffer address */
	u_short		rmd_own:1,	/* 0x80 0=owned by cpu, 1=7990 */
			rmd_err:1,	/* 0x40 error */
			rmd_fram:1,	/* 0x20 framing error */
			rmd_oflo:1,	/* 0x10 silo overflow */
			rmd_crc:1,	/* 0x08 CRC error */
			rmd_buff:1,	/* 0x04 buffer chaining bug */
			rmd_stp_enp:2,	/* 0x03 start/end of packet */
			rmd_hadr:8;	/* high byte of address */
	short		rmd_bcnt;	/* negative buffer size */
	u_short		rmd_mcnt;	/* message byte count */
};

/* transmit descriptors, 8 byte aligned */
struct tmd {
	u_short		tmd_ladr;	/* lo buffer address */
	u_short		tmd_own:1,	/* 0x80 0=owned by cpu, 1=7990 */
			tmd_err:1,	/* 0x40 error */
			tmd_res:1,	/* 0x20 (reserved) */
			tmd_try:2,	/* 0x18 retries required */
			tmd_def:1,	/* 0x04 chip deferred to others */
			tmd_stp_enp:2,	/* 0x03 start/end of packet */
			tmd_hadr:8;	/* high byte of address */
	short		tmd_bcnt;	/* negative buffer size */
	u_short		tmd_buff:1,	/* 0x8000 buffer chaining bug */
			tmd_uflo:1,	/* 0x4000 silo underflow */
			tmd_res2:1,	/* 0x2000 */
			tmd_lcol:1,	/* 0x1000 late collision */
			tmd_lcar:1,	/* 0x0800 loss of carrier */
			tmd_rtry:1,	/* 0x0400 retried but failed */
			tmd_tdr:10;	/* 0x03ff time domain reflectometry */
};

/*
 * Network interface structure for LANCE-based Ethernet device drivers.
 */
struct lanceif {
	struct etherif		lif_eif;	/* common Ethernet interface */
	volatile struct ib	*lif_ib;	/* ptr to init block */
	struct lafilter		lif_laf;	/* copy of ib_laf */
	u_int			lif_lafcoll;	/* laf hash collisions */
	u_int			lif_nmulti;	/* active multicast count */
};

/*
 * Lance_init sets up the 7990 initialization block pointed at by lif_ib
 * using the promiscuous flag and current Ethernet address in lif_eif, and
 * the logical address filter copy in lif_laf.  It sets the receive and
 * transmit descriptor addresses from rdra and tdra. It uses rlen and tlen 
 * to set the receive and transmit ring length.
 *
 *	lance_init(&eninfo[unit].lanceif, flags, rdra, tdra, rlen, tlen);
 */
extern void	lance_init(struct lanceif *, int, u_int, u_int, u_int, u_int);

/*
 * An etherif ioctl implementation which handles multicast requests.
 * Since the 7990's logical address filter is imperfect, the code to
 * add a multicast address sets IFF_FILTMULTI.
 */
extern int	lance_ioctl(struct lanceif *, int, caddr_t);

#endif
