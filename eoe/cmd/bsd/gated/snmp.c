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
 */

#ifndef	lint
static char *rcsid = "$Header: /proj/irix6.5.7m/isms/eoe/cmd/bsd/gated/RCS/snmp.c,v 1.1 1989/09/18 19:02:22 jleong Exp $";
#endif	not lint

#if	defined(AGENT_SNMP) || defined(AGENT_SGMP)
#include "include.h"
#include "snmp.h"
#include <snmp.h>

static int snmp_next;
#endif	defined(AGENT_SNMP) || defined(AGENT_SGMP)

#ifdef	AGENT_SNMP

int sysDescr();
int ipRouteMetric();
int ipRouteNextHop();
int ipRouteType();
int ipRouteProto();
int ipRouteAge();
int egpInMsgs();
int egpInErrors();
int egpOutMsgs();
int egpOutErrors();
int egpNeighState();
int egpNeighAddr();
struct egpngh *snmp_lookup_ngp();

static struct mibtbl snmptbl[] = {
/*	0, { 1, 3, 6, 1, 2, 1, 1, 1 }, sysDescr, */
	0, { 1, 3, 6, 1, 2, 1, 4, 21, 1, 3 }, ipRouteMetric,
/*	0, { 1, 3, 6, 1, 2, 1, 4, 21, 1, 7 }, ipRouteNextHop, */
/*	0, { 1, 3, 6, 1, 2, 1, 4, 21, 1, 8 }, ipRouteType, */
	0, { 1, 3, 6, 1, 2, 1, 4, 21, 1, 9 }, ipRouteProto,
	0, { 1, 3, 6, 1, 2, 1, 4, 21, 1, 10 }, ipRouteAge,
	0, { 1, 3, 6, 1, 2, 1, 8, 1 }, egpInMsgs,
	0, { 1, 3, 6, 1, 2, 1, 8, 2 }, egpInErrors,
	0, { 1, 3, 6, 1, 2, 1, 8, 3 }, egpOutMsgs,
	0, { 1, 3, 6, 1, 2, 1, 8, 4 }, egpOutErrors,
/*	0, { 1, 3, 6, 1, 2, 1, 8, 5, 1, 1 }, egpNeighState, */
/*	0, { 1, 3, 6, 1, 2, 1, 8, 5, 1, 2 }, egpNeighAddr, */
	0, { 0 } , 0
};

/*
 *	Process an incoming request from SNMPD.
 */
snmpin(from, size, pkt)
        struct sockaddr *from;
        int size;
        char *pkt;
{
        struct sockaddr_in *sin_from = (struct sockaddr_in *)from;
	
	snmp_in_pkt(sin_from, snmp_socket, size, pkt, snmptbl);
        return;
}


/*
 *  Register all of our supported variables with SNMPD.
 */
register_snmp_vars()
{
	
	snmp_register(snmp_socket, (u_short)AGENT_SNMP_PORT, snmptbl);
}


int sysDescr(src, dst)
char *src, *dst;
{
	char version_id[SNMPSTRLEN];
	char *cp;
	int len, cnt, i;
	
	(void) sprintf(version_id, "%s %s compiled %s on %s",
	                    my_name, version, build_date, my_hostname);
	*dst++ = STR;
	cp = dst;
	*dst++ = strlen(version_id);
	len = (strlen(version_id) + 2);
	bcopy(version_id, dst, strlen(version_id));
	if ( version_kernel ) {
		char *p1, *p2;
		char *lead_in = " under ";
		
		dst += strlen(version_id);
		cnt = SNMPSTRLEN - (strlen(version_id) + strlen(lead_in));
		if ( p1 = index(version_kernel, '#') ) {
			if ( (p2 = index(version_kernel, '(')) && (p2 < p1) ) {
				p1 = p2;
			}
			p1--;
			while ( isspace(*p1) ) {
				p1--;
			}
			i = p1 - version_kernel + 1;
			if ( i < cnt ) {
				cnt = i;
			}
		}
		bcopy(lead_in, dst, strlen(lead_in));
		dst += strlen(lead_in);
		bcopy(version_kernel, dst, cnt);
		cnt += strlen(lead_in);
		*cp += cnt;
		len += cnt;
	}
	return(len);
};


