/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/sun/snoop/RCS/ntp.h,v 1.2 1996/07/06 17:45:10 nn Exp $"

#ifndef	_NTP_H
#define	_NTP_H

#ifdef __cplusplus
extern "C" {
#endif

/* $Source: /proj/irix6.5.7m/isms/eoe/cmd/sun/snoop/RCS/ntp.h,v $  */
/* $Revision: 1.2 $ $Date: 1996/07/06 17:45:10 $ */

/*
 *  $Log: ntp.h,v $
 *  Revision 1.2  1996/07/06 17:45:10  nn
 *  fix n32 compile-time warnings
 *
 * Revision 1.1  1996/06/19  20:33:22  nn
 * Initial revision
 *
 * Revision 3.4.1.5  89/04/10  15:55:42  louie
 * Provide default value for number of bits/byte if not defined.  Compute the
 * Window shift mask inside of conditional code on XTAL so we get the correct
 * value if configured without a crystal controled clock (!!)
 *
 * Revision 3.4.1.4  89/03/31  16:34:50  louie
 * Add bit in flags which allow a peer to be synced to.  Changed a char to a bit
 * field so that it is always signed.
 *
 * Revision 3.4.1.3  89/03/29  12:26:18  louie
 * Removed some unused #defines.  Replaced MAXSTRATUM with NTP_INFIN per new
 * spec.  The variable 'mode' in the peer structure has been renamed 'hmode'
 * per the new spec.
 *
 * Revision 3.4.1.2  89/03/22  18:28:18  louie
 * patch3: Use new RCS headers.
 *
 * Revision 3.4.1.1  89/03/20  00:02:53  louie
 * 1
 *
 * Revision 3.4  89/03/17  18:37:00  louie
 * Latest test release.
 *
 * Revision 3.3.1.1  89/03/17  18:23:49  louie
 * Change CLOCK_FACTOR to be a power of 2.
 *
 * Revision 3.3  89/03/15  14:19:36  louie
 * New baseline for next release.
 *
 * Revision 3.2.1.2  89/03/15  13:46:52  louie
 * The version number for that particular flavor of ntpd <--> ntpdc interaction
 * is now defined by NTPDC_VERSION.  The packet format for the ntpdc program
 * has changed slightly to improve robustness when dealing with multiple packets
 * of status data.
 *
 * Revision 3.2.1.1  89/03/09  17:11:24  louie
 * patch1: Updated constants, which were previously in incorrect units.
 *
 * Revision 3.2  89/03/07  18:21:45  louie
 * New version of UNIX NTP daemon and software based on the 6 March 1989
 * draft of the new NTP protocol specification.  This version doesn't
 * implement authentication, and accepts and send only NTP Version 1
 * packets.
 *
 * Revision 3.1.1.1  89/02/15  08:54:42  louie
 * *** empty log message ***
 *
 *
 * Revision 3.1  89/01/30  14:43:07  louie
 * Second UNIX NTP test release.
 *
 * Revision 3.0  88/12/12  16:01:07  louie
 * Test release of new UNIX NTP software.  This version should conform to the
 * revised NTP protocol specification.
 *
 */

#ifndef FD_SET
#define	NFDBITS		32
#define	FD_SETSIZE	32
#define	FD_SET(n, p)	((p)->fds_bits[(n)/NFDBITS] |= (1 << ((n) % NFDBITS)))
#define	FD_CLR(n, p)	((p)->fds_bits[(n)/NFDBITS] &= ~(1 << ((n) % NFDBITS)))
#define	FD_ISSET(n, p)	((p)->fds_bits[(n)/NFDBITS] & (1 << ((n) % NFDBITS)))
#define	FD_ZERO(p)	bzero((char *)(p), sizeof (*(p)))
#endif

#ifndef	NBBY
#define	NBBY	8	/* number of bits per byte */
#endif

#define	MAXNETIF	10

struct intf {
	int fd;
	char *name;
	struct sockaddr_in sin;
	struct sockaddr_in bcast;
	struct sockaddr_in mask;
	int uses;
	int if_flags;
};
extern struct intf addrs[];
extern int nintf;

/*
 *  Definitions for the masses
 */
#define	JAN_1970	2208988800	/* 1970 - 1900 in seconds */

/*
 *  Daemon specific (ntpd.c)
 */
#define	SHIFT_MASK	0xff	/* number of intervals to wait */

#ifndef	WAYTOOBIG
#define	WAYTOOBIG	1000.0	/* Too many seconds to correct, something is */
				/* really wrong */
#endif

#ifndef	XTAL
#define	XTAL	1	/* crystal controlled clock by default */
#endif

#ifndef	NTPINITFILE
#define	NTPINITFILE	"/etc/ntp.conf"
#endif

struct list {
	struct ntp_peer *head;
	struct ntp_peer *tail;
	int members;
};

#define	STRMCMP(a, cond, b) \
	(((a) == UNSPECIFIED ? NTP_INFIN+1 : a) cond \
		((b) == UNSPECIFIED ? NTP_INFIN+1 : (b)))


/*
 *  Definitions outlined in the NTP spec
 */
#define	NTP_VERSION	1
#define	NTP_PORT	123	/* for ref only (see /etc/services) */
#define	NTP_INFIN	15
#define	NTP_MAXAGE	86400
#define	NTP_MAXSKW	0.01	/* seconds */
#define	NTP_MINDIST	0.02	/* seconds */
#define	NTP_MINPOLL	6	/* (64) seconds between messages */
#define	NTP_MAXPOLL	10	/* (1024) secs to poll */
#define	NTP_WINDOW	8	/* size of shift register */
#define	NTP_MAXWGT	8	/* maximum allowable dispersion */
#define	NTP_MAXLIST	5	/* max size of selection list */
#define	NTP_MAXSTRA	2	/* max number of strata in selection list */
#define	X_NTP_CANDIDATES 64	/* number of peers to consider when doing */
				/*	clock selection */
#define	NTP_SELECT	0.75	/* weight used to compute dispersion */

#define	PEER_MAXDISP	64.0	/* Maximum dispersion  */
#define	PEER_THRESHOLD	0.5	/* dispersion threshold */
#define	PEER_FILTER	0.5	/* filter weight */

#if	XTAL == 0
#define	PEER_SHIFT	4
#define	NTP_WINDOW_SHIFT_MASK 0x0f
#else
#define	PEER_SHIFT	8
#define	NTP_WINDOW_SHIFT_MASK 0xff
#endif


/*
 *  5.1 Uniform Phase Adjustments
 *  Clock parameters
 */
#define	CLOCK_UPDATE	8	/* update interval (1<<CLOCK_UPDATE secs) */
#if	XTAL
#define	CLOCK_ADJ	2	/* adjustment interval (1<<CLOCK_ADJ secs) */
#define	CLOCK_PHASE	8	/* phase shift */
#define	CLOCK_MAX	0.128	/* maximum aperture (milliseconds) */
#else
#define	CLOCK_ADJ	0
#define	CLOCK_PHASE	6	/* phase shift */
#define	CLOCK_MAX	0.512	/* maximum aperture (milliseconds) */
#endif
#define	CLOCK_FREQ	10	/* frequency shift */
#define	CLOCK_TRACK	8
#define	CLOCK_COMP	4
#define	CLOCK_FACTOR	18

/*
 * Structure definitions for NTP fixed point values
 */
/*CSTYLED*/
/*    0			  1		      2			  3 */
/*CSTYLED*/
/*    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
/*CSTYLED*/
/*   |			       Integer Part			     |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
/*CSTYLED*/
/*   |			       Fraction Part			     |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
/*CSTYLED*/
/*    0			  1		      2			  3
 *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
/*CSTYLED*/
/*   |		  Integer Part	     |	   Fraction Part	     |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
struct l_fixedpt {
	u_long int_part;
	u_long fraction;
};

struct s_fixedpt {
	u_short int_part;
	u_short fraction;
};

/*  =================  Table 3.3. Packet Variables   ================= */
/*CSTYLED*/
/*    0			  1		      2			  3
 *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
/*CSTYLED*/
/*   |LI | VN  | Mode|	  Stratum    |	    Poll     |	 Precision   |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
/*CSTYLED*/
/*   |			   Synchronizing Distance		     |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
/*CSTYLED*/
/*   |			  Synchronizing Dispersion		     |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
/*CSTYLED*/
/*   |			Reference Clock Identifier		     |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
/*CSTYLED*/
/*   |								     | */
/*CSTYLED*/
/*   |		       Reference Timestamp (64 bits)		     | */
/*CSTYLED*/
/*   |								     |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
/*CSTYLED*/
/*   |								     | */
/*CSTYLED*/
/*   |		       Originate Timestamp (64 bits)		     | */
/*CSTYLED*/
/*   |								     |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
/*CSTYLED*/
/*   |								     | */
/*CSTYLED*/
/*   |			Receive Timestamp (64 bits)		     | */
/*CSTYLED*/
/*   |								     |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
/*CSTYLED*/
/*   |								     | */
/*CSTYLED*/
/*   |			Transmit Timestamp (64 bits)		     | */
/*CSTYLED*/
/*   |								     |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
struct ntpdata {
	u_char status;		/* status of local clock and leap info */
	u_char stratum;		/* Stratum level */
	u_char ppoll;		/* poll value */
	int precision:8;
	struct s_fixedpt distance;
	struct s_fixedpt dispersion;
	u_long refid;
	struct l_fixedpt reftime;
	struct l_fixedpt org;
	struct l_fixedpt rec;
	struct l_fixedpt xmt;
};
/*
 *	Leap Second Codes (high order two bits)
 */
