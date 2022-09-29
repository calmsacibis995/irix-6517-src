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
static char *rcsid = "$Header: /proj/irix6.5.7m/isms/eoe/cmd/bsd/gated/RCS/sgmp.c,v 1.1 1989/09/18 19:02:21 jleong Exp $";
#endif	not lint

#ifdef	AGENT_SGMP

#include "include.h"
#include "snmp.h"
#include <snmp.h>

int Rt_Gateway();
int Rt_Type();
int Rt_Howlearned();
int Rt_Metric0();
int Egp_Core();
int Egp_As();
int Egp_Neigh_Addr();
int Egp_Neigh_State();
int Egp_Errors();
int Version_Id();
int Version_Rev();

static struct mibtbl sgmptbl[12] = {
	0, { 1, 4, 1, 2, 1 }, Rt_Gateway,
	0, { 1, 4, 1, 2, 2 }, Rt_Type,
	0, { 1, 4, 1, 2, 3 }, Rt_Howlearned,
	0, { 1, 4, 1, 2, 4 }, Rt_Metric0,
	0, { 1, 4, 1, 3, 1 }, Egp_Core,
	0, { 1, 4, 1, 3, 2 }, Egp_As,
	0, { 1, 4, 1, 3, 3, 1 }, Egp_Neigh_Addr,
	0, { 1, 4, 1, 3, 3, 2 }, Egp_Neigh_State,
	0, { 1, 4, 1, 3, 5 }, Egp_Errors,
	0, { 1, 1, 1 }, Version_Id,
	0, { 1, 1, 2 }, Version_Rev,
	0, { 0 } , 0
};


sgmpin(from, size, pkt)
        struct sockaddr *from;
        int size;
        char *pkt;
{
	
        struct sockaddr_in *sin_from = (struct sockaddr_in *)from;
	
	snmp_in_pkt(sin_from, sgmp_socket, size, pkt, sgmptbl);
        return;
}


/*
 *  Register all of our supported variables with SGMPD.
 */
register_sgmp_vars()
{
	
	snmp_register(sgmp_socket, (u_short)AGENT_SGMP_PORT, sgmptbl);
}


int Rt_Gateway(src, dst)
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
	*dst++ = STR;
	*dst++ = sizeof (u_long);
	bcopy((char *)&sock_inaddr(&grte->rt_router).s_addr, dst, sizeof(u_long));
	return(sizeof(u_long) + 2);
}


int Rt_Type(src, dst)
char *src, *dst;
{
	int rttype;
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
	if (grte->rt_state & RTS_HOSTROUTE) {
		rttype = TO_REMOTE_HOST;
	} else if (grte->rt_state & RTS_INTERFACE) {
		rttype = TO_DIRECT_NET;
	} else if (grte->rt_state & (RTS_INTERIOR|RTS_EXTERIOR)) {
		if (grte->rt_state & RTS_SUBNET) {
			rttype = TO_REMOTE_SUBNET;
		} else {
			rttype = TO_REMOTE_NET;
		}
	} else {
		rttype = TO_NOWHERE;
	}
	*dst++ = INT;
	*dst++ = sizeof(rttype);
	bcopy((char *)&rttype, dst, sizeof(rttype));
	return(sizeof(rttype) + 2);
}


int Rt_Howlearned(src, dst)
char *src, *dst;
{
	char *how_learned;
	register struct bits *p;
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
	how_learned = "Unknown";
	for (p = protobits; p->t_bits > (u_int)0; p++) {
		if ( (grte->rt_proto == p->t_bits) ) {
			how_learned = p->t_name;
			break;
		}
	}
	*dst++ = STR;
	*dst++ = strlen(how_learned);
	bcopy(how_learned, dst, strlen(how_learned));
	return(strlen(how_learned) + 2);
}


int Rt_Metric0(src, dst)
char *src, *dst;
{
	int reqmet;
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
			reqmet = mapmetric(HELLO_TO_RIP, grte->rt_metric);
			break;
		case RTPROTO_EGP:
		case RTPROTO_KERNEL:
			reqmet = mapmetric(HELLO_TO_EGP, grte->rt_metric);
			break;
		case RTPROTO_HELLO:
			reqmet = grte->rt_metric;
			break;
		default:
			reqmet = mapmetric(HELLO_TO_RIP, grte->rt_metric);
			break;
	}
	bcopy((char *)&reqmet, dst, sizeof(int));
	return(sizeof(int) + 2);
}