int ipRouteMetric(src, dst)
char *src, *dst;
{
	int metric;
	struct sockaddr_in reqdst;
	struct rt_entry *grte;
	
	bzero((char *)&reqdst, sizeof(reqdst));
	reqdst.sin_family = AF_INET;
	bcopy(src, (char *)&reqdst.sin_addr.s_addr, sizeof(u_long));
	grte = rt_lookup((int)INTERIOR|(int)EXTERIOR, &reqdst);
	if (grte == NULL) {
		*--dst = AGENT_ERR;
		return(1);
	}
	*dst++ = INT;
	*dst++ = sizeof(int);
	switch (grte->rt_proto) {
		case RTPROTO_RIP:
		case RTPROTO_DIRECT:
		case RTPROTO_DEFAULT:
		case RTPROTO_REDIRECT:
			metric = mapmetric(HELLO_TO_RIP, grte->rt_metric);
			break;
		case RTPROTO_EGP:
		case RTPROTO_KERNEL:
			metric = mapmetric(HELLO_TO_EGP, grte->rt_metric);
			break;
		case RTPROTO_HELLO:
			metric = grte->rt_metric;
			break;
		default:
			metric = mapmetric(HELLO_TO_RIP, grte->rt_metric);
			break;
	}
	bcopy((char *)&metric, dst, sizeof(int));
	return(sizeof(int) + 2);
};


int ipRouteNextHop(src, dst)
char *src, *dst;
{
	struct sockaddr_in reqdst;
	struct rt_entry *grte;
	
	bzero((char *)&reqdst, sizeof(reqdst));
	reqdst.sin_family = AF_INET;
	bcopy(src, (char *)&reqdst.sin_addr.s_addr, sizeof(u_long));
	grte = rt_lookup((int)INTERIOR|(int)EXTERIOR, &reqdst);
	if (grte == NULL) {
		*--dst = AGENT_ERR;
		return(1);
	}
	*dst++ = IPADD;
	*dst++ = sizeof (u_long);
	bcopy((char *)&sock_inaddr(&grte->rt_router).s_addr, dst, sizeof(u_long));
	return(sizeof(u_long) + 2);
}


int ipRouteType(src, dst)
char *src, *dst;
{
	int type;
	struct sockaddr_in reqdst;
	struct rt_entry *grte;
	
	bzero((char *)&reqdst, sizeof(reqdst));
	reqdst.sin_family = AF_INET;
	bcopy(src, (char *)&reqdst.sin_addr.s_addr, sizeof(u_long));
	grte = rt_lookup((int)INTERIOR|(int)EXTERIOR, &reqdst);
	if (grte == NULL) {
		*--dst = AGENT_ERR;
		return(1);
	}
	if (grte->rt_metric >= DELAY_INFINITY) {
		type = 2;
	} else {
		switch (grte->rt_proto) {
			case RTPROTO_DIRECT:
				type = 3;
				break;
			default:
				type = 4;
		}
	}

	*dst++ = INT;
	*dst++ = sizeof(int);
	bcopy((char *)&type, dst, sizeof(int));
	return(sizeof(int) + 2);
};


int ipRouteProto(src, dst)
char *src, *dst;
{
	int proto;
	struct sockaddr_in reqdst;
	struct rt_entry *grte;
	
	bzero((char *)&reqdst, sizeof(reqdst));
	reqdst.sin_family = AF_INET;
	bcopy(src, (char *)&reqdst.sin_addr.s_addr, sizeof(u_long));
	grte = rt_lookup((int)INTERIOR|(int)EXTERIOR, &reqdst);
	if (grte == NULL) {
		*--dst = AGENT_ERR;
		return(1);
	}
	if (grte->rt_state & RTS_STATIC) {
		proto = 2;
	} else {
		switch (grte->rt_proto) {
			case RTPROTO_RIP:
				proto = 8;
				break;
			case RTPROTO_HELLO:
				proto = 7;
				break;
			case RTPROTO_EGP:
				proto = 5;
				break;
			case RTPROTO_DIRECT:
				proto = 1;
				break;
			case RTPROTO_REDIRECT:
				proto = 4;
				break;
			case RTPROTO_DEFAULT:
				proto = 1;
				break;
			default:
				proto = 1;
		}
	}

	*dst++ = INT;
	*dst++ = sizeof(int);
	bcopy((char *)&proto, dst, sizeof(int));
	return(sizeof(int) + 2);
};


int ipRouteAge(src, dst)
char *src, *dst;
{
	int age;
	struct sockaddr_in reqdst;
	struct rt_entry *grte;
	
	bzero((char *)&reqdst, sizeof(reqdst));
	reqdst.sin_family = AF_INET;
	bcopy(src, (char *)&reqdst.sin_addr.s_addr, sizeof(u_long));
	grte = rt_lookup((int)INTERIOR|(int)EXTERIOR, &reqdst);
	if (grte == NULL) {
		*--dst = AGENT_ERR;
		return(1);
	}
	age = grte->rt_timer;
	*dst++ = INT;
	*dst++ = sizeof(int);
	bcopy((char *)&age, dst, sizeof(int));
	return(sizeof(int) + 2);
};


