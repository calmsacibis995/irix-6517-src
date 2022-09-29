/*
 *  Spider STREAMS Data Link Interface Primitives
 *
 *  Copyright (c) 1989  Spider Systems Limited
 *
 *  This Source Code is furnished under Licence, and may not be
 *  copied or distributed without express written agreement.
 *
 *  All rights reserved.
 *
 *  Written by Mark Valentine
 *
 *  Made in Scotland.
 *
 *	@(#)dl_control.h	1.1
 *
 *	Last delta created	15:02:19 5/12/93
 *	This file extracted	16:42:12 5/28/93
 *
 *	Modifications:
 *
 *		28 Jan 1992	Modified for datalink version 2
 *
 */

#include <sys/snet/dl_version.h>

#if DL_VERSION == 0	/* externally defined */
/*
 *  Generic Ethernet Driver interface.  This is defined to be
 *  Version 0 of the Spider Data Link proptocol.
 */
#elif DL_VERSION == 1
/*
 *  This protocol interface is derived and generalised from the
 *  SpiderTCP Generic Ethernet Driver interface.  This defines
 *  Version 1 of the protocol.  The main restriction in this
 *  version is that all hardware addresses must be of length 6.
 *  In addition, statistics are only defined for Ethernet hardware.
 */
#elif DL_VERSION == 2
/*
 *  This defines Version 2 of Spider's STREAMS Data Link protocol.
 *  Its main feature is its ability to cope with hardware addresses
 *  of length not equal to 6.
 */
#endif /*DL_VERSION*/

/*
 *  Data Link ioctl commands.
 *
 *  To determine the version of the protocol in use, use the DATAL_VERSION
 *  command, and assume Version 0 if this fails with EINVAL.  (Yuk.)
 *
 *  The ETH_* commands will work for any current version of the protocol,
 *  but only for Ethernet drivers (hw_type == HW_ETHER).
 *
 *  Hardware types are defined in dl_proto.h.
 */

#define DATAL_STAT	('E'<<8|1)	/* gather data link statistics */
#define DATAL_ZERO	('E'<<8|2)	/* reset data link statistics */
#define DATAL_REGISTER	('E'<<8|3)	/* register data link type range */
#define DATAL_GPARM	('E'<<8|4)	/* determine data link parameters */
#ifdef DEBUG
#define DATAL_DEBUG	('E'<<8|5)	/* debugging interface */
#endif
#define DATAL_SET_ADDR	('E'<<8|6)	/* set hardware address */
#define DATAL_DFLT_ADDR	('E'<<8|7)	/* restore default hardware address */
#define DATAL_VERSION	('E'<<8|8)	/* interrogate protocol version */

#if DL_VERSION == 0

#define ETH_STAT	DATAL_STAT
#define ETH_ZERO 	DATAL_ZERO
#define ETH_REGISTER	DATAL_REGISTER
#define ETH_GPARM	DATAL_GPARM
#define ETH_SET_ADDR	DATAL_SET_ADDR
#define ETH_DFLT_ADDR	DATAL_DFLT_ADDR
#ifdef DEBUG
#define ETH_DEBUG	DATAL_DEBUG
#define ED_RANGETAB	DATAL_RANGETAB
#endif

#define eth_stat	datal_stat
#define eth_register	datal_register
#define eth_gparm	datal_gparm
#define eth_parm	eth_gparm
#ifdef DEBUG
#define eth_debug	datal_debug
#endif

#define	dl_tx		datal_tx
#define	dl_rx		datal_rx
#define	eth_coll	dl_coll
#define eth_lost	dl_lost
#define eth_txerr	dl_txerr
#define eth_rxerr	dl_rxerr
#endif

/*
 *  Data Link statistics structure.
 */

struct datal_stat
{
	uint32	dl_tx;		/* packets transmitted */
	uint32	dl_rx;		/* packets received */
	uint32	dl_coll;	/* collisions detected */
	uint32	dl_lost;	/* packets lost */
	uint32	dl_txerr;	/* transmission errors */
	uint32	dl_rxerr;	/* receive errors */
};

struct datal_register
{
#if DL_VERSION <= 1
	uint16	lwb, upb;	/* data link type range */
#else
	uint8	version;	/* protocol version */
	uint32	mac_type;	/* mac type */
	uint8	addr_len;	/* hardware address length */
	uint8	align;		/* don't use */
	uint16	lwb;		/* data link type (lower bound) */
	uint16	upb;		/* data link type (upper bound) */
#endif
};

struct datal_gparm
{
#if DL_VERSION <= 1
	uint8	addr[6];	/* hardware address */
	uint16	frgsz;		/* max. packet size on net */
#else /*DL_VERSION*/
	uint8	version;	/* protocol version */
	uint32	mac_type;	/* mac type */
	uint8	addr_len;	/* hardware address length */
	uint8	align;		/* don't use */
	uint16	frgsz;		/* max. packet size on net */
	uint8	addr[1];	/* hardware address (variable length) */
#endif /*DL_VERSION*/
};

#ifdef DEBUG
/*
 *  Debugging interface
 */

struct datal_debug
{
	int	flags;		/* what to do */
	int	count;		/* how much done */
	char	space[1024];	/* where to do it */
};

#define	DATAL_RANGETAB	0x01	/* copy out range table */
#endif /* DEBUG */

struct datal_version
{
	uint8	version;	/* protocol version number */
};