int Egp_Core(src, dst)
char *src, *dst;
{
	int reqmet = CORE_VALUE;
	
	if (!(doing_egp)) {
		*--dst = AGENT_ERR;
		return(1);
	}
	*dst++ = INT;
	*dst++ = sizeof(int);
	bcopy((char *)&reqmet, dst, sizeof(int));
	return(sizeof(int) + 2);
}


int Egp_As(src, dst)
char *src, *dst;
{
	int as;
	
	if (!(doing_egp)) {
		*--dst = AGENT_ERR;
		return(1);
	}
	*dst++ = INT;
	*dst++ = sizeof(int);
	as = (int)mysystem;
	bcopy((char *)&as, dst, sizeof(int));
	return(sizeof(int) + 2);
}


int Egp_Neigh_Addr(src, dst)
char *src, *dst;
{
	int neighnum, cnt;
	struct egpngh *ngp;
	
	if (!(doing_egp)) {
		*--dst = AGENT_ERR;
		return(1);
	}
	neighnum = *src++;
	ngp = egpngh;
	cnt = 1;
	while ((cnt < neighnum) && (ngp != NULL)) {
		ngp = ngp->ng_next;
		cnt++;
	}
	if (ngp == NULL) {
		*--dst = AGENT_ERR;
		return(1);
	}
	*dst++ = STR;
	*dst++ = sizeof(u_long);
	bcopy((char *)&ngp->ng_addr.s_addr, dst, sizeof(u_long));
	return(sizeof(u_long) + 2);
}


int Egp_Neigh_State(src, dst)
char *src, *dst;
{
	int neighnum, cnt;
	char *cp;
	register struct bits *p;
	struct egpngh *ngp;
	
	if (!(doing_egp)) {
		*--dst = AGENT_ERR;
		return(1);
	}
	neighnum = *src++;
	ngp = egpngh;
	cnt = 1;
	while ((cnt < neighnum) && (ngp != NULL)) {
		ngp = ngp->ng_next;
		cnt++;
	}
	if (ngp == NULL) {
		*--dst = AGENT_ERR;
		return(1);
	}
	cp = "Unknown";
	for (p = egp_states; p->t_bits != (u_int) -1; p++) {
		if ( p->t_bits == ngp->ng_state ) {
			cp = p->t_name;
			break;
		}
	}
	*dst++ = STR;
	*dst++ = strlen(cp);
	bcopy(cp, dst, strlen(cp));
	return(strlen(cp) + 2);
}


int Egp_Errors(src, dst)
char *src, *dst;
{
	int errors;
	
	if (!(doing_egp)) {
		*--dst = AGENT_ERR;
		return(1);
	}
	*dst++ = INT;
	*dst++ = sizeof(int);
	errors = egp_stats.inerrors + egp_stats.outerrors;
	bcopy((char *)&errors, dst, sizeof(int));
	return(sizeof(int) + 2);
}


int Version_Id(src, dst)
char *src, *dst;
{
	char version_id[SNMPSTRLEN];
	char *cp;
	int len, cnt, i;
	
	(void) sprintf(version_id, "%s %s compiled %s on %s", my_name, version, build_date, my_hostname);
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
}


int Version_Rev(src, dst)
char *src, *dst;
{
	char version_id[SNMPSTRLEN];
	char *cp;
	u_long	version_rev;
	
	(void) strcpy(version_id, version);
	if ( !isdigit(version_id[strlen(version_id)-1]) ) {
		if ( cp = rindex(version_id, '.') ) {
			*cp = NULL;
		}
	}
	version_rev = ntohl(inet_addr(version_id));
	*dst++ = INT;
	*dst++ = sizeof(version_rev);
	bcopy((char *)&version_rev, dst, sizeof(version_rev));
	return(sizeof(version_rev) + 2);
}

#endif	defined(AGENT_SGMP)