int egpInMsgs(src, dst)
char *src, *dst;
{
	u_int msgs;
	
	if (!(doing_egp)) {
		*--dst = AGENT_ERR;
		return(1);
	}
	*dst++ = CNTR;
	*dst++ = sizeof(u_int);
	msgs = egp_stats.inmsgs;
	bcopy((char *)&msgs, dst, sizeof(u_int));
	return(sizeof(u_int) + 2);
};


int egpInErrors(src, dst)
char *src, *dst;
{
	u_int errors;
	
	if (!(doing_egp)) {
		*--dst = AGENT_ERR;
		return(1);
	}
	*dst++ = CNTR;
	*dst++ = sizeof(u_int);
	errors = egp_stats.inerrors;
	bcopy((char *)&errors, dst, sizeof(u_int));
	return(sizeof(u_int) + 2);
};


int egpOutMsgs(src, dst)
char *src, *dst;
{
	u_int msgs;
	
	if (!(doing_egp)) {
		*--dst = AGENT_ERR;
		return(1);
	}
	*dst++ = CNTR;
	*dst++ = sizeof(u_int);
	msgs = egp_stats.outmsgs;
	bcopy((char *)&msgs, dst, sizeof(u_int));
	return(sizeof(u_int) + 2);
};


int egpOutErrors(src, dst)
char *src, *dst;
{
	u_int errors;
	
	if (!(doing_egp)) {
		*--dst = AGENT_ERR;
		return(1);
	}
	*dst++ = CNTR;
	*dst++ = sizeof(u_int);
	errors = egp_stats.outerrors;
	bcopy((char *)&errors, dst, sizeof(u_int));
	return(sizeof(u_int) + 2);
};



int egpNeighState(src, dst)
char *src, *dst;
{
	int state;
	struct egpngh *ngp;
	
	if (!(doing_egp)) {
		*--dst = AGENT_ERR;
		return(1);
	}
	if ( (ngp = snmp_lookup_ngp(src)) == 0 ) {
		*--dst = AGENT_ERR;
		return(1);
	}
	*dst++ = INT;
	*dst++ = sizeof(int);
	state = ngp->ng_state + 1;
	bcopy((char *)&state, dst, sizeof(int));
	return(sizeof(int) + 2);
}


int egpNeighAddr(src, dst)
char *src, *dst;
{
	int addr;
	struct egpngh *ngp;
	
	if (!(doing_egp)) {
		*--dst = AGENT_ERR;
		return(1);
	}
	if ( (ngp = snmp_lookup_ngp(src)) == 0 ) {
		*--dst = AGENT_ERR;
		return(1);
	}
	*dst++ = IPADD;
	*dst++ = sizeof(u_long);
	addr = ngp->ng_addr.s_addr;
	bcopy((char *)&addr, dst, sizeof(u_long));
	return(sizeof(u_long) + 2);
}


struct egpngh *
snmp_lookup_ngp(src)
	char *src;
{
	struct sockaddr_in reqdst;
	struct egpngh *ngp = egpngh;
	
	bzero((char *)&reqdst, sizeof(reqdst));
	reqdst.sin_family = AF_INET;
	bcopy(src, (char *)&reqdst.sin_addr.s_addr, sizeof(u_long));

	if (reqdst.sin_addr.s_addr != (u_long)DEFAULTNET) {
		for (; ngp; ngp = ngp->ng_next) {
			if (ngp->ng_addr.s_addr == reqdst.sin_addr.s_addr) {
				break;
			}
		}
	} 
	if (ngp && snmp_next) {
		ngp = ngp->ng_next;
	}
	return(ngp);
}
#endif	AGENT_SNMP


/*
 *	Routines common to both SGMP and SNMP
 */
 
#if	defined(AGENT_SGMP) || defined(AGENT_SNMP)

/*
 *  Process an incoming request from SNMPD.  Speed is of the essence here.
 *  Not elegance.
 */
int snmp_in_pkt(sin_from, socket, size, pkt, objtbl)
	struct sockaddr_in *sin_from;
        int socket, size;
        char *pkt;
        struct mibtbl objtbl[];
{
        char agntreqpkt[SNMPMAXPKT];
	char *ptr = pkt;
	char *ptr1 = agntreqpkt;
	int rspsize;
	struct mibtbl *mib_ptr;

