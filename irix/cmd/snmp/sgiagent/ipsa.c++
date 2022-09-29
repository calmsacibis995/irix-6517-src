/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	IP sub-agent
 *
 *	$Revision: 1.6 $
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

#include <sys/types.h>
#include <sys/mbuf.h>
#include <sys/ioctl.h>
#include <malloc.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <sys/hashing.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#include <netinet/in_pcb.h>
#include <sys/tcpipstats.h>
#include <sys/sysctl.h>
#include <sys/sysmp.h>
#include <alloca.h>
#include <errno.h>
extern "C" {
#include "exception.h"
};
#include <syslog.h>
#include <unistd.h>
#include <bstring.h>
#include <stdlib.h>
#include <stdio.h>

#include "oid.h"
#include "asn1.h"
#include "snmp.h"
#include "packet.h"
#include "subagent.h"
#include "sat.h"
#include "agent.h"
#include "table.h"
#include "ipsa.h"
#include "ifsa.h"
#include "rt.h"

#define SIN(s) ((struct sockaddr_in *) (&(s)))
#define SINPTR(p)  ((struct sockaddr_in *)(p))

// this prototype no longer needed in sherwood
extern "C" {			// Where's the real one?
extern int rsocket;
}

const subID ipForwarding = 1;
const subID ipDefaultTTL = 2;
const subID ipInReceives = 3;
const subID ipInHdrErrors = 4;
const subID ipInAddrErrors = 5;
const subID ipForwDatagrams = 6;
const subID ipInInUnknownProtos = 7;
const subID ipInDiscards = 8;
const subID ipInDelivers = 9;
const subID ipOutRequests = 10;
const subID ipOutDiscards = 11;
const subID ipOutNoRoutes = 12;
const subID ipReasmTimeout = 13;
const subID ipReasmReqds = 14;
const subID ipReasmOKs = 15;
const subID ipReasmFails = 16;
const subID ipFragOKs = 17;
const subID ipFragFails = 18;
const subID ipFragCreates = 19;
const subID ipAddrTable = 20;
const subID ipAdEntAddr = 1;
const subID ipAdEntIfIndex = 2;
const subID ipAdEntNetMask = 3;
const subID ipAdEntBcastAddr = 4;
const subID ipAdEntReasmMaxSize = 5;
const subID ipAdEntValid = 6;		// Internal
const subID ipRouteTable = 21;
const subID ipRouteDest = 1;
const subID ipRouteIfIndex = 2;
const subID ipRouteMetric1 = 3;
const subID ipRouteMetric2 = 4;
const subID ipRouteMetric3 = 5;
const subID ipRouteMetric4 = 6;
const subID ipRouteNextHop = 7;
const subID ipRouteType = 8;
const subID ipRouteProto = 9;
const subID ipRouteAge = 10;
const subID ipRouteMask = 11;
const subID ipRouteMetric5 = 12;
const subID ipRouteInfo = 13;
const subID ipRouteValid = 14;		// Internal
const subID ipNetToMediaTable = 22;
const subID ipNetToMediaIfIndex = 1;
const subID ipNetToMediaPhysAddress = 2;
const subID ipNetToMediaNetAddress = 3;
const subID ipNetToMediaType = 4;
const subID ipNetToMediaValid = 5;		// Internal
const subID ipRoutingDiscards = 23;

const int forwardGateway = 1;
const int forwardHost = 2;

// const int routeTypeOther = 1;		// Never referenced
const int routeTypeInvalid = 2;
const int routeTypeDirect = 3;
const int routeTypeIndirect = 4;

const int routeProtoOther = 1;
const int routeProtoNetmgmt = 3;
const int routeProtoIcmp = 4;

// const int mediaTypeOther = 1;		// Never referenced
const int mediaTypeInvalid = 2;
const int mediaTypeDynamic = 3;
const int mediaTypeStatic = 4;

// For tables
static ipSubAgent *ipsa;
extern ifSubAgent *ifsa;

#ifdef sgi

struct ifreq ifreq[200];
int seqno = 0;
int install = 1;

u_long
check_netmask(u_long i_addr)
{
    int s;			/* socket fd */
    int len;
    struct ifconf ifconf;
    struct ifreq *ifrp;
    struct ifreq ifnetmask;
    u_long if_mask, if_netnum;
    struct in_addr if_addr;

    s = 0;
    len = 0;
    ifconf.ifc_len = sizeof ifreq;
    ifconf.ifc_req = ifreq;
    if (ioctl(s, SIOCGIFCONF, (caddr_t)&ifconf) < 0 || ifconf.ifc_len <= 0) {
	syslog(LOG_ERR, "'get interface config' ioctl failed (%m)");
	return 0;
    }
    len = ifconf.ifc_len;
    for (ifrp = &ifreq[0]; len > 0; len -= sizeof (struct ifreq), ifrp++) {
	if (SINPTR(&ifrp->ifr_addr)->sin_family == AF_INET) {
	    strncpy(ifnetmask.ifr_name, ifrp->ifr_name,
		    sizeof(ifnetmask.ifr_name));
            if (ioctl(s, SIOCGIFNETMASK, (caddr_t)&ifnetmask) < 0) {
		syslog(LOG_ERR, "'get interface netmask' ioctl failed (%m)");
		return 0;
	    }
	    if_mask = SINPTR(&ifnetmask.ifr_addr)->sin_addr.s_addr;
	    if_addr = SINPTR(&ifrp->ifr_addr)->sin_addr;
	    if_netnum = if_mask & if_addr.s_addr;
	    if( (if_mask & i_addr) == if_netnum)
		return if_mask;
	}
    }
    return 0;
}

/*
 * Return the netmask pertaining to an internet address.
 */
u_long
inet_maskof(u_long inaddr)
{
	register u_long i = ntohl(inaddr);
	register u_long mask, hmask;

	if (i == 0) {
		mask = 0;
	} else if (IN_CLASSA(i)) {
		mask = IN_CLASSA_NET;
	} else if (IN_CLASSB(i)) {
		mask = IN_CLASSB_NET;
	} else
		mask = IN_CLASSC_NET;

	/*
	 * Check whether network is a subnet;
	 * if so, use the modified interpretation of `host'.
	 */
	/****
	for (ifp = ifnet; ifp; ifp = ifp->int_next)
		if ((ifp->int_netmask & i) == ifp->int_net)
			mask = ifp->int_subnetmask;
	***/
	hmask = check_netmask(i);
	if(hmask)
	    mask = hmask;
	return (htonl(mask));
}

#define ADD 1
#define DELETE 2
#define CHANGE 3

