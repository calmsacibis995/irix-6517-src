/*
 * |-----------------------------------------------------------|
 * | Copyright (c) 1991, 1990 MIPS Computer Systems, Inc.      |
 * | All Rights Reserved                                       |
 * |-----------------------------------------------------------|
 * |          Restricted Rights Legend                         |
 * | Use, duplication, or disclosure by the Government is      |
 * | subject to restrictions as set forth in                   |
 * | subparagraph (c)(1)(ii) of the Rights in Technical        |
 * | Data and Computer Software Clause of DFARS 252.227-7013.  |
 * |         MIPS Computer Systems, Inc.                       |
 * |         950 DeGuigne Avenue                               |
 * |         Sunnyvale, California 94088-3650, USA             |
 * |-----------------------------------------------------------|
 */
/* $Header: /proj/irix6.5.7m/isms/irix/kern/sys/RCS/xti.h,v 1.8 1999/07/21 21:11:45 ericm Exp $ */

#ifndef _SYS_XTI_H
#define _SYS_XTI_H

#include <sys/tiuser.h>

/* 
 * The following are the events returned by t_look
 */

#define T_GODATA	0x0100	/* sending normal data is again possible */
#define T_GOEXDATA	0x0200	/* sending expedited data is again possible */
#undef T_EVENTS
#define T_EVENTS	0x03ff	/* event mask	                  */

/*
 * XTI error return
 */

#ifndef _KERNEL
#ifdef _BUILDING_SVR4_LIBC
extern int *_t_errnop;
#define T_ERRNO	(*_t_errnop)
#else
extern int t_errno;
#endif /* _BUILDING_SVR4_LIBC */
#endif

/*
 * The following are structure types used when dynamically
 * allocating the above structures via t_alloc().
 */
#define T_BIND_STR	1		/* struct t_bind	*/
#define T_OPTMGMT_STR	2		/* struct t_optmgmt	*/
#define T_CALL_STR	3		/* struct t_call	*/
#define T_DIS_STR	4		/* struct t_discon	*/
#define T_UNITDATA_STR	5		/* struct t_unitdata	*/
#define T_UDERROR_STR	6		/* struct t_uderr	*/
#define T_INFO_STR	7		/* struct t_info	*/

/*
 * general purpose defines
 */

#define	T_YES		1
#define T_NO		0
#define T_UNUSED	-1
#define	T_NULL		0
#define T_ABSREQ	0x8000
#if _XOPEN4
#define T_INFINITE	-1
#define T_INVALID	-2
#endif

/* T_INFINITE and T_INVALID are values of t_info */

/*
 *	ISO-specific option and management primitives 
 */

/*
 *	definition of the ISO transport classes
 */

#define	T_CLASS0	0
#define	T_CLASS1	1
#define	T_CLASS2	2
#define	T_CLASS3	3
#define	T_CLASS4	4

/*
 *	definition of the  priorities 
 */

#define	T_PRITOP	0
#define	T_PRIHIGH	1
#define	T_PRIMID	2
#define	T_PRILOW	3
#define	T_PRIDFLT	4

/*
 *	definition of the protection levels
 */

#define	T_NOPROTECT		1
#define	T_PASSIVEPROTECT	2
#define T_ACTIVEPROTECT		4

/*
 *	default values for the length of the TPDU's
 */

#define	T_LTPDUDFLT	128

/*
 *	rate structure
 */

struct rate {
	xtiscalar_t targetvalue;	/* target value */
	xtiscalar_t minacceptvalue;	/* minimum acceptable value */
};

/*
 *	reqvalue structure
 */

struct reqvalue {
	struct	rate	called;		/* called rate */
	struct	rate	calling;	/* calling rate */
};

/*
 *	thrpt structure
 */

struct	thrpt {
	struct	reqvalue maxthrpt;	/* maximum throughput */
	struct	reqvalue avgthrpt;	/* average throughput */
};

/*
 *	management structure 
 */

