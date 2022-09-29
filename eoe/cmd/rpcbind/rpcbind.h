/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#ident	"@(#)rpcbind:rpcbind.h	1.2.5.2"

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*	PROPRIETARY NOTICE (Combined)
*
* This source code is unpublished proprietary information
* constituting, or derived under license from AT&T's UNIX(r) System V.
* In addition, portions of such source code were derived from Berkeley
* 4.3 BSD under license from the Regents of the University of
* California.
*
*
*
*	Copyright Notice 
*
* Notice of copyright on this source code product does not indicate 
*  publication.
*
*	(c) 1986,1987,1988,1989,1990  Sun Microsystems, Inc
*	(c) 1983,1984,1985,1986,1987,1988,1989,1990  AT&T.
*	(c) 1990,1991  UNIX System Laboratories, Inc.
*          All rights reserved.
*/ 
/*
 * rpcbind.h
 * The common header declarations
 */

/* Global variables */
extern int debugging;
#ifdef sgi
extern int verbose; 		/* report errors */
extern int do_mcast;		/* register for UDP multicasts */
extern int max_forks;		/* maximum number of callits at once */
#else
#define verbose debugging
#endif
extern RPCBLIST *list_rbl;	/* A list of version 3 rpcbind services */
extern char *loopback_dg;	/* CLTS loopback transport, for set/unset */
extern char *loopback_vc;	/* COTS loopback transport, for set/unset */
extern char *loopback_vc_ord;	/* COTS_ORD loopback transport, for set/unset */
extern int Secure;

#ifdef PORTMAP
extern PMAPLIST *list_pml;	/* A list of version 2 rpcbind services */
extern char *udptrans;		/* Name of UDP transport */
extern char *tcptrans;		/* Name of TCP transport */
extern char *udp_uaddr;		/* Universal UDP address */
extern char *tcp_uaddr;		/* Universal TCP address */
#endif

extern char *mergeaddr();
extern int add_bndlist();
extern bool_t is_bound();
extern void rpcbproc_callit_common();
extern struct netconfig *rpcbind_get_conf();
extern void rpcbind_abort();

#define PMAP_TYPE 1
#define RPCB_TYPE 2


#ifdef sgi
#include <syslog.h>
#ifndef DEBUG
/* an extremely ugly hack, but it works */
# define perror(string) syslog(LOG_DEBUG, "%s: %m", string)
# define fprintf syslog
# undef stderr
# define stderr  LOG_DEBUG
# define SYSLOG syslog
#endif /* DEBUG */

struct oknet {
	struct in_addr  mask;
	struct in_addr  match;
};

extern struct oknet oknets[];
extern int num_oknets;
extern int Aflag;
extern int bflag;
extern int num_local;
extern union addrs {
	char	buf[1];
	struct oknet a[1];
} *addrs;
extern SVCXPRT *m_xprt;
extern SVCXPRT *ludp_xprt, *ltcp_xprt; 

extern bool_t chklocal(struct in_addr taddr);
extern void getlocal(void);
extern int chknet(struct in_addr addr);
#endif /* SGI */
