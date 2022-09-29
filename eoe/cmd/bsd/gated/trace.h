/*
 *   CENTER FOR THEORY AND SIMULATION IN SCIENCE AND ENGINEERING
 *			CORNELL UNIVERSITY
 *
 *      Portions of this software may fall under the following
 *      copyrights: 
 *
 *	Copyright (c) 1983 Regents of the University of California.
 *	All rights reserved.  The Berkeley software License Agreement
 *	specifies the terms and conditions for redistribution.
 *
 *  GATED - based on Kirton's EGP, UC Berkeley's routing daemon (routed),
 *	    and DCN's HELLO routing Protocol.
 *
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/bsd/gated/RCS/trace.h,v 1.1 1989/09/18 19:02:26 jleong Exp $
 *
 */

/*
 * trace.h
 */

#define	TIME_STAMP	60*10	/* Duration between timestamps in seconds */

		/* tracing levels */
#define	TR_INT		0x1		/* internal errors */
#define TR_EXT		0x2		/* external changes resulting from egp */
#define TR_RT		0x4		/* routing changes */
#define TR_EGP		0x8		/* all egp packets sent and received */
#define TR_UPDATE	0x10		/* trace update info sent */
#define TR_RIP		0x20		/* trace update info sent */
#define TR_HELLO	0x40		/* trace update info sent */
#define	TR_JOB		0x80		/* trace dispatching */
#define TR_STAMP	0x100		/* timestamp */
#define	TR_SNMP		0x200		/* SNMP transaction */
#define	TR_ICMP		0x400		/* ICMP packet */
#ifdef	NSS
#define	TR_SPF		0x10000000	/* trace SPF protocol */
#define	TR_NSSRT	0x20000000	/* trace inter-NSS routing */
#define	TR_ESIS		0x40000000	/* trace ES-IS protocol */
#define	TR_ISIS		0x80000000	/* trace IS-IS protocol */
#define	TR_ALL		0xF00007FF	/* trace everything */
#else	NSS
#define	TR_ALL		0x000007FF	/* trace everything */
#endif	NSS

#define TRACE_TRC	if (tracing) printf
#define TRACE_INT	if (tracing & TR_INT) printf
#define TRACE_EXT	if (tracing & TR_EXT) printf
#define TRACE_RT	if (tracing & TR_RT) printf
#define TRACE_EGP	if (tracing & TR_EGP) printf
#define	TRACE_EGPUPD	if ( (tracing & (TR_EGP|TR_UPDATE)) == (TR_EGP|TR_UPDATE) ) printf
#define TRACE_RIP	if (tracing & TR_RIP) printf
#define	TRACE_RIPUPD	if ( (tracing & (TR_RIP|TR_UPDATE)) == (TR_RIP|TR_UPDATE) ) printf
#define TRACE_HEL	if (tracing & TR_HELLO) printf
#define	TRACE_HELUPD	if ( (tracing & (TR_HELLO|TR_UPDATE)) == (TR_HELLO|TR_UPDATE) ) printf
#define	TRACE_JOB	if (tracing & TR_JOB) printf
#define	TRACE_STAMP	if (tracing & TR_STAMP) printf
#define	TRACE_SNMP	if (tracing & TR_SNMP) printf
#define	TRACE_SNMPUPD	if ( (tracing & (TR_SNMP|TR_UPDATE)) == (TR_SNMP|TR_UPDATE) ) printf
#define	TRACE_ICMP	if (tracing & TR_ICMP) printf
#ifdef	NSS
#define TRACE_ISIS	if (tracing & TR_ISIS) printf
#define TRACE_ESIS	if (tracing & TR_ESIS) printf
#define	TRACE_SPF	if (tracing & TR_SPF) printf
#define	TRACE_NSSRT	if (tracing & TR_NSSRT) printf
#endif	NSS

/*
 *  Trace Flags for traceon()
 */
#define RIP_TRACE	0x1
#define HELLO_TRACE	0x2
#define GEN_TRACE	0x4

#define	TRACE_ACTION(action, route) { \
	    if (tracing & TR_RT) \
		traceaction( stdout, "action", route); \
	}

#define TRACE_EGPPKT(comment, src, dst, egp, length) { \
	    if( tracing & TR_EGP) \
		traceegp( "comment", src, dst, egp, length); \
	}
 
#define TRACE_RIPOUTPUT(ifp, dst, size) { \
	    if (tracing & TR_RIP) \
		tracerip("SENT", dst, rip_packet, size); \
	}

#define TRACE_RIPINPUT(ifp, dst, size, ripinfo) { \
	    if (tracing & TR_RIP) \
		tracerip("RECV", dst, ripinfo, size); \
	}
 
#define TRACE_HELLOPKT(comment, src, dst, hello, length, nets) { \
	    if (tracing & TR_HELLO) \
		tracehello( "comment", src, dst, hello, length, nets); \
	}

#define	TRACE_SNMPPKT(comment, dst, packet, length) { \
	if (tracing & TR_SNMP) \
		tracesnmp( "comment", dst, packet, length); \
	}