int
rtioctl(int action, struct rtuentry *ort)
{
#ifndef RTM_ADD
	if (install == 0)
		return (errno = 0);
	ort->rtu_rtflags = ort->rtu_flags;
	switch (action) {

	case ADD:
		return (ioctl(s, SIOCADDRT, (char *)ort));

	case DELETE:
		return (ioctl(s, SIOCDELRT, (char *)ort));

	default:
		return (-1);
	}
#else /* RTM_ADD */
	struct {
		struct rt_msghdr w_rtm;
		struct sockaddr_in w_dst;
		struct sockaddr w_gate;
#ifdef _HAVE_SA_LEN
		struct sockaddr_in w_netmask;
#else
		struct sockaddr_in_new w_netmask;
#endif
	} w;
#define rtm w.w_rtm

	bzero((char *)&w, sizeof(w));
	rtm.rtm_msglen = sizeof(w);
	rtm.rtm_version = RTM_VERSION;
	rtm.rtm_type = (action == ADD ? RTM_ADD :
				(action == DELETE ? RTM_DELETE : RTM_CHANGE));
#undef rt_dst
	rtm.rtm_flags = ort->rtu_flags;
	rtm.rtm_seq = ++seqno;
	rtm.rtm_addrs = RTA_DST|RTA_GATEWAY;
	bcopy((char *)&ort->rtu_dst, (char *)&w.w_dst, sizeof(w.w_dst));
	bcopy((char *)&ort->rtu_router, (char *)&w.w_gate, sizeof(w.w_gate));
	w.w_dst.sin_family = AF_INET;
#ifdef _HAVE_SA_LEN
	w.w_dst.sin_len = sizeof(w.w_dst);
#endif
	w.w_gate.sa_family = AF_INET;
#ifdef _HAVE_SA_LEN
	w.w_gate.sa_len = sizeof(w.w_gate);
#endif
	if (rtm.rtm_flags & RTF_HOST) {
		rtm.rtm_msglen -= sizeof(w.w_netmask);
	} else {
#ifdef _HAVE_SA_LEN
		register char *cp;
		int len;
#endif

		rtm.rtm_addrs |= RTA_NETMASK;
		w.w_netmask.sin_addr.s_addr =
			inet_maskof(w.w_dst.sin_addr.s_addr);
#ifdef _HAVE_SA_LEN
		for (cp = (char *)(1 + &w.w_netmask.sin_addr);
				    --cp > (char *) &w.w_netmask; )
			if (*cp)
				break;
		len = cp - (char *)&w.w_netmask;
		if (len) {
			len++;
			w.w_netmask.sin_len = len;
			len = 1 + ((len - 1) | (sizeof(long) - 1));
		} else 
			len = sizeof(long);
		rtm.rtm_msglen -= (sizeof(w.w_netmask) - len);
#endif
	}
	errno = 0;
	return (install ? write(rsocket, (char *)&w, rtm.rtm_msglen) : (errno = 0));
#endif  /* RTM_ADD */
}

#if 0
static int
sysctl( int *name, u_int namelen, void *oldp,
	size_t *oldlenp, void *newp, size_t newlen)
{
	static int s = -1;
	struct rtsysctl ctl;

	if (s < 0) {
		s = socket(AF_ROUTE, SOCK_RAW, 0);
		if (s < 0)
			return s;
	}

	ctl.name = name;
	ctl.namelen = namelen;
	ctl.oldp = oldp;
	ctl.oldlen = *oldlenp;
	ctl.newp = newp;
	ctl.newlen = newlen;
	if (0 > ioctl(s, _SIOCRTSYSCTL, &ctl))
		return -1;
	*oldlenp = ctl.oldlen;
	return 0;
}
#endif

#endif /* sgi */

ipSubAgent::ipSubAgent(void)
{
    // Store address for table update routines
    ipsa = this;

    // Export subtree
    subtree = "1.3.6.1.2.1.4";
    int rc = export("ip", &subtree);
    if (rc != SNMP_ERR_genErr) {
	log(LOG_ERR, 0, "error exporting IP subagent");
	return;
    }

    // Define tables
    addrtable = table(ipAddrTable, 4, updateIpAddrTable);
    routetable = table(ipRouteTable, 4, updateIpRouteTable);
    nettomediatable = table(ipNetToMediaTable, 5, updateNetToMediaTable);
}

ipSubAgent::~ipSubAgent(void)
{
    unexport(&subtree);
    delete addrtable;
    delete routetable;
}