struct	management {
	short	dflt;		/* T_YES: the following parameter values */
				/*	  are ignored, default values	*/
				/*	  are used;			*/
				/* T_NO:  the following parameter values */
				/*	  are used			*/
	int	ltpdu;		/* maximum length of TPDU (in octets)	*/
	short	reastime;	/* reassignment time (in seconds)	*/
#ifdef __cplusplus
	char	prfclass;	/* preferred class; value: T_CLASS0-    */
#else /* ansi c */
	char	class;		/* preferred class; value: T_CLASS0-	*/
#endif
	char	altclass;	/* alternative class			*/
	char	extform;	/* extended format: T_YES or T_NO	*/
	char	flowctrl;	/* flow control: T_YES or T_NO		*/
	char	checksum;	/* checksum (cl. 4): T_YES or T_NO	*/
	char	netexp;		/* network expeditted data: T_YES or 	*/
				/* 	T_NO				*/
	char	netrecptcf;	/* receipt confirmation: T_YES or T_NO	*/
};


/*
 *	ISO connection-oriented options
 */

struct	isoco_options {
	struct	thrpt	throughput;	/* throughput			*/
	struct	reqvalue transdel;	/* transit delay		*/
	struct	rate	reserrorrate;	/* residual error rate		*/
	struct	rate	transffailprob;	/* transfer failure probability */
	struct	rate	estfailprob;	/* connection establishment 	*/
					/* 	failure probability	*/
	struct	rate	relfailprob;	/* connection release failure	*/
					/*	probability		*/
	struct	rate	estdelay;	/* connection establishment	*/
					/*	delay			*/
	struct	rate	reldelay;	/* connection release delay	*/
	struct	netbuf	connresil;	/* connection resilience	*/
	unsigned short	protection;	/* protection			*/
	short		priority;	/* priority 			*/
	struct management mngmt;	/* management parameters	*/
	char		expd;		/* expedited data: T_YES or	*/
					/*	T_NO			*/
};

/*
 *	ISO connectionless options
 */

struct isocl_options {
	struct	rate	transdel;	/* transit delay		*/
	struct	rate	reserrorrate;	/* residual error rate		*/
	unsigned short	protection;	/* protection			*/
	short		priority;	/* priority			*/
};

/*
 *	TCP-specific environment 
 */

/*
 *	TCP precedence levels
 */

#define	T_ROUTINE	0
#define	T_PRIORITY	1
#define	T_IMMEDIATE	2
#define	T_FLASH		3
#define	T_OVERRIDEFLASH	4
#define	T_CRITIC_ECP	5
#define	T_INETCONTROL	6
#define	T_NETCONTROL	7

/*
 *	TCP security options structure
 */

struct	secoptions {
	short	security;		/* security field */
	short	compartment;		/* compartment */
	short	handling;		/* handling restrictions */
	xtiscalar_t tcc;		/* transmission control code */
};

/*
 *	TCP options
 */

struct	tcp_options {
	short		precedence;	/* precedence */
	xtiscalar_t	timeout;	/* abort timeout */
	xtiscalar_t	max_seg_size;	/* maximum segment size */
	struct secoptions secopt;	/* TCP security options */
};

#if _XOPEN4
/*
 * Flags defines (other info about the transport provider).
 */

#define	T_SENDZERO	0x001	/* supports 0-length TSDUs */

struct	t_opthdr {
	xtiuscalar_t len;	/* total length of option; that is, */
				/* sizeof (struct t_opthdr) + length */
				/* of option value in bytes */
	xtiuscalar_t level;	/* protocol affected */
	xtiuscalar_t name;	/* option name */
	xtiuscalar_t status;	/* status value */
	/* followed by the option value */
};

/*
 * transdel structure
 */
struct transdel {
	struct reqvalue	maxdel;	/* maximum transit delay */
	struct reqvalue	avgdel;	/* average transit delay */
};

/*
 * General definitions for option management
 */
#define	T_UNSPEC	(~0 - 2)  /* applicable to u_long, long, char .. */
#define	T_ALLOPT	0
#define	T_ALIGN(p)	(((xtiuscalar_t)(p) + (sizeof (xtiscalar_t) - 1)) \
				& ~(sizeof (xtiscalar_t) - 1))

#define	OPT_NEXTHDR( pbuf, buflen, popt) \
	(((char *)(popt) + T_ALIGN( (popt)->len ) < \
	(char *)(pbuf) + (buflen)) ? \
	(struct t_opthdr *)((char *)(popt) + T_ALIGN( (popt)->len )) : \
	(struct t_opthdr *)0 )

             /* OPTIONS ON XTI LEVEL */
/* XTI-level */
#define	XTI_GENERIC	0xffff

/*
 * XTI-level Options
 */

