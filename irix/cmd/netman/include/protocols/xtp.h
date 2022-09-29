/*
 * xtp.h --
 *
 *	XTP packet format definitions. 
 *
 * Copyright (C) 1989 Protocol Engines, Incorporated.
 * All rights reserved.
 * ##NOTICE##
 *
 * $Revision: 1.4 $
 */

#ifndef __xtp__
#define __xtp__

#include "byteorder.h"

/* These are for sun */
#ifndef ETHERTYPE_XTP
#define ETHERTYPE_XTP		0x817D  /* Protocol Engines XTP */
#endif
#ifndef PF_XTP
#define PF_XTP			19	/* really just proto family, no AF */
#endif

#if _MIPS_SZLONG == 64
typedef u_int seq_t;	/* Type for sequence numbers */
#else
typedef u_long seq_t;	/* Type for sequence numbers */
#endif
/*
 * Header format:
 */

typedef struct header {

#if BYTE_ORDER == LITTLE_ENDIAN
	u_char	type;		/* packet type, version, HO_LITTLE bit */
	u_char	offset;		/* pad at beginnging of INFO segment */
	u_short	options; 	/* Options defined below */
#else
	u_short	options;
	u_char	offset;
	u_char	type;
#endif
#if _MIPS_SZLONG == 64
	int	key; 		/* Connection/transaction key */
	int	sort;		/* Priority or deadline */
	int	reserved;	/* Ignored for now */
	seq_t	seq;		/* Data sequence number */
	int	route; 		/* Route ID */
#else
	long	key; 		/* Connection/transaction key */
	long	sort;		/* Priority or deadline */
	long	reserved;	/* Ignored for now */
	seq_t	seq;		/* Data sequence number */
	long	route; 		/* Route ID */
#endif
} xtp_header;


#define	XTP_VERSION	(1 << 5)	/* H_VERSION value */

#define	H_TYP_MASK	0x1f		/* type    = bits 0-4 */
#define	H_VER_MASK	0x60		/* version = bits 5-6 */
#define	H_LE_MASK	0x80		/* little-endian  = bit 7 */
#define H_TYPE(x)	((x) & H_TYP_MASK)
#define H_VERSION(x)	(((x) & H_VER_MASK) >> 5)
#define H_LITTLE(x)	(((x) & H_LE_MASK) >> 7 )

#define H_ALIGNMENT	16	/* must be aligned on this size boundary */


#if BYTE_ORDER == LITTLE_ENDIAN
#define H_MUSTSWAP(h)	(!((h)->options & HO_LITTLE))
#else
#define H_MUSTSWAP(h)	((h)->options & HO_LITTLE)
#endif


/*
 * Header options flags:
 */
#define HO_BTAG		0x0001		/* Beginning user-tagged data present */
#define HO_FASTNAK	0x0080		/* send CNTL immed if span detected */
#define HO_DEADLINE 	0x0100		/* Sort field is time, not priority */
#define HO_SORT		0x0200		/* Enable sort field */
#define HO_RES		0x0400		/* Reservation mode */
#define HO_MULTI	0x0800		/* Multicast mode */
#define HO_NOERR	0x1000		/* Ignore errors: disable retrans. */
#define HO_DADDR	0x2000		/* Direct address format */
#define HO_NOCHECK	0x4000		/* Don't do checksum calculations */
#define HO_LITTLE	0x8000		/* 1 = little endian format, also used
				 	 * in type field */
/*
 *  Header options that are preserved from one packet to another.
 */
#define	X_HDR_KEEP	(HO_NOCHECK | HO_DADDR | HO_NOERR | HO_MULTI | HO_RES)


/*
 * Header packet types:
 *
 * If the low-order bit is set, the packet contains a control segment
 *  otherwise it contains an information segment.
 */