#define	NO_WARNING	0x00	/* no warning */
#define	PLUS_SEC	0x40	/* add a second (61 seconds) */
#define	MINUS_SEC	0x80	/* minus a second (59 seconds) */
#define	ALARM		0xc0	/* alarm condition (clock unsynchronized) */

/*
 *	Clock Status Bits that Encode Version
 */
#define	NTPVERSION_1	0x08
#define	VERSIONMASK	0x38
#define	LEAPMASK	0xc0
#define	NTPMODEMASK	0x07

/*
 *	Code values
 */
#define	MODE_UNSPEC	0	/* unspecified */
#define	MODE_SYM_ACT	1	/* symmetric active */
#define	MODE_SYM_PAS	2	/* symmetric passive */
#define	MODE_CLIENT	3	/* client */
#define	MODE_SERVER	4	/* server */
#define	MODE_BROADCAST	5	/* broadcast */
#define	MODE_RES1	6	/* reserved */
#define	MODE_RES2	7	/* reserved */

/*
 *	Stratum Definitions
 */
#define	UNSPECIFIED	0
#define	PRIM_REF	1	/* radio clock */
#define	INFO_QUERY	62	/* **** THIS implementation dependent **** */
#define	INFO_REPLY	63	/* **** THIS implementation dependent **** */


