#ifndef _SMT_SNMP_VARS_H_
#define _SMT_SNMP_VARS_H_
/*
 * Copyright 1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 * Definitions for SNMP (RFC 1067) agent variable finder.
 *
 *	$Revision: 1.7 $
 */

/***********************************************************
	Copyright 1988, 1989 by Carnegie Mellon University

                      All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of CMU not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

CMU DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
CMU BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.
******************************************************************/

#define		MAX_INTERFACES	2
extern	struct mib_ifEntry  mib_ifEntry[MAX_INTERFACES];
extern	struct mib_ip	    mib_ip;
#define		ROUTE_ENTRIES	2
extern	struct mib_udp	    mib_udp;
extern	long	mib_icmpInMsgs;
extern	long	mib_icmpOutMsgs;
extern	long	mib_icmpInErrors;	/* not checked in KIP */
extern	long	mib_icmpOutErrors;	/* not checked in KIP */
extern	long	mib_icmpInCount[];
extern	long	mib_icmpOutCount[];

extern void init_snmp();
extern u_char  *getStatPtr(oid*, int*, u_char*, int*, u_short*, int, int*);
extern int compare(oid*, int, oid*, int);
extern u_char *var_system(struct variable*, oid*, int*, int, int*, int*);
extern u_char *var_ifEntry(struct variable*, oid*, int*, int, int*, int*);
extern u_char *var_atEntry(struct variable*, oid*, int*, int, int*, int*);
extern u_char *var_ip(struct variable*, oid*, int*, int, int*, int*);
extern u_char *var_ipRouteEntry(struct variable*, oid*, int*, int, int*, int*);
extern u_char *var_ipAddrEntry(struct variable*, oid*, int*, int, int*, int*);
extern u_char *var_icmp(struct variable*, oid*, int*, int, int*, int*);
extern u_char *var_udp(struct variable*, oid*, int*, int, int*, int*);

#endif