#define HT_DATA		0		/* Data packet */
#define HT_CNTL		1		/* Control segment */
#define HT_FIRST	2		/* First packet (has address segment) */
#define HT_PATH		6		/* Thread a path command */
#define HT_DIAG		8		/* Diagnostic packet */
#define HT_MAINT	10		/* Network aintenance packet */
#define HT_MGMT		14		/* Management packet */
#define HT_SUPER	16		/* Super packet */
#define HT_ROUTE	18		/* Router message */
#define HT_RCNTL	19		/* Router-generated CNTL message */


/*
 * Trailer format:
 */

typedef struct trailer {
#if _MIPS_SZLONG == 64
	int	dcheck;		/* Data checksum */
#else
	long	dcheck;		/* Data checksum */
#endif
	seq_t	dseq;		/* Delivered sequence number */
#if BYTE_ORDER == LITTLE_ENDIAN
	u_short	ttl; 		/* Time to live */
	u_short align:6;	/* Data segment trailing pad length */
	u_short	flags:10;	/* XTP trailer command */
#else
	u_short	flags:10;
	u_short align:6;
	u_short	ttl;
#endif
#if _MIPS_SZLONG == 64
	int	htcheck;	/* Header/trailer checksum */
#else
	long	htcheck;	/* Header/trailer checksum */
#endif
} xtp_trailer;

#define TO_END		0x0004		/* Last packet */
#define TO_EOM		0x0008		/* End of message */
#define TO_ETAG		0x0010		/* User-tagged data at end of data */
#define TO_DEFERCHK	0x0020		/* Fragment; defer data checksum */
#define TO_WCLOSE	0x0040		/* Writer closing connection */
#define TO_RCLOSE	0x0080		/* Reader closing connection */
#define TO_DREQ		0x0100		/* Delayed status request */
#define TO_SREQ		0x0200		/* Status request */

/*
 *  Define trailer flags that are preserved from one packet 
 *  to another, and which are likely kept in the context.
 */
#define	X_TRLR_KEEP	(TO_NODCHECK | TO_WCLOSE | TO_RCLOSE | TO_END)


/*
 * Packet prototype:
 */
typedef struct packet {
	xtp_header h;
	char data[16];
} xtp_packet;


/*
 * Control packet format:
 *	Contains the standard XTP header, control segment and trailer.
 *	Currently we ignore the possibility that some pad space might
 *	be needed.  With MAX_SPAN=3, the control pkt can fit a small mbuf.
 */
#define MAX_SPAN 3		/* # of resend pairs */

				/* rate control */
typedef struct	cseg {
#if _MIPS_SZLONG == 64
	int	rate;		/* Max. output rate in bytes/sec */
	int	burst;		/* Max. bytes per transmission */

				/* error synchronization */
	int	sync;		/* Sync value to be returned */
	int	echo;		/* Returned echo */

				/* rtt estimation */
	int	time;		/* Current time for estim. round-trip time */
	int	timeecho;	/* Current time echo for estim. rtt */

				/* efficient key management */
	int	xkey;		/* Key exchange */
	int	xroute;		/* Route exchange */

				/* flow control */
	int	reserved;	/* unassigned */
	seq_t	alloc;		/* Do-not-exceed output allocation */

				/* error control */
	seq_t	rseq;		/* Greatest received consecutive seq. + 1 */
	int	nspan;		/* Number of retransmission spans */
#else
	long	rate;		/* Max. output rate in bytes/sec */
	long	burst;		/* Max. bytes per transmission */

				/* error synchronization */
	long	sync;		/* Sync value to be returned */
	long	echo;		/* Returned echo */

				/* rtt estimation */
	long	time;		/* Current time for estim. round-trip time */
	long	timeecho;	/* Current time echo for estim. rtt */

				/* efficient key management */
	long	xkey;		/* Key exchange */
	long	xroute;		/* Route exchange */

				/* flow control */
	long	reserved;	/* unassigned */
	seq_t	alloc;		/* Do-not-exceed output allocation */

				/* error control */
	seq_t	rseq;		/* Greatest received consecutive seq. + 1 */
	long	nspan;		/* Number of retransmission spans */
#endif
	seq_t	spans[MAX_SPAN][2];
				/* Selective retransmission seq pairs */
} xtp_cseg;

