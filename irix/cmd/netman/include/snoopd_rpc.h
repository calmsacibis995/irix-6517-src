#ifndef SNOOPD_RPC_H
#define SNOOPD_RPC_H
/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Snoopd include file
 *
 *	$Revision: 1.8 $
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

#include <rpc/types.h>

struct nlpacket;
struct snoopstats;
struct histogram;
struct counts;
struct alpacket;

/*
 * RPC Procedure Numbers
 */
#define SNOOPPROG			391000
#define SNOOPVERS			1

#define SNOOPPROC_NULL			0
#define SNOOPPROC_SUBSCRIBE		1
#define SNOOPPROC_UNSUBSCRIBE		2
#define SNOOPPROC_SETSNOOPLEN		3
#define SNOOPPROC_SETERRFLAGS		4
#define SNOOPPROC_ADDFILTER		5
#define SNOOPPROC_DELETEFILTER		6
#define SNOOPPROC_STARTCAPTURE		7
#define SNOOPPROC_STOPCAPTURE		8
#define SNOOPPROC_GETSTATS		9
#define SNOOPPROC_GETADDR		10
#define SNOOPPROC_SETINTERVAL		11

/*
 * Service types
 */
enum svctype {
	SS_NULL = 0,
	SS_NETSNOOP = 1,
	SS_NETLOOK = 2,
	SS_HISTOGRAM = 3,
	SS_ADDRLIST = 4
};

/*
 * Status codes
 */
enum snoopstat {
	SNOOP_OK = 0,
	SNOOPERR_PERM = 1,	/* No permission match */
	SNOOPERR_INVAL = 2,	/* Invalid argument */
	SNOOPERR_NOIF = 3,	/* No such interface */
	SNOOPERR_NOSVC = 4,	/* No such service */
	SNOOPERR_BADF = 5,	/* Illegal filter */
	SNOOPERR_AGAIN = 6,	/* Resource temporarily unavailable */
	SNOOPERR_IO = 7		/* I/O error */
};

/*
 * Structures to XDR
 */
struct subreq {
	char		*sr_interface;
	enum svctype	sr_service;
	u_int		sr_buffer;
	u_int		sr_count;
	u_int		sr_interval;
};

struct intres {
	enum snoopstat	ir_status;
	union {
		int	iru_value;
		char	*iru_message;
	} ir_u;
};

#define	ir_value	ir_u.iru_value
#define	ir_message	ir_u.iru_message

struct expression {
	struct expr	*exp_expr;
	struct protocol	*exp_proto;
};

/*
 * NB:  We import three address family codes, struct sockaddr, and struct
 *	sockaddr_in from SGI's 4.3BSD-based socket header files into this
 *	RPC protocol definition header file.  These numbers and structures
 *	shouldn't change too soon.  When they do, we can spin a version of
 *	SNOOPPROG that uses the new values and types.  Until then, SGI
 *	clients and servers benefit from simpler encoding and decoding.
 */
#if defined(__sgi) || defined(sun)
#define __BSD4_3_SOCKETS 1
#endif

#ifdef __BSD4_3_SOCKETS

#include <sys/socket.h>
#ifndef AF_RAW
#define	AF_RAW		18		/* Raw link layer interface */
#endif

#ifndef PF_RAW
#define	PF_RAW		AF_RAW
#endif

#else	/* !__BSD4_3_SOCKETS */

#define	AF_UNSPEC	0		/* unspecified */
#define	AF_INET		2		/* internetwork: UDP, TCP, etc. */
#define	AF_RAW		18		/* Raw link layer interface */

struct sockaddr {
	u_short	sa_family;		/* address family */
	char	sa_data[14];		/* up to 14 bytes of direct address */
};

struct in_addr {
	u_long	s_addr;
};

struct sockaddr_in {
	short	sin_family;		/* NB: BSD chose short here */
	u_short	sin_port;
	struct	in_addr sin_addr;
	char	sin_zero[8];
};

#endif	/* !__BSD4_3_SOCKETS */

enum getaddrcmd {
	GET_IFADDR = 1,
	GET_IFDSTADDR = 2,
	GET_IFBRDADDR = 3,
	GET_IFNETMASK = 4
};

struct getaddrarg {
	enum getaddrcmd	gaa_cmd;
	struct sockaddr	gaa_addr;
};

struct getaddrres {
	enum snoopstat	gar_status;
	struct sockaddr	gar_addr;
};

/*
 * XDR routines
 */
bool_t xdr_subreq(struct __xdr_s *, struct subreq *);
bool_t xdr_intres(struct __xdr_s *, struct intres *);
bool_t xdr_expression(struct __xdr_s *, struct expression *);
bool_t xdr_histogram(struct __xdr_s *, struct histogram *);
bool_t xdr_counts(struct __xdr_s *, struct counts *);
bool_t xdr_nlpacket(struct __xdr_s *, struct nlpacket *);
bool_t xdr_snoopstats(struct __xdr_s *, struct snoopstats *);
bool_t xdr_sockaddr(struct __xdr_s *, struct sockaddr *);
bool_t xdr_getaddrarg(struct __xdr_s *, struct getaddrarg *);
bool_t xdr_getaddrres(struct __xdr_s *, struct getaddrres *);
bool_t xdr_alpacket(struct __xdr_s *, struct alpacket *);

#endif /* !SNOOPD_RPC_H */
