/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	System sub-agent
 *
 *	$Revision: 1.1 $
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

#include <sys/time.h>
#include <netinet/in.h>
#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include <sys/times.h>
#include <unistd.h>

#include <errno.h>
// #include <bstring.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <sys/select.h>

#include "asn1.h"
#include "packet.h"
#include "subagent.h"
#include "sat.h"
#include "agent.h"
#include "hpicmpsa.h"
#include "sgisa.h"
#include "traphandler.h"

extern "C" {
#include "exception.h"
}

#define MAXPACKET       (65536-60-8)    /* max packet size */

const subID hpIcmpEchoReq 	= 1;

int get_echo_req(long addr, int datalen, int ping_timeout);
static int in_cksum(u_short *addr, int len);
static long msec(struct timeval *now, struct timeval *then);


extern sgiHWSubAgent *sgihwsa;
extern snmpTrapHandler *traph;

hpIcmpSubAgent *hpicmpsa;

hpIcmpSubAgent::hpIcmpSubAgent(void)
{
    // Store address for use by other subagents
    hpicmpsa = this;

    // Export subtree
    subtree = "1.3.6.1.4.1.11.2.7";
    int rc = export("hpIcmpEcho", &subtree);
    if (rc != SNMP_ERR_genErr) {
	exc_errlog(LOG_ERR, 0, 
	    "hpicmpsa: error exporting HP icmp echo request subagent");
	return;
    }
}

hpIcmpSubAgent::~hpIcmpSubAgent(void)
{
    unexport(&subtree);
}

int
hpIcmpSubAgent::get(asnObjectIdentifier *o, asnObject **a, int *)
{
    // Pull out the subID array and length
    subID *subid;
    unsigned int len;
    oid *oi = o->getValue();
    oi->getValue(&subid, &len);

    // Check that the subID array is of valid size
    if (len != sublen + 8 || subid[sublen + 7] != 0)
	return SNMP_ERR_noSuchName;

    // Switch on the subID and assign the value to *a
    switch (subid[sublen]) {
	case hpIcmpEchoReq: {
    
	    int echoreq = 0;
	    long addr;
	    int size, timeout;

    	    addr =         subid[len - 2];
    	    addr = addr | (subid[len - 3] << 8);
    	    addr = addr | (subid[len - 4] << 16);
    	    addr = addr | (subid[len - 5] << 24);

    	    size = subid[len - 7];
    	    if ( size <= 0 || size > 128) {
        	exc_errlog(LOG_ERR, 0, "hpicmpsa: invalid packet size (%d)\n",
                    size);
        	echoreq = -4;
    	    }
	    else {
    		timeout = subid[len - 6];
		if ( timeout <= 0 ) {
        	    exc_errlog(LOG_ERR, 0, "hpicmpsa: invalid timeout (%d)\n",
                        timeout);
        	    echoreq = -4;
		}
		else
	    	    echoreq = get_echo_req(addr, size, timeout);
    	    }

	    *a = new asnInteger((int) echoreq);
	    break;
	    }

	default:
	    return SNMP_ERR_noSuchName;
    }
    return SNMP_ERR_noError;
}

int
hpIcmpSubAgent::getNext(asnObjectIdentifier *o, asnObject **a, int *t)
{
    return simpleGetNext(o, a, t, hpIcmpEchoReq);
}

int
hpIcmpSubAgent::set(asnObjectIdentifier *, asnObject **, int *)
{
        // No sets in this MIB group
	return SNMP_ERR_noSuchName;
}


// get_echo_req just does an ICMP echo request, and returns:
// 	-1 if there is an internal error
//	-2 if the echo request timed out
//	-3 if the echo reply is not the correct reply
//	-4 if the packet size is too large (or too small)
//	-5 if the timeout is invalid
//	time the echo request took
        