int
ipSubAgent::get(asnObjectIdentifier *o, asnObject **a, int *)
{
    // Must be static to work correctly. Otherwise, sysmp gives
    // errno = 14 (Bad address).
    static struct kna kna_s;
    struct kna *kna_p = &kna_s;
    struct ipstat *ipstat_p;

    // Pull out the subID array and length
    subID *subid;
    unsigned int len;
    oid *oi = o->getValue();
    oi->getValue(&subid, &len);

    // Check that the subID array is of valid size
    if (len == sublen)
	return SNMP_ERR_noSuchName;

    // Handle the tables separately
    switch (subid[sublen]) {
	case ipAddrTable:
	    if (len != sublen + 7
		|| subid[sublen + 1] != 1
		|| subid[sublen + 2] == 0
		|| subid[sublen + 2] > ipAdEntReasmMaxSize)
		return SNMP_ERR_noSuchName;
	    updateIpAddrTable();
	    return addrtable->get(oi, a);

	case ipRouteTable:
	    if (len != sublen + 7
		|| subid[sublen + 1] != 1
		|| subid[sublen + 2] == 0
		|| subid[sublen + 2] > ipRouteInfo)
		return SNMP_ERR_noSuchName;
	    updateIpRouteTable();
	    return routetable->get(oi, a);

	case ipNetToMediaTable:
            if (len != sublen + 8
                || subid[sublen + 1] != 1
                || subid[sublen + 2] == 0
                || subid[sublen + 2] > ipNetToMediaType)
                return SNMP_ERR_noSuchName;
            updateNetToMediaTable();
            return nettomediatable->get(oi, a);
    }

    // Check that the subID array is of valid size
    if (len != sublen + 2 || subid[sublen + 1] != 0)
	return SNMP_ERR_noSuchName;

    // Get the pointer to the ipstat structure
    int err = sysmp(MP_SAGET, MPSA_TCPIPSTATS, kna_p, sizeof(struct kna));
    if (err < 0) {
        exc_errlog(LOG_ERR, 0, "ip get: sysmp error: errno=%d", errno);
        // No IP objects will be returned
	return SNMP_ERR_noSuchName;
    }
    ipstat_p = &(kna_p->ipstat);

    // Switch on the subID and assign the value to *a
    switch (subid[sublen]) {
	case ipForwarding :
	    {
		int mib[4], i = 0;
		size_t needed = sizeof(i);

		mib[0] = CTL_NET;
		mib[1] = PF_INET;
		mib[2] = IPPROTO_IP;
		mib[3] = IPCTL_FORWARDING;

		if (sysctl(mib, 4, &i, &needed, NULL, 0) < 0) {
			perror("sysctl");
		}

		if (i != 0)
		    *a = new asnInteger(forwardGateway);
		else
		    *a = new asnInteger(forwardHost);
	    }
	    break; 

	case ipDefaultTTL :
	    *a = new asnInteger(IP_DEFAULT_MULTICAST_TTL);
	    break;

	case ipInReceives :
	    *a = new snmpCounter((unsigned int) ipstat_p->ips_total);
	    break; 

	case ipInHdrErrors :
	    *a = new snmpCounter((unsigned int) (ipstat_p->ips_badhlen
						 + ipstat_p->ips_badsum
						 + ipstat_p->ips_badlen
						 + ipstat_p->ips_badoptions));
	    break;

	case ipInAddrErrors :
	    *a = new snmpCounter((unsigned int) ipstat_p->ips_cantforward);
	    break; 

	case ipForwDatagrams :
	    *a = new snmpCounter ((unsigned int) (ipstat_p->ips_forward
                                       + ipstat_p->ips_cantforward));
	    break; 

	case ipInInUnknownProtos :
	    *a = new snmpCounter((unsigned int) ipstat_p->ips_noproto);
	    break; 

	case ipInDiscards :
	    *a = new snmpCounter(0);		// We can't have these
	    break; 

	case ipInDelivers :
	    *a = new snmpCounter((unsigned int) ipstat_p->ips_delivered);
	    break; 

	case ipOutRequests :
	    *a = new snmpCounter((unsigned int) (ipstat_p->ips_localout
						 - ipstat_p->ips_forward));
	    break; 

	case ipOutDiscards :
	    *a = new snmpCounter((unsigned int) ipstat_p->ips_odropped);
	    break; 

	case ipOutNoRoutes :
	    *a = new snmpCounter((unsigned int) ipstat_p->ips_noroute);
	    break; 

	case ipReasmTimeout :
	    *a = new asnInteger(IPFRAGTTL);
	    break; 

	case ipReasmReqds :
	    *a = new snmpCounter((unsigned int) ipstat_p->ips_fragments);
	    break; 

	case ipReasmOKs :
	    *a = new snmpCounter((unsigned int) ipstat_p->ips_reassembled); 
	    break; 

	case ipReasmFails :
	    *a = new snmpCounter((unsigned int) (ipstat_p->ips_fragdropped
						 + ipstat_p->ips_fragtimeout));
	    break; 

	case ipFragOKs :
	    *a = new snmpCounter((unsigned int) ipstat_p->ips_fragmented);
	    break; 

	case ipFragFails :
	    *a = new snmpCounter((unsigned int) ipstat_p->ips_cantfrag);
	    break; 

	case ipFragCreates :
	    *a = new snmpCounter((unsigned int) ipstat_p->ips_fragmented);
	    break; 

	case ipRoutingDiscards:
	    // XXX - Do we do this?
	    *a = new snmpCounter(0);
	    break;

	default :
	    return SNMP_ERR_noSuchName;
    }

    return SNMP_ERR_noError;
}

int
ipSubAgent::getNext(asnObjectIdentifier *o, asnObject **a, int *t)
{
    return simpleGetNext(o, a, t, ipRoutingDiscards);
}