        TRACE_SNMPPKT(SNMP RECV, sin_from, pkt, size);
        snmp_next = 0;
	switch (*ptr) {
		case AGENT_REG:
		case AGENT_RSP:
		case AGENT_ERR:
			syslog(LOG_ERR, "snmp_in_pkt: unexpected AGENT pkt type");
			TRACE_TRC("snmp_in_pkt: unexpected AGENT pkt type\n");
			return;
		case AGENT_REQN:
			snmp_next = 1;
		case AGENT_REQ:
			ptr += 2;
			*ptr1++ = AGENT_RSP;
			rspsize = 1;
			for (mib_ptr = objtbl; mib_ptr->function; mib_ptr++) {
				if (bcmp(ptr, mib_ptr->object, mib_ptr->length) == 0) {
					break;
				}
			}
			if (mib_ptr->function) {
				ptr += mib_ptr->length;
				rspsize += (mib_ptr->function)(ptr, ptr1);
			} else {
				TRACE_SNMP("error\n");
				agntreqpkt[0] = AGENT_ERR;
				rspsize = 1;
			}
			break;
		default:
			syslog(LOG_ERR, "snmp_in_pkt: invalid AGENT pkt type");
			TRACE_SNMP("snmp_in_pkt: invalid AGENT pkt type\n");
			agntreqpkt[0] = AGENT_ERR;
			rspsize = 1;
			break;
	} /* switch */
	
        TRACE_SNMPPKT(SNMP SEND, sin_from, agntreqpkt, rspsize);
        if (sendto(socket, agntreqpkt, rspsize, 0, (struct sockaddr *)sin_from, sizeof(struct sockaddr_in)) < 0) {
        	p_error("snmp_in: sendto() error");
        }
	return;
}


/*
 *  Register all of our supported variables with SNMPD.
 */
snmp_register(socket, port, objtbl)
	int socket;
	u_short port;
	struct mibtbl objtbl[];
{
        struct sockaddr_in dst;
        char agntpkt[MAXPACKETSIZE];
	char *p = agntpkt;
	int asize, len;
	struct mibtbl *mib_ptr;

	*p++ = 0x01;
	asize = 1;

	for (mib_ptr = objtbl; mib_ptr->function; mib_ptr++) {
		if (mib_ptr->length == 0) {
			mib_ptr->length = strlen(mib_ptr->object);
		}
		*p++ = mib_ptr->length;
		bcopy(mib_ptr->object, p, mib_ptr->length);
		p += mib_ptr->length;
		asize += mib_ptr->length + 1;
	}

        bzero((char *)&dst, sizeof(struct sockaddr_in));
        dst.sin_family = AF_INET;
        dst.sin_port = htons(port);
        dst.sin_addr.s_addr = inet_addr("127.0.0.1");

        TRACE_SNMPPKT(SNMP SEND, &dst, agntpkt, asize);
        if (sendto(socket, agntpkt, asize, 0, (struct sockaddr *)&dst, sizeof(struct sockaddr_in)) < 0) {
        	p_error("snmp_register: sendto() error");
        }
}


struct sockaddr_in snmpaddr;
int
snmp_init()
{
        int snmpinits;

        snmpaddr.sin_family = AF_INET;
        snmpaddr.sin_port = 0;
        snmpinits = get_snmp_socket(AF_INET, SOCK_DGRAM, &snmpaddr);
        if (snmpinits < 0)
                return(ERROR);
        return(snmpinits);
}


/*
 *  Open up the SNMP socket.
 */
int
get_snmp_socket(domain, type, sin)
        int domain, type;
        struct sockaddr_in *sin;
{
        int snmpsocks, on = 1;

        if ((snmpsocks = socket(domain, type, 0)) < 0) {
                p_error("get_snmp_socket: socket");
                return (ERROR);
        }
#ifdef SO_RCVBUF
        on = 48*1024;
        if (setsockopt(snmpsocks,SOL_SOCKET,SO_RCVBUF,(char *)&on,sizeof(on))<0) {
                p_error("setsockopt SO_RCVBUF");
        }
#endif SO_RCVBUF
        if (bind(snmpsocks, sin, sizeof (*sin)) < 0) {
                p_error("get_snmp_socket: bind");
                (void) close(snmpsocks);
                return (ERROR);
        }
        return (snmpsocks);
}

#endif defined(AGENT_SNMP) || defined(AGENT_SGMP)