typedef struct control {
	xtp_header h;
	xtp_cseg c;
	xtp_trailer t;
} xtp_control;

#define CSEG_MINLEN  48

/*
 *  Data segment contents --
 *
 */

/* 
 * Address segment format:
 *
 *	For the Internet address field:
 *	Contains the same data that would have been in
 *	a TCP/IP or TCP/UDP packet header to the same
 *	address. The bits of significance include src & dest
 *	IP addresses, IP protocol type, and src & dest port numbers.
 */

typedef struct addrseg {
#if BYTE_ORDER == LITTLE_ENDIAN
    u_short	format;			/* Addr. format: defined below */
    u_short	length;			/* Length of the entire segment */
#else
    u_short	length;			/* Length of the entire segment */
    u_short	format;			/* Addr. format: defined below */
#endif
#if _MIPS_SZLONG == 64
    u_int	ratereq;		/* Requested max sending rate */
#else
    u_long	ratereq;		/* Requested max sending rate */
#endif
    u_char	id[6];			/* MAC addr of original sender */
    u_short	reserved;		/* May be used for quality of service */
    union {
	struct {	/* Internet Protocol */
#if _MIPS_SZLONG == 64
		u_int	dstaddr;
		u_int	srcaddr;
#else
		u_long	dstaddr;
		u_long	srcaddr;
#endif
		u_short	dstport;
		u_short	srcport;
		u_char	ipproto;	/* protocol associated with dst port */
		u_char	pad[3];  	/* Aligns DATA on octal boundary */
	} inet;
	struct {	/* ISO */
		u_char	nsap[20];
#if _MIPS_SZLONG == 64
		u_int	tsap;
#else
		u_long	tsap;
#endif
	} iso;
	struct {	/* XNS */
#if _MIPS_SZLONG == 64
		u_int	dstnet;
		u_int	srcnet;
#else
		u_long	dstnet;
		u_long	srcnet;
#endif
		u_char	dstaddr[6];
		u_char	srcaddr[6];
		u_short	dstport;
		u_short	srcport;
		u_char	type;		/* ITP type field */
		u_char	pad[7];  	/* Aligns DATA on octal boundary */
	} xns;
        struct {	/* DADDR */
#if _MIPS_SZLONG == 64
		u_int  key;
#else
		u_long  key;
#endif 
		u_char	mac[6];
        } daddr;
	    		/* NETBIOS */
	u_char	netbios_addr[16];	/* unformatted bytes, fixed length */
    } addr;
} xtp_addr;

#define	XTP_ADDRLEN	XTP_ROUND(sizeof xtp_addr)
#define ADDRSEG_MINLEN  16


/* 
 * Address format types:
 *	this code must be put into both format bytes.
 */
#define XTPAF_NULL		0	/* Null address */
#define XTPAF_INET		0x0101	/* Internet Protocol address */
#define XTPAF_ISO		0x0202	/* ISO address */
#define XTPAF_XNS		0x0303	/* Xerox NS address */
#define XTPAF_IBMSRCRTE		0x0404	/* IBM-style Source route address */
#define XTPAF_MODSIM		0x0505	/* USAF Modular Simulator address */
#define XTPAF_NETBIOS		0x0606	/* Microsoft/NetBIOS address format */
#define XTPAF_IPLOOSERTE	0x0707	/* IP-style Loose Source Route */
#define XTPAF_IPSTRICTRTE 	0x0808	/* IP-style Strict Source Route */
#define XTPAF_DADDR	 	0x0909	/* XTP Direct Address Key */
#define XTPAF_EXPERIMENTAL 	0x0a0a	/* XTP experimental address type */
#define XTPAF_RIVETFIRE	 	0x0b0b	/* USAF embedded system address */
#define	NXTPAFORMATS		12


#define	MIN_PKT_SIZE	64		/* Packets must be this long */
#define	MAX_PAD_SIZE	\
		(MIN_PKT_SIZE - sizeof (xtp_header) - sizeof (xtp_trailer))