#define	XTI_DEBUG	0x0001	/* enable debugging */
#define	XTI_LINGER	0x0080	/* linger on close if data present */
#define	XTI_RCVBUF	0x1002	/* receive buffer size */
#define	XTI_RCVLOWAT	0x1004	/* receive low-water mark */
#define	XTI_SNDBUF	0x1001	/* send buffer size */
#define	XTI_SNDLOWAT	0x1003	/* send low-water mark */

/*
 * Structure used with linger option.
 */
struct t_linger {
	xtiscalar_t l_onoff;	/* option on/off */
	xtiscalar_t l_linger;	/* linger time */
};

/*
 * Protocol Levels
 */
#define	ISO_TP	0x0100
/*
 * Options for Quality of Service and Expedited Data (ISO 8072:1986)
 */
#define	TCO_THROUGHPUT		0x0001
#define	TCO_TRANSDEL		0x0002
#define	TCO_RESERRORRATE	0x0003
#define	TCO_TRANSFFAILPROB	0x0004
#define	TCO_ESTFAILPROB		0x0005
#define	TCO_RELFAILPROB		0x0006
#define	TCO_ESTDELAY		0x0007
#define	TCO_RELDELAY		0x0008
#define	TCO_CONNRESIL		0x0009
#define	TCO_PROTECTION		0x000a
#define	TCO_PRIORITY		0x000b
#define	TCO_EXPD		0x000c
#define	TCL_TRANSDEL		0x000d
#define	TCL_RESERRORRATE	TCO_RESERRORRATE
#define	TCL_PROTECTION		TCO_PROTECTION
#define	TCL_PRIORITY		TCO_PRIORITY

/*
 * Management Options
 */
#define	TCO_LTPDU	0x0100
#define	TCO_ACKTIME	0x0200
#define	TCO_REASTIME	0x0300
#define	TCO_EXTFORM	0x0400
#define	TCO_FLOWCTRL	0x0500
#define	TCO_CHECKSUM	0x0600
#define	TCO_NETEXP	0x0700
#define	TCO_NETRECPTCF	0x0800
#define	TCO_PREFCLASS	0x0900
#define	TCO_ALTCLASS1	0x0a00
#define	TCO_ALTCLASS2	0x0b00
#define	TCO_ALTCLASS3	0x0c00
#define	TCO_ALTCLASS4	0x0d00
#define	TCL_CHECKSUM	TCO_CHECKSUM

             /* INTERNET SPECIFIC ENVIRONMENT */
/*
 * TCP level
 */
#define	INET_TCP	0x6
/*
 *TCP-level Options
 */

/* TCP_NODELAY and TCP_MAXSEG also defined in bsd/netinet/tcp.h */
#define	TCP_NODELAY	0x01	/* don't delay packets to coalesce	*/
#define	TCP_MAXSEG	0x02	/* get maximum segment size		*/

#define	TCP_KEEPALIVE	0x8	/* check, if connections are alive	*/

/*
 * Structure used with TCP_KEEPALIVE option.
 */
struct t_kpalive {
	xtiscalar_t kp_onoff;	/* option on/off */
	xtiscalar_t kp_timeout;	/* timeout in minutes */
};

#define	T_GARBAGE	0x02

/*
 * UDP level
 */

#define	INET_UDP	0x11

/*
 * UDP-level Options
 */

#define	UDP_CHECKSUM TCO_CHECKSUM	/* checksum computation	*/

/*
 * IP level
 */
#define	INET_IP	0x0
/*
 * IP-level Options
 */

/* IP_OPTIONS, IP_TOS, IP_TTL also defined in bsd/netinet/in.h */
#define	IP_OPTIONS 	1	/* IP per-packet options */
#define	IP_TOS		3	/* IP per-packet type of service */
#define	IP_TTL		4	/* IP per-packet time to live	*/

#define	IP_REUSEADDR	29	/* allow local address reuse	*/
#define	IP_DONTROUTE	30	/* just use interface addresses	*/
#define	IP_BROADCAST	31	/* permit sending of broadcast msgs	*/




        /* SPECIFIC ISO OPTION AND MANAGEMENT PARAMETERS */
/*
 * IP_TOS type of service
 */

#define	T_NOTOS		0
#define	T_LDELAY	1<<4
#define	T_HITHRPT	1<<3
#define	T_HIREL		1<<2
#define	SET_TOS(prec, tos)	((0x7 & (prec)) << 5 | (0x1c &(tos)))
#endif	/* _XOPEN4 */

#endif	/* _SYS_XTI_H */