int
ipSubAgent::set(asnObjectIdentifier *o, asnObject **a, int *)
{
    subID *subid, s;
    unsigned int len;
    int rc;
    asnObject *ao;

    // Pull out the subID array and length
    oid *oi = o->getValue();
    oi->getValue(&subid, &len);

    // Check that the subID array is of valid size
    if (len == sublen)
	return SNMP_ERR_noSuchName;

    // Handle the tables separately
    switch (subid[sublen]) {
	case ipRouteTable:
	    if (len != sublen + 7
		|| subid[sublen + 1] != 1
		|| subid[sublen + 2] == 0
		|| subid[sublen + 2] > ipRouteInfo)
		return SNMP_ERR_noSuchName;

	    // Update the table
	    updateIpRouteTable();

	    // Check if the entry exists
	    rc = routetable->get(oi, &ao);
	    if (rc == SNMP_ERR_noSuchName) {
		// Add a new row
#if _MIPS_SZLONG == 64
                unsigned int addr;
#else
		unsigned long addr;
#endif
		snmpIPaddress p;
		asnInteger i;
		asnObjectIdentifier aoi = "0.0";
		subID s = subid[sublen + 2];

		// Set up new row
		subid[sublen + 2] = ipRouteDest;
		addr = (subid[sublen + 3] & 0xFF) << 24;
		addr |= (subid[sublen + 4] & 0xFF) << 16;
		addr |= (subid[sublen + 5] & 0xFF) << 8;
		addr |= subid[sublen + 6] & 0xFF;
		p = addr;
		ao = &p;
		routetable->set(oi, &ao);

		subid[sublen + 2] = ipRouteIfIndex;
		i = 0;
		ao = &i;
		routetable->set(oi, &ao);

		// XXX - Routing metrics?
		subid[sublen + 2] = ipRouteMetric1;
		i = -1;
		routetable->set(oi, &ao);

		subid[sublen + 2] = ipRouteMetric2;
		routetable->set(oi, &ao);

		subid[sublen + 2] = ipRouteMetric3;
		routetable->set(oi, &ao);

		subid[sublen + 2] = ipRouteMetric4;
		routetable->set(oi, &ao);

		subid[sublen + 2] = ipRouteMetric5;
		routetable->set(oi, &ao);

		subid[sublen + 2] = ipRouteType;
		i = routeTypeInvalid;
		routetable->set(oi, &ao);

		subid[sublen + 2] = ipRouteProto;
		i = routeProtoNetmgmt;
		routetable->set(oi, &ao);

		subid[sublen + 2] = ipRouteAge;
		i = 0;
		routetable->set(oi, &ao);

		subid[sublen + 2] = ipRouteValid;
		routetable->set(oi, &ao);

		subid[sublen + 2] = ipRouteNextHop;
		ao = &p;
		routetable->set(oi, &ao);

		subid[sublen + 2] = ipRouteMask;
		routetable->set(oi, &ao);

		subid[sublen + 2] = ipRouteInfo;
		ao = &aoi;
		routetable->set(oi, &ao);

		subid[sublen + 2] = s;
		ao = 0;
	    }

	    switch (subid[sublen + 2]) {
		case ipRouteDest:
		case ipRouteNextHop:
		    delete ao;

		    // Check for correct type
		    if ((*a)->getTag() != TAG_IPADDRESS)
			return SNMP_ERR_badValue;

		    // Only allow setting an invalid route
		    s = subid[sublen + 2];
		    subid[sublen + 2] = ipRouteType;
		    rc = routetable->get(oi, &ao);
		    if (rc != SNMP_ERR_noError) {
			subid[sublen + 2] = s;
			return rc;
		    }
		    subid[sublen + 2] = s;
		    if (((asnInteger *) ao)->getValue() != routeTypeInvalid) {
			delete ao;
			return SNMP_ERR_badValue;
		    }

		    // Set it in the agent
		    delete ao;
		    return routetable->set(oi, a);

		case ipRouteIfIndex:
		case ipRouteMask:
		    // We don't allow this to be set
		    return SNMP_ERR_noSuchName;

		case ipRouteMetric1:
		case ipRouteMetric2:
		case ipRouteMetric3:
		case ipRouteMetric4:
		case ipRouteMetric5:
		case ipRouteAge:
		    delete ao;

		    // Check for correct type
		    if ((*a)->getTag() != TAG_INTEGER)
			return SNMP_ERR_badValue;

		    // Only allow setting an invalid route
		    s = subid[sublen + 2];
		    subid[sublen + 2] = ipRouteType;
		    rc = routetable->get(oi, &ao);
		    if (rc != SNMP_ERR_noError) {
			subid[sublen + 2] = s;
			return rc;
		    }
		    subid[sublen + 2] = s;
		    if (((asnInteger *) ao)->getValue() != routeTypeInvalid) {
			delete ao;
			return SNMP_ERR_badValue;
		    }

		    // Set it in the agent
		    delete ao;
		    return routetable->set(oi, a);

		case ipRouteType:
		    // Check for correct type
		    if ((*a)->getTag() != TAG_INTEGER) {
			delete ao;
			return SNMP_ERR_badValue;
		    }

		    if (((asnInteger *) ao)->getValue() == routeTypeInvalid) {
			// Add a new route
#ifdef sgi
			struct rtuentry rt;
#else
			struct rtentry rt;
#endif
			bzero(&rt, sizeof rt);
			delete ao;

			// Check that new value is legal
			if (((asnInteger *) *a)->getValue() == routeTypeInvalid)
			    return SNMP_ERR_badValue;

			// Fill in the rtentry
#ifdef sgi
			rt.rtu_rtflags = RTF_UP;
			rt.rtu_rtflags |= ((asnInteger *) *a)->getValue() ==
							routeTypeIndirect ?
					RTF_GATEWAY : RTF_HOST;
#else
			rt.rt_flags = RTF_UP;
			rt.rt_flags |= ((asnInteger *) *a)->getValue() ==
							routeTypeIndirect ?
					RTF_GATEWAY : RTF_HOST;
#endif
			subid[sublen + 2] = ipRouteDest;
			rc = routetable->get(oi, &ao);
			if (rc != SNMP_ERR_noError) {
			    subid[sublen + 2] = ipRouteType;
			    return rc;
			}
#ifdef sgi
			SIN(rt.rtu_dst)->sin_family = AF_INET;
			SIN(rt.rtu_dst)->sin_addr.s_addr = (unsigned int)
					((snmpIPaddress *) ao)->getValue();
#else
			SIN(rt.rt_dst)->sin_family = AF_INET;
			SIN(rt.rt_dst)->sin_addr.s_addr = (unsigned int)
					((snmpIPaddress *) ao)->getValue();
#endif
			delete ao;
			subid[sublen + 2] = ipRouteNextHop;
			rc = routetable->get(oi, &ao);
			if (rc != SNMP_ERR_noError) {
			    subid[sublen + 2] = ipRouteType;
			    return rc;
			}
#ifdef sgi
			SIN(rt.rtu_router)->sin_family = AF_INET;
			SIN(rt.rtu_router)->sin_addr.s_addr = (unsigned int)
					((snmpIPaddress *) ao)->getValue();
#else
			SIN(rt.rt_gateway)->sin_family = AF_INET;
			SIN(rt.rt_gateway)->sin_addr.s_addr = (unsigned int)
					((snmpIPaddress *) ao)->getValue();
#endif
			delete ao;
			subid[sublen + 2] = ipRouteType;

			// Add to kernel route table
#ifdef sgi
			if(rtioctl(ADD, &rt) < 0) {
#else
			int sock = socket(PF_INET, SOCK_RAW, 0);
			if (sock < 0 || ioctl(sock, SIOCADDRT, &rt) < 0) {
			    close(sock);
#endif
			    return SNMP_ERR_noSuchName;
			}
#ifndef sgi
			close(sock);
#endif
			return routetable->set(oi, a);
		    } else {
#ifdef sgi
			struct rtuentry rt;
#else
			struct rtentry rt;
#endif

			// Only allow deleting an existing route.
			if (((asnInteger*)*a)->getValue() != routeTypeInvalid) {
			    delete ao;
			    return SNMP_ERR_badValue;
			}

			// Fill in the rtentry
			bzero(&rt, sizeof rt);
#ifdef sgi
			rt.rtu_rtflags = RTF_UP;
			rt.rtu_rtflags |= ((asnInteger *) ao)->getValue() ==
							routeTypeIndirect ?
					RTF_GATEWAY : RTF_HOST;
#else
			rt.rt_flags = RTF_UP;
			rt.rt_flags |= ((asnInteger *) ao)->getValue() ==
							routeTypeIndirect ?
					RTF_GATEWAY : RTF_HOST;
#endif
			delete ao;
			subid[sublen + 2] = ipRouteDest;
			rc = routetable->get(oi, &ao);
			if (rc != SNMP_ERR_noError) {
			    subid[sublen + 2] = ipRouteType;
			    return rc;
			}
#ifdef sgi
			SIN(rt.rtu_dst)->sin_family = AF_INET;
			SIN(rt.rtu_dst)->sin_addr.s_addr = (unsigned int)
					((snmpIPaddress *) ao)->getValue();
#else
			SIN(rt.rt_dst)->sin_family = AF_INET;
			SIN(rt.rt_dst)->sin_addr.s_addr = (unsigned int)
					((snmpIPaddress *) ao)->getValue();
#endif
			delete ao;
			subid[sublen + 2] = ipRouteNextHop;
			rc = routetable->get(oi, &ao);
			if (rc != SNMP_ERR_noError) {
			    subid[sublen + 2] = ipRouteType;
			    return rc;
			}
#ifdef sgi
			SIN(rt.rtu_router)->sin_family = AF_INET;
			SIN(rt.rtu_router)->sin_addr.s_addr = (unsigned int)
					((snmpIPaddress *) ao)->getValue();
#else
			SIN(rt.rt_gateway)->sin_family = AF_INET;
			SIN(rt.rt_gateway)->sin_addr.s_addr = (unsigned int)
					((snmpIPaddress *) ao)->getValue();
#endif
			delete ao;

			// Delete from kernel route table
#ifdef sgi
			if(rtioctl(DELETE, &rt) < 0) {
#else
			int sock = socket(PF_INET, SOCK_RAW, 0);
			if (sock < 0 || ioctl(sock, SIOCDELRT, &rt) < 0) {
			    close(sock);
#endif
			    subid[sublen + 2] = ipRouteType;
			    return SNMP_ERR_noSuchName;
			}
#ifndef sgi
			close(sock);
#endif

			// Delete from agent table
			ao = 0;
			for (s = ipRouteDest; s <= ipRouteValid; s++) {
			    subid[sublen + 2] = s;
			    routetable->set(oi, &ao);
			}

			subid[sublen + 2] = ipRouteType;
			return SNMP_ERR_noError;
		    }
	    }

	case ipNetToMediaTable:
	    if (len != sublen + 8
                || subid[sublen + 1] != 1
                || subid[sublen + 2] == 0
                || subid[sublen + 2] > ipNetToMediaType)
		return SNMP_ERR_noSuchName;

	    // Update the table
	    updateNetToMediaTable();

	    // Check if the entry exists
	    rc = nettomediatable->get(oi, &ao);
	    if (rc == SNMP_ERR_noSuchName) {
		// Add a new row
#if _MIPS_SZLONG == 64
                unsigned int addr;
#else
		unsigned long addr;
#endif
		snmpIPaddress p;
		asnInteger i;
		asnObjectIdentifier aoi = "0.0";
		asnOctetString physaddr = "0:0:0:0:0:0";
		subID s = subid[sublen + 2];

		// Set up new row
		subid[sublen + 2] = ipNetToMediaPhysAddress;
		ao = &physaddr;
		nettomediatable->set(oi, &ao);

		subid[sublen + 2] = ipNetToMediaNetAddress;
		addr = (subid[sublen + 4] & 0xFF) << 24;
		addr |= (subid[sublen + 5] & 0xFF) << 16;
		addr |= (subid[sublen + 6] & 0xFF) << 8;
		addr |= subid[sublen + 7] & 0xFF;
		p = addr;
		ao = &p;
		nettomediatable->set(oi, &ao);

		subid[sublen + 2] = ipNetToMediaIfIndex;
		i = subid[sublen + 3];
		ao = &i;
		nettomediatable->set(oi, &ao); 

		subid[sublen + 2] = ipNetToMediaValid;
		nettomediatable->set(oi, &ao); 

		subid[sublen + 2] = ipNetToMediaType;
		i = mediaTypeInvalid;
		nettomediatable->set(oi, &ao);

		subid[sublen + 2] = s;
		ao = 0;
	    }

	    switch (subid[sublen + 2]) {
		case ipNetToMediaIfIndex: // We don't allow this to be set
		    delete ao;
		    return SNMP_ERR_badValue;

		case ipNetToMediaPhysAddress:
		    delete ao;

		    // Check for correct type
		    if ((*a)->getTag() != TAG_OCTET_STRING)
			return SNMP_ERR_badValue;

		    s = subid[sublen + 2];
		    subid[sublen + 2 ] = ipNetToMediaType;
		    rc = nettomediatable->get(oi, &ao);
		    if (rc != SNMP_ERR_noError) {
			subid[sublen + 2] = s;
			return rc;
		    }
		    subid[sublen + 2] = s;
		    if (((asnInteger *) ao)->getValue() != mediaTypeInvalid) {
			delete ao;
			return SNMP_ERR_badValue;
		    }

		    // Set it in the agent
		    delete ao;
		    return nettomediatable->set(oi, a);

		case ipNetToMediaNetAddress:
		    delete ao;

		    // Check for correct type
		    if ((*a)->getTag() != TAG_IPADDRESS)
			return SNMP_ERR_badValue;

		    s = subid[sublen + 2];
		    subid[sublen + 2 ] = ipNetToMediaType;
		    rc = nettomediatable->get(oi, &ao);
		    if (rc != SNMP_ERR_noError) {
			subid[sublen + 2] = s;
			return rc;
		    }
		    subid[sublen + 2] = s;
		    if (((asnInteger *) ao)->getValue() != mediaTypeInvalid) {
			delete ao;
			return SNMP_ERR_badValue;
		    }

		    // Set it in the agent
		    delete ao;
		    return nettomediatable->set(oi, a);

		case ipNetToMediaType:
		    // Check for correct type
		    if ((*a)->getTag() != TAG_INTEGER) {
			delete ao;
			return SNMP_ERR_badValue;
		    }

		    if (((asnInteger *) ao)->getValue() == mediaTypeInvalid) {
			// Add a new entry
			struct arpreq arpreq;
			bzero(&arpreq, sizeof arpreq);
			delete ao;

			// Check that new value is legal
			if (((asnInteger *) *a)->getValue() == mediaTypeInvalid)
			    return SNMP_ERR_badValue;

			// Fill in the arp table entry
			struct sockaddr_in *sin;

			subid[sublen + 2] = ipNetToMediaNetAddress;
			rc = nettomediatable->get(oi, &ao);
			if (rc != SNMP_ERR_noError) {
			    subid[sublen + 2] = ipNetToMediaType;
			    return rc;
			}

			sin = (struct sockaddr_in *)&arpreq.arp_pa;
			sin->sin_family = AF_INET;
			sin->sin_addr.s_addr =  (unsigned int)((snmpIPaddress *) ao)->getValue();
			delete ao;

			subid[sublen + 2] = ipNetToMediaPhysAddress;
			rc = nettomediatable->get(oi, &ao);
			if (rc != SNMP_ERR_noError) {
			    subid[sublen + 2] = ipNetToMediaType;
			    return rc;
			}
			bcopy(((asnOctetString *) ao)->getValue(),
			      arpreq.arp_ha.sa_data,
			      ((asnOctetString *) ao)->getLength()); 
			delete ao;
			subid[sublen + 2] = ipNetToMediaType;

			// Add arp entry to kernel
			int sock = socket(PF_INET, SOCK_RAW, 0);
			if (sock < 0 || ioctl(sock, SIOCSARP, &arpreq) < 0) {
			    close(sock);
			    return SNMP_ERR_noSuchName;
			}
			close(sock);
			return SNMP_ERR_noError;
		    } else {
			struct arpreq arpreq;
			bzero (&arpreq, sizeof arpreq);

			// Only allow deleting an existing entry
			if (((asnInteger *) *a)->getValue() != mediaTypeInvalid) {
			    delete ao;
			    return SNMP_ERR_badValue;
			}

			// Fill in the arp table entry
			struct sockaddr_in *sin;

			subid[sublen + 2] = ipNetToMediaNetAddress;
			rc = nettomediatable->get(oi, &ao);
			if (rc != SNMP_ERR_noError) {
			    subid[sublen + 2] = ipNetToMediaType;
			    return rc;
			}

			sin = (struct sockaddr_in *)&arpreq.arp_pa;
			sin->sin_family = AF_INET;
			sin->sin_addr.s_addr =  (unsigned int)((snmpIPaddress *) ao)->getValue();
			delete ao;

			subid[sublen + 2] = ipNetToMediaPhysAddress;
			rc = nettomediatable->get(oi, &ao);
			if (rc != SNMP_ERR_noError) {
			    subid[sublen + 2] = ipNetToMediaType;
			    return rc;
			}
			bcopy(((asnOctetString *) ao)->getValue(),
			      arpreq.arp_ha.sa_data,
			      ((asnOctetString *) ao)->getLength());
			delete ao;

			// Delete arp entry from kernel
			int sock = socket(PF_INET, SOCK_RAW, 0);
			if (sock < 0 || ioctl(sock, SIOCDARP, &arpreq) < 0) {
			    close(sock);
			    subid[sublen + 2] = ipNetToMediaType;
			    return SNMP_ERR_noSuchName;
			}
			close(sock);

			// Delete from the agent table
			ao = 0;
			for (s = ipNetToMediaIfIndex;
					s <= ipNetToMediaValid; s++) {
			    subid[sublen + 2] = s;
			    nettomediatable->set(oi, &ao);
			}

			subid[sublen + 2] = ipNetToMediaType;
			return SNMP_ERR_noError;
		    }

	      }

	case ipForwarding:
	    // IRIX can't switch on the fly
	    // XXX - Can it?
	    return SNMP_ERR_noSuchName;

	case ipDefaultTTL:
	    // This is a defined constant in IRIX
	    return SNMP_ERR_noSuchName;
    }

    return SNMP_ERR_noSuchName;
}

void
updateIpAddrTable(void)
{
	// Only update once per packet
	extern unsigned int packetCounter;
	static unsigned int validCounter = 0;
	if (validCounter == packetCounter) {
		return;
	}
	validCounter = packetCounter;

	char *buf, *next, *lim;
	struct ifa_msghdr *ifam;
	oid oi = "1.3.6.1.2.1.4.20.1.6.0.0.0.0";	// ipAddrTable
	asnObject *a;
	snmpIPaddress p;
	asnInteger i;
	int mib[6];
	subID *subid;
	unsigned int len;
	struct sockaddr *sa;
	char *cp;
	size_t needed;
	int rc;

	oi.getValue(&subid, &len);

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = 0;
	mib[4] = NET_RT_IFLIST;
	mib[5] = 0;

	if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0) {
		perror("sysctl");
		return;
	}
	buf = (char *)alloca(needed);
	if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0) {
		perror("sysctl");
		return;
	}

	lim = buf + needed;
	for (next = buf; next < lim; next += ifam->ifam_msglen) {
		struct sockaddr_in *dest = 0, *mask = 0, *addr = 0;
		ifam = (struct ifa_msghdr *)next;
		if (ifam->ifam_type == RTM_IFINFO) {
			continue;
		}

		if (ifam->ifam_addrs & RTA_NETMASK) {
			mask = (struct sockaddr_in *)(ifam + 1);
		}
		if (ifam->ifam_addrs & RTA_IFA) {
#define ROUNDUP(a) ((a) > 0 ? (1 + (((a) - 1) | (sizeof(__uint64_t) - 1))) \
	: sizeof(__uint64_t))
#ifdef _HAVE_SA_LEN
#define ADVANCE(x, n) (x += ROUNDUP((n)->sa_len))
#else
#define ADVANCE(x, n) (x += ROUNDUP(_FAKE_SA_LEN_DST(n)))
#endif
			cp = (char *)mask;
			sa = (struct sockaddr *)mask;
			ADVANCE(cp, sa);
			addr = (struct sockaddr_in *)cp;
		}
		if (ifam->ifam_addrs & RTA_BRD) {
			dest = addr + 1;
		}

		subid[len - 4] = (subID)((addr->sin_addr.s_addr &
		    0xFF000000) >> 24);
		subid[len - 3] = (subID)((addr->sin_addr.s_addr &
		    0x00FF0000) >> 16);
		subid[len - 2] = (subID)((addr->sin_addr.s_addr &
		    0x0000FF00) >> 8);
		subid[len - 1] = (subID)(addr->sin_addr.s_addr &
		    0x000000FF);

		// Set the variables
		subid[len - 5] = ipAdEntAddr;
		p.setValue(addr->sin_addr.s_addr);
		a = &p;
		ipsa->addrtable->set(&oi, &a);

		subid[len - 5] = ipAdEntNetMask;
		p.setValue(mask->sin_addr.s_addr);
		ipsa->addrtable->set(&oi, &a);

		subid[len - 5] = ipAdEntBcastAddr;
		p.setValue(dest->sin_addr.s_addr);
		ipsa->addrtable->set(&oi, &a);

		subid[len - 5] = ipAdEntIfIndex;
		i = ifam->ifam_index;
		a = &i;
		ipsa->addrtable->set(&oi, &a);

		subid[len - 5] = ipAdEntReasmMaxSize;
		i = 65535;
		ipsa->addrtable->set(&oi, &a);

		subid[len - 5] = ipAdEntValid;
		i = validCounter;
		ipsa->addrtable->set(&oi, &a);
	}

	// Remove invalid entries
	subid[len - 4] = subid[len - 3] = subid[len - 2] = subid[len - 1] = 0;
	// subid[len - 5] equals ipAdEntValid
	for ( ; ; ) {
		rc = ipsa->addrtable->formNextOID(&oi);
		if (rc != SNMP_ERR_noError) {
			break;
		}
		rc = ipsa->addrtable->get(&oi, &a);
		if (rc == SNMP_ERR_noError
		    && ((asnInteger *) a)->getValue() == validCounter) {
			delete a;
			continue;
		}
		if (a != 0) {
			delete a;
			a = 0;
		}

		// Entries is invalid, delete it
		for (subID s = ipAdEntAddr; s <= ipAdEntValid; s++) {
			subid[len - 5] = s;
			ipsa->addrtable->set(&oi, &a);
		}
	}
}