/* =================  table 3.2 Peer Variables	================= */
struct ntp_peer {
	struct ntp_peer *next, *prev;
	struct sockaddr_in src;		/* both peer.srcadr and peer.srcport */
	int	flags;			/* local flags */
#define	PEER_FL_CONFIG		1
#define	PEER_FL_AUTHENABLE	2
#define	PEER_FL_SYNC		0x1000	/* peer can bet sync'd to */
#define	PEER_FL_BCAST		0x2000	/* broadcast peer */
#define	PEER_FL_SELECTED	0x8000	/* actually used by query routine */

	int	sock;			/* index into sockets to derive */
					/*   peer.dstadr and peer.dstport */
	u_char	leap;			/* receive */
	u_char	hmode;			/* receive */
	u_char	stratum;		/* receive */
	u_char	ppoll;			/* receive */
	u_char	hpoll;			/* poll update */
	short	precision;		/* receive */
	struct	s_fixedpt distance;	/* receive */
	struct	s_fixedpt dispersion;	/* receive */
	u_long	refid;			/* receive */
	struct	l_fixedpt reftime;	/* receive */
	struct	l_fixedpt org;		/* receive, clear */
	struct	l_fixedpt rec;		/* receive, clear */
	struct	l_fixedpt xmt;		/* transmit, clear */
	u_long	reach;			/* receive, transmit, clear */
	u_long	valid;			/* packet, transmit, clear */
	u_long	timer;			/* receive, transmit, poll update */
	long	stopwatch;		/* <<local>> for timing */
	/*
	 * first order offsets
	 */
	struct	filter {
		short samples;		/* <<local>> */
		double offset[PEER_SHIFT];
		double delay[PEER_SHIFT];
	} filter;			/* filter, clear */

	double	estdelay;		/* filter */
	double	estoffset;		/* filter */
	double	estdisp;		/* filter */

	u_long	pkt_sent;		/* <<local>> */
	u_long 	pkt_rcvd;		/* <<local>> */
	u_long	pkt_dropped;		/* <<local>> */
};

/* ================= table 3.1:  System Variables ================= */

struct sysdata {			/* procedure */
	u_char leap;			/* clock update */
	u_char stratum;			/* clock update */
	short precision;		/* system */
	struct s_fixedpt distance;	/* clock update */
	struct s_fixedpt dispersion;	/* clock update */
	u_long refid;			/* clock update */
	struct l_fixedpt reftime;	/* clock update */
	int hold;			/* clock update */
	struct ntp_peer *peer;		/* selection */
	int maxpeers;			/* <<local>> */
	u_char filler;		/* put here for %&*%$$ SUNs */
};

#define	NTPDC_VERSION	2

/*
 *  These structures are used to pass information to the ntpdc (control)
 *  program.  They are unique to this implementation and not part of the
 *  NTP specification.
 */
struct clockinfo {
	u_long net_address;
	u_long my_address;
	u_short port;
	u_short flags;
	u_long pkt_sent;
	u_long pkt_rcvd;
	u_long pkt_dropped;
	u_long timer;
	u_char leap;
	u_char stratum;
	u_char ppoll;
	int precision:8;

	u_char hpoll;
	u_char filler1;
	u_short reach;

	long	estdisp;			/* scaled by 1000 */
	long	estdelay;			/* in milliseconds */
	long	estoffset;			/* in milliseconds */
	u_long refid;
	struct l_fixedpt reftime;
	struct info_filter {
		short index;
		short filler;
		long offset[PEER_SHIFT];	/* in milliseconds */
		long delay[PEER_SHIFT];		/* in milliseconds */
	} info_filter;
};

struct ntpinfo {
	u_char version;
	u_char type;		/* request type (stratum in ntp packets) */
	u_char count;		/* number of entries in this packet */
	u_char seq;		/* sequence number of this packet */

	u_char npkts;		/* total number of packets */
	u_char peers;
	u_char fill3;
	u_char fill4;
};

#ifdef __cplusplus
}
#endif

#endif	/* _NTP_H */