/*
 * Diagnostic packet format:
 */

#define DIAG_MSG_LEN	32

typedef struct diag {
#if _MIPS_SZLONG == 64
	u_int code;			/* Diag. code defined below */
	u_int value;			/* Code-specific value */
#else
	u_long code;			/* Diag. code defined below */
	u_long value;			/* Code-specific value */
#endif
	u_char msg[DIAG_MSG_LEN];	/* Code-specific message */
} xtp_diag;

typedef struct diagpkt {
	xtp_header	h;
	xtp_diag	d;
#if MIN_PKT_SIZE > 80  /* sizeof (header,diag,trailer) */
	char		pad [MIN_PKT_SIZE -sizeof (xtp_header)
					  -sizeof (xtp_diag)
					  -sizeof (xtp_trailer)];
#endif
	xtp_trailer	t;
} xtp_diagpkt;

#define DIAG_UNKNOWN_KEY	1	/* Can't find context for the key */
#define DIAG_CONTEXT_REFUSED	2	/* Can't create a context */
#define DIAG_UNKNOWN_DEST	3	/* Bad address */
#define DIAG_DEAD_HOST		4	/* Host not up (from router) */
#define DIAG_INVALID_ROUTE	5	/* Bad route ID (from router) */
#define DIAG_REDIRECT		6	/* Shorter route possible(from router)*/
#define DIAG_NOROUTE		7	/* Cannot create route */
#define	DIAG_NORESOURCE		8	/* No room for new context */
#define	DIAG_PROTERR		9	/* error in protocol (pkt semantics)  */
#define DIAG_MAX_CODE	DIAG_PROTERR

/* DIAG_UNKNOWN_DEST subtypes: */
#define	DIAG_UD_UNSUPPAF	1	/* unsupported address family */

/* DIAG_PROTERR subtypes: */
#define DIAG_PE_BADAF		1	/* malformed address segment */
#define DIAG_PE_ROUTEREQD	2	/* route=0 on pkt to be fwded */
#define DIAG_PE_INVALID_REXMT	3	/* rseq < start of output Q */

/* DIAG_NOROUTE subtypes: */
#define	DIAG_NR_UNSUPPAF	1	/* unsupported address family */


/*
 * Route packet format:
 */

typedef struct {
#if _MIPS_SZLONG == 64
	u_int code;			/* Diag. code defined below */
	u_int value;			/* Code-specific value */
#else
	u_long code;			/* Diag. code defined below */
	u_long value;			/* Code-specific value */
#endif
} xtp_route;

typedef struct routepkt {
	xtp_header	h;
	xtp_route	r;
#if MIN_PKT_SIZE > 48  /* sizeof (header,diag,trailer) */
	char		pad [MIN_PKT_SIZE -sizeof (xtp_header)
					  -sizeof (xtp_route)
					  -sizeof (xtp_trailer)];
#endif
	xtp_trailer	t;
} xtp_routepkt;

#define RTE_RELEASE	1	/* Release route */
#define RTE_RELACK	2	/* Release ACK */


/*
 * XTP sequence numbers are 32-bit integers operated
 * on with modular arithmetic.  
 */
#define	SEQ_LT(a,b)	((int)((a)-(b)) < 0)
#define	SEQ_LEQ(a,b)	((int)((a)-(b)) <= 0)
#define	SEQ_GT(a,b)	((int)((a)-(b)) > 0)
#define	SEQ_GEQ(a,b)	((int)((a)-(b)) >= 0)

#define	XTP_PKT_ROUND		8		/* byte modulus size */
#define	XTP_ROUND(n)		(((int)(n)+XTP_PKT_ROUND-1)& ~(XTP_PKT_ROUND-1))
#define IS_ALIGNED(p, align)    ((((int) p) & (align -1)) == 0)

#define	INITIAL_SEQ	((seq_t) 0)	/* 1st seq # to xmit */

#endif