void
updateIpRouteTable(void)
{
    size_t needed;
    int	mib[6];
    char *buf, *next, *lim;
    struct rt_msghdr *rtmsg;
    struct sockaddr *sa;
    struct k_rtentry {
	struct	sockaddr k_rt_dst;	/* key */
	struct	sockaddr k_rt_gateway;	/* value */
	struct	sockaddr k_rt_netmask;	/* mask */
	short	k_rt_flags;		/* up/down?, host/net */
    } k_rt;
    struct k_rtentry *rt = &k_rt;
  
    // If packets are coming every fastRequestTime, hold off updating
    // until mustUpdateTime.

    const int mustUpdateTime = 30;
    const int fastRequestTime = 5;

    extern unsigned int packetCounter;

    static unsigned int validCounter = 0;
    static time_t lastRequestTime = 0;
    static time_t lastUpdateTime = 0;

    if (validCounter == packetCounter)
	return;
    validCounter = packetCounter;
    time_t t = time(0);
    if (t - lastUpdateTime < mustUpdateTime
	&& t - lastRequestTime <= fastRequestTime) {
	lastRequestTime = t;
	return;
    }
    lastUpdateTime = lastRequestTime = t;

    subID *subid, *sub;
    unsigned int len, l;
    unsigned int addr;
    int rc;
    asnObject *a;
    asnInteger i;
    snmpIPaddress p;
    asnObjectIdentifier ao = "0.0";
    oid oi = "1.3.6.1.2.1.4.21.1.1.0.0.0.0";	// ipRouteTable
    oi.getValue(&subid, &len);
    oid aoi = "1.3.6.1.2.1.2.2.1.24.0";		// if_addr
    aoi.getValue(&sub, &l);

    mib[0] = CTL_NET;
    mib[1] = PF_ROUTE;
    mib[2] = 0;
    mib[3] = 0;
    mib[4] = NET_RT_DUMP;
    mib[5] = 0;

    if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0) {
        exc_errlog(LOG_ERR, 0, "route-sysctl-estimate =%d", errno);
	return;
    }
    buf = (char *)alloca(needed);
    if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0) {
	exc_errlog(LOG_ERR, 0, "sysctl of routing table =%d", errno);
	return;
    }

    lim  = buf + needed;
    for (next = buf; next < lim; next += rtmsg->rtm_msglen) {
	rtmsg = (struct rt_msghdr *)next;
	/* skip it if do not have both gateway and dst */
	if ((rtmsg->rtm_addrs & (RTA_DST|RTA_GATEWAY)) ==
	  (RTA_DST|RTA_GATEWAY)) {
	    sa = (struct sockaddr*)(rtmsg + 1);
#ifdef _HAVE_SA_LEN
	    bcopy(sa, sa->sa_len, &rt->k_rt_dst);
	    sa = (struct sockaddr*)(sa->sa_len + (char*)sa);
	    bcopy(sa, sa->sa_len, &rt->k_rt_gateway);
	    if (rtmsg->rtm_addrs & RTA_NETMASK) {
		sa = (struct sockaddr*)(sa->sa_len + (char*)sa);
		bcopy(sa, sa->sa_len, &rt->k_rt_netmask);
	    }
#else
	    rt->k_rt_dst = sa[0];
	    rt->k_rt_gateway = sa[1];
	    if (rtmsg->rtm_addrs & RTA_NETMASK) {
	        rt->k_rt_netmask = sa[2];
	    }
#endif
	    rt->k_rt_flags =
	      rtmsg->rtm_flags & (RTF_UP | RTF_GATEWAY | RTF_HOST);
	    if (rt->k_rt_gateway.sa_family != AF_INET)
		continue;
	}

	// Form the oid
	addr = SIN(rt->k_rt_dst)->sin_addr.s_addr;
	subid[len - 4] = (subID) ((addr & 0xFF000000) >> 24);
	subid[len - 3] = (subID) ((addr & 0x00FF0000) >> 16);
	subid[len - 2] = (subID) ((addr & 0x0000FF00) >> 8);
	subid[len - 1] = (subID) (addr & 0x000000FF);

	// Set the variables
	subid[len - 5] = ipRouteDest;
	p = addr;
	a = &p;
	ipsa->routetable->set(&oi, &a);

	subid[len - 5] = ipRouteIfIndex;
	i = (unsigned)rtmsg->rtm_index;
	a = &i;
	ipsa->routetable->set(&oi, &a);

	subid[len - 5] = ipRouteMetric1;
	i = -1;
	ipsa->routetable->set(&oi, &a);

	subid[len - 5] = ipRouteMetric2;
	i = -1;
	ipsa->routetable->set(&oi, &a);

	subid[len - 5] = ipRouteMetric3;
	i = -1;
	ipsa->routetable->set(&oi, &a);

	subid[len - 5] = ipRouteMetric4;
	i = -1;
	ipsa->routetable->set(&oi, &a);

	subid[len - 5] = ipRouteMetric5;
	i = -1;
	ipsa->routetable->set(&oi, &a);

	subid[len - 5] = ipRouteType;
	i = (rt->k_rt_flags & RTF_GATEWAY) ?
		    	routeTypeIndirect : routeTypeDirect;
	ipsa->routetable->set(&oi, &a);

	subid[len - 5] = ipRouteAge;
	i = 0;
	ipsa->routetable->set(&oi, &a);

	subid[len - 5] = ipRouteValid;
	i = validCounter;
	ipsa->routetable->set(&oi, &a);

	subid[len - 5] = ipRouteProto;
	if (rt->k_rt_flags & RTF_DYNAMIC || rt->k_rt_flags & RTF_MODIFIED) {
	    i = routeProtoIcmp;
	    ipsa->routetable->set(&oi, &a);
	} else {
	    rc = ipsa->routetable->get(&oi, &a);
	    if (rc != SNMP_ERR_noError) {
		i = routeProtoOther;
		ipsa->routetable->set(&oi, &a);
	    } else {
		delete a;
		// Just leave it what it is
	    }
	}

	subid[len - 5] = ipRouteNextHop;
	p = SIN(rt->k_rt_gateway)->sin_addr.s_addr;
	a = &p;
	ipsa->routetable->set(&oi, &a);

	subid[len - 5] = ipRouteMask;
	if (rtmsg->rtm_addrs & RTA_NETMASK) {
	    p = SIN(rt->k_rt_netmask)->sin_addr.s_addr;
	} else {
	    addr = SIN(rt->k_rt_gateway)->sin_addr.s_addr;
	    if (addr == 0)
#if _MIPS_SZLONG == 64
	        p = (unsigned int) 0;
#else
	        p = (unsigned long) 0;
#endif
	    else if (IN_CLASSA(addr))
	        p = IN_CLASSA_NET;
	    else if (IN_CLASSB(addr))
	        p = IN_CLASSB_NET;
	    else if (IN_CLASSC(addr))
	        p = IN_CLASSC_NET;
	    else if (IN_CLASSD(addr))
	        p = IN_CLASSD_NET;
	    else
#if _MIPS_SZLONG == 64
	        p = (unsigned int) 0;
#else
	        p = (unsigned long) 0;
#endif
	}
	ipsa->routetable->set(&oi, &a);

	subid[len - 5] = ipRouteInfo;
	a = &ao;
	ipsa->routetable->set(&oi, &a);
    }

    // Remove invalid entries
    subid[len - 4] = subid[len - 3] = subid[len - 2] = subid[len - 1] = 0;
    subid[len - 5] = ipRouteValid;
    for ( ; ; ) {
	rc = ipsa->routetable->formNextOID(&oi);
	if (rc != SNMP_ERR_noError)
	    break;
	rc = ipsa->routetable->get(&oi, &a);
	if (rc == SNMP_ERR_noError
	    && ((asnInteger *) a)->getValue() == validCounter) {
	    delete a;
	    continue;
	}
	delete a;

	// Check if this is an entry under construction
	subid[len - 5] = ipRouteType;
	rc = ipsa->routetable->get(&oi, &a);
	subid[len - 5] = ipRouteValid;
	if (rc == SNMP_ERR_noError
	    && ((asnInteger *) a)->getValue() == routeTypeInvalid) {
	    delete a;
	    continue;
	}
	delete a;
	a = 0;

	// Entry is invalid, delete it
	for (subID s = ipRouteDest; s <= ipRouteValid; s++) {
	    subid[len - 5] = s;
	    ipsa->routetable->set(&oi, &a);
	}
    }
}