int
get_echo_req(long addr, int datalen, int ping_timeout)
{
    union {
    	u_char 		outbuf[MAXPACKET];
    	struct icmp 	outicmp;
    } outpack;
    char inbuf[128];

    struct icmp *icp = &outpack.outicmp;
    struct protoent *proto;
    struct sockaddr_in to_s, from_s;
    int fromlen = sizeof(from_s);
    struct timeval timeout;
    struct ip *in_ip;
    struct icmp *in_icmp;

    // First we create a raw socket to use for the ICMP ECHO
    to_s.sin_family = AF_INET;
    
    bcopy((struct in_addr *) &addr, (caddr_t)&(to_s.sin_addr), sizeof(to_s.sin_addr));

    if ((proto = getprotobyname("icmp")) == NULL) {
        exc_errlog(LOG_ERR, 0, 
    	"hpicmpsa: getprotobyname: unknown \"icmp\" protocol\n");
        return -1;
    }

    int s = socket(AF_INET, SOCK_RAW, proto->p_proto);
    if (s < 0) {
        exc_errlog(LOG_ERR, 0, "hpicmpsa: %s\n",
    	strerror(errno));
        return -1;
    }

    // Now we fill in a structure to use as our echo request.
    icp->icmp_type = ICMP_ECHO;
    icp->icmp_code = 0;
    icp->icmp_cksum = 0;
    icp->icmp_seq = 0;
    int ident = icp->icmp_id = getpid() & 0xffff;

    // Fill in timeval in the icmp packet
    struct timeval last_tx;
    gettimeofday(&last_tx,0);
    bcopy(&last_tx, &outpack.outbuf[8], sizeof(last_tx));

    icp->icmp_cksum = in_cksum((u_short *)icp, datalen + 8);

    int i = sendto(s, &outpack.outbuf, datalen, 0, 
    	       &to_s, sizeof(struct sockaddr));
    if ((i < 0) || (i != datalen)) {
        close(s);
        exc_errlog(LOG_ERR, 0, "hpicmpsa: sendto: internal error\n");
        return -1;
    }

    timeout.tv_sec = ping_timeout;
    timeout.tv_usec = 0;
    fd_set fdmask;
    FD_ZERO(&fdmask);
    FD_SET(s, &fdmask);
    for (;;) {
	i = select(s+1,&fdmask,0,0,&timeout);
	if (i == 0) {
            exc_errlog(LOG_ERR, 0, "hpicmpsa: select: timeout\n");
            return -2;
	} else if (i < 0) {
	    close(s);
            exc_errlog(LOG_ERR, 0, "hpicmpsa: select: internal error\n");
            return -1;
	} else {
	    i = recvfrom(s, inbuf, sizeof(inbuf), 0, &from_s, &fromlen);
	    if ( i < 0 ) {
		close (s);
                exc_errlog(LOG_ERR, 0, 
		    "hpicmpsa: recvfrom: internal error\n");
                return -1;
	    }
	    in_ip = (struct ip *)inbuf;
	    in_icmp = (struct icmp *)(inbuf + (in_ip->ip_hl << 2));
	    from_s.sin_addr.s_addr = ntohl(from_s.sin_addr.s_addr);
	    if ((in_icmp->icmp_type == ICMP_ECHOREPLY) &&
	            (in_icmp->icmp_id == ident) &&
	            (from_s.sin_addr.s_addr == to_s.sin_addr.s_addr)) {
		close (s);
		gettimeofday(&last_tx,0);
		struct timeval tv;
		bcopy(&icp->icmp_data[0], &tv, sizeof(tv));
		int triptime = msec(&last_tx, &tv);
	        return triptime;
	    }
	    else {
		close (s);
                exc_errlog(LOG_ERR, 0, 
		    "hpicmpsa: recvfrom: incorrect reply\n");
                return -3;
	    }
       }
    }
}

/*
** Simple routine to calculate the checksum.
*/
static int
in_cksum(u_short *addr, int len)
{
        register u_short *ptr;
        register int sum;
        u_short *lastptr;

        sum = 0;
        ptr = (u_short *)addr;
        lastptr = ptr + (len/2);
        for (; ptr < lastptr; ptr++) {
                sum += *ptr;
                if (sum & 0x10000) {
                        sum &= 0xffff;
                        sum++;
                }
        }
        return (~sum & 0xffff);
}


// compute the difference of two timevals in msec.

static long
msec(struct timeval *now, struct timeval *then)
{
        double val;

        val = (now->tv_sec - then->tv_sec);
        if (val < 1000000.0 && val > -1000000.0)
                val *= 1000.0;

        val += (now->tv_usec - then->tv_usec)/1000.0;
        return val;
}


