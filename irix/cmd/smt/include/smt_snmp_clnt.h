#ifndef _SMT_SNMP_CLNT_
#define _SMT_SNMP_CLNT_
/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	A toolkit of common functions for an SNMP client.
 *
 *	$Revision: 1.8 $
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

struct synch_state {
    int	waiting;
    int status;
/* status codes */
#define STAT_SUCCESS	0
#define STAT_ERROR	1
#define STAT_TIMEOUT 2
    int reqid;
    struct snmp_pdu *pdu;
};

struct snmp_session;

extern struct synch_state snmp_synch_state;

extern struct snmp_pdu *snmp_pdu_create(int);
extern void snmp_add_null_var(struct snmp_pdu*, oid*, int);
extern int snmp_synch_input(int, struct snmp_session*, int,
				struct snmp_pdu*,void*);
extern struct snmp_pdu *snmp_fix_pdu(struct snmp_pdu*, int);
extern int snmp_synch_response(struct snmp_session*, struct snmp_pdu*,
					struct snmp_pdu**);
extern void snmp_synch_setup(struct snmp_session*);
extern char *snmp_errstring(int);
#endif