void
updateNetToMediaTable(void)
{
    // If packets are coming every fastRequestTime, hold off updating
    // until mustUpdateTime.

    const int mustUpdateTime = 30;
    const int fastRequestTime = 5;

    extern unsigned int packetCounter;

    static unsigned int validCounter = 0;
    static time_t lastRequestTime = 0;
    static time_t lastUpdateTime = 0;

    if (validCounter == packetCounter)
	return;
    validCounter = packetCounter;
    time_t t = time(0);
    if (t - lastUpdateTime < mustUpdateTime
	&& t - lastRequestTime <= fastRequestTime) {
	lastRequestTime = t;
	return;
    }
    lastUpdateTime = lastRequestTime = t;

    snmpIPaddress ip;
    asnOctetString o;
    asnInteger ai;
    asnObject *a;
    subID *subid;
    unsigned int len;
    u_int i;
    int rc, mib[6];
    char *buf, *next, *lim;
    size_t needed;
    struct sockaddr_inarp *sarp;
    struct sockaddr_dl *sdl;
    struct rt_msghdr *msg;

    oid oi = "1.3.6.1.2.1.4.22.1.1.1.0.0.0.0";
    oi.getValue(&subid, &len);

    // Read arp table
    mib[0] = CTL_NET;
    mib[1] = PF_ROUTE;
    mib[2] = 0;
    mib[3] = AF_INET;
    mib[4] = NET_RT_FLAGS;
    mib[5] = RTF_LLINFO;

    if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0)
	return;
    buf = (char *)alloca(needed * 2);
    if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0)
	return;

    // Loop through entries
    lim = buf + needed;
    for (next = buf; next < lim; next += msg->rtm_msglen) {
	msg = (struct rt_msghdr *)next;
	sarp = (struct sockaddr_inarp *)(msg + 1);
#ifdef _HAVE_SA_LEN
	sdl = (struct sockaddr_dl *)(sarp->sarp_len + (char *)sarp);
#else /* _HAVE_SA_LEN */
	sdl = (struct sockaddr_dl *)(_FAKE_SA_LEN_DST((struct sockaddr *)sarp)
	    + (char *)sarp);
#endif /* _HAVE_SA_LEN */

	if (sdl->sdl_alen) {
	    subid[len - 4] = (subID)((sarp->sarp_addr.s_addr & 0xFF000000) >> 24);
	    subid[len - 3] = (subID)((sarp->sarp_addr.s_addr & 0x00FF0000) >> 16);
	    subid[len - 2] = (subID)((sarp->sarp_addr.s_addr & 0x0000FF00) >> 8);
	    subid[len - 1] = (subID)(sarp->sarp_addr.s_addr & 0x000000FF);

	    subid[len - 6] = ipNetToMediaIfIndex;
	    ai = 1;
	    a = &ai;
            ipsa->nettomediatable->set(&oi, &a);

	    subid[len - 6] = ipNetToMediaType;
	    ai = (msg->rtm_rmx.rmx_expire == 0) ?
				mediaTypeStatic : mediaTypeDynamic;
	    ipsa->nettomediatable->set(&oi, &a);

	    subid[len - 6] = ipNetToMediaValid;
	    ai = validCounter;
	    ipsa->nettomediatable->set(&oi, &a);

            subid[len - 6] = ipNetToMediaPhysAddress;
            o.setValue((char *)LLADDR(sdl), sdl->sdl_alen);
            a = &o;
            ipsa->nettomediatable->set(&oi, &a);

            subid[len - 6] = ipNetToMediaNetAddress;
            ip = sarp->sarp_addr.s_addr;
            a = &ip;
            ipsa->nettomediatable->set(&oi, &a);
	}
    }

    // Remove invalid entries
    for (i = 5; i != 0; i--)
	subid[len - i] = 0;
    subid[len - 6] = ipNetToMediaValid;
    for ( ; ; ) {
	rc = ipsa->nettomediatable->formNextOID(&oi);
	if (rc != SNMP_ERR_noError)
	    break;
	rc = ipsa->nettomediatable->get(&oi, &a);
	if (rc == SNMP_ERR_noError
	    && ((asnInteger *) a)->getValue() == validCounter) {
	    delete a;
	    continue;
	}
	delete a;

	// Check if this is an entry under construction
	subid[len - 6] = ipNetToMediaType;
	rc = ipsa->nettomediatable->get(&oi, &a);
	subid[len - 6] = ipNetToMediaValid;
	if (rc == SNMP_ERR_noError
	    && ((asnInteger *) a)->getValue() == mediaTypeInvalid) {
	    delete a;
	    continue;
	}
	delete a;
	a = 0;

	// Entry is invalid, delete it
	for (subID s = ipNetToMediaIfIndex; s <= ipNetToMediaValid; s++) {
	    subid[len - 6] = s;
	    ipsa->nettomediatable->set(&oi, &a);
	}
    }
}
