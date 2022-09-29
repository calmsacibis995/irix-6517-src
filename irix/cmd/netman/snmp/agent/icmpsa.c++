/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	ICMP sub-agent
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

#include <sys/types.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <sys/tcpipstats.h>
#include <sys/sysmp.h>
extern "C" {
#include "exception.h"
};
#include <stdio.h>
#include <syslog.h>
#include <errno.h>

#include "oid.h"
#include "asn1.h"
#include "snmp.h"
#include "packet.h"
#include "subagent.h"
#include "sat.h"
#include "agent.h"
#include "icmpsa.h"
#include "knlist.h"


const subID icmpInMsgs = 1;
const subID icmpInErrors = 2;
const subID icmpInDestUnreachs = 3;
const subID icmpInTimeExcds = 4;
const subID icmpInParmProbs = 5;
const subID icmpInSrcQuenchs = 6;
const subID icmpInRedirects = 7;
const subID icmpInEchos = 8;
const subID icmpInEchoReps = 9;
const subID icmpInTimestamps = 10;
const subID icmpInTimestampReps  = 11;
const subID icmpInAddrMasks = 12;
const subID icmpInAddrMaskReps = 13;
const subID icmpOutMsgs = 14;
const subID icmpOutErrors = 15;
const subID icmpOutDestUnreachs = 16;
const subID icmpOutTimeExcds = 17;
const subID icmpOutParmProbs = 18;
const subID icmpOutSrcQuenchs = 19;
const subID icmpOutRedirects = 20;
const subID icmpOutEchos = 21;
const subID icmpOutEchoReps = 22;
const subID icmpOutTimestamps = 23;
const subID icmpOutTimestampReps = 24;
const subID icmpOutAddrMasks = 25;
const subID icmpOutAddrMaskReps = 26;


icmpSubAgent::icmpSubAgent(void)
{
    // Export subtree
    subtree = "1.3.6.1.2.1.5";
    int rc = export("icmp", &subtree);

    if (rc != SNMP_ERR_genErr) {
	log(LOG_ERR, 0, "error exporting ICMP subagent");
	return;
    }
}

icmpSubAgent::~icmpSubAgent(void)
{
    unexport(&subtree);
}

int
icmpSubAgent::get(asnObjectIdentifier *o, asnObject **a, int *)
{
    // Must be static to work correctly. Otherwise, sysmp gives
    // errno = 14 (Bad address).
    static struct kna kna_s;
    struct kna *kna_p = &kna_s;
    struct icmpstat *icmpstat_p;

    // Pull out the subID array and length
    subID *subid;
    unsigned int len;
    oid *oi = o->getValue();
    oi->getValue(&subid, &len);

    // Check that the subID array is of valid size
    if (len != sublen + 2 || subid[sublen + 1] != 0)
	return SNMP_ERR_noSuchName;

    // Get the pointer to the icmp structure
    int err = sysmp(MP_SAGET, MPSA_TCPIPSTATS, kna_p, sizeof(struct kna));
    if (err < 0) {
        exc_errlog(LOG_ERR, 0, "icmp get: sysmp error: errno=%d", errno);
	// No ICMP objects will be returned
	return SNMP_ERR_noSuchName;
    }
    icmpstat_p = &(kna_p->icmpstat);

    // Switch on the subID and assign the value to *a
    switch (subid[sublen]) {
	case icmpInMsgs: 
	    {
		unsigned int i, total;

		for (i = 0, total = 0; i < ICMP_MAXTYPE + 1; i++)
		    total += icmpstat_p->icps_inhist[i];

		*a = new snmpCounter(total);
	    }
	    break;

	case icmpInErrors: 
	      *a = new snmpCounter((unsigned int) (icmpstat_p->icps_badcode
						   + icmpstat_p->icps_checksum
						   + icmpstat_p->icps_tooshort
						   + icmpstat_p->icps_badlen));
	      break;

	case icmpInDestUnreachs: 
	      *a = new snmpCounter((unsigned int)
				icmpstat_p->icps_inhist[ICMP_UNREACH]);
	      break;

	case icmpInTimeExcds:
	      *a = new snmpCounter((unsigned int)
				icmpstat_p->icps_inhist[ICMP_TIMXCEED]);
	      break;

	case icmpInParmProbs: 
	      *a = new snmpCounter((unsigned int)
				icmpstat_p->icps_inhist[ICMP_PARAMPROB]);
	      break;

	case icmpInSrcQuenchs:
	      *a = new snmpCounter((unsigned int)
				icmpstat_p->icps_inhist[ICMP_SOURCEQUENCH]);
	      break;

	case icmpInRedirects:
	      *a = new snmpCounter((unsigned int)
				icmpstat_p->icps_inhist[ICMP_REDIRECT]);
	      break;

	case icmpInEchos:
	      *a = new snmpCounter((unsigned int)
				icmpstat_p->icps_inhist[ICMP_ECHO]);
	      break;

	case icmpInEchoReps:
	      *a = new snmpCounter((unsigned int)
				icmpstat_p->icps_inhist[ICMP_ECHOREPLY]);
	      break;

	case icmpInTimestamps:
	      *a = new snmpCounter((unsigned int)
				icmpstat_p->icps_inhist[ICMP_TSTAMP]);
	      break;

	case icmpInTimestampReps:
	      *a = new snmpCounter((unsigned int)
				icmpstat_p->icps_inhist[ICMP_TSTAMPREPLY]);
	      break;

	case icmpInAddrMasks:
	      *a = new snmpCounter((unsigned int)
				icmpstat_p->icps_inhist[ICMP_MASKREQ]);
	      break;

	case icmpInAddrMaskReps:
	      *a = new snmpCounter((unsigned int)
				icmpstat_p->icps_inhist[ICMP_MASKREPLY]);
	      break;

	case icmpOutMsgs:
	    {
		unsigned int i, total;

		for (i = 0, total = 0; i < ICMP_MAXTYPE + 1; i++)
		    total += icmpstat_p->icps_outhist[i];

		*a = new snmpCounter(total);
	    }
	    break;

	case icmpOutErrors:
	      *a = new snmpCounter((unsigned int) icmpstat_p->icps_error);
	      break;

	case icmpOutDestUnreachs:
	      *a = new snmpCounter((unsigned int)
				icmpstat_p->icps_outhist[ICMP_UNREACH]);
	      break;

	case icmpOutTimeExcds:
	      *a = new snmpCounter((unsigned int)
				icmpstat_p->icps_outhist[ICMP_TIMXCEED]);
	      break;

	case icmpOutParmProbs:
	      *a = new snmpCounter((unsigned int)
				icmpstat_p->icps_outhist[ICMP_PARAMPROB]);
	      break;

	case icmpOutSrcQuenchs: 
	      *a = new snmpCounter((unsigned int)
				icmpstat_p->icps_outhist[ICMP_SOURCEQUENCH]);
	      break; 

	case icmpOutRedirects:
	      *a = new snmpCounter((unsigned int)
				icmpstat_p->icps_outhist[ICMP_REDIRECT]);
	      break;

	case icmpOutEchos: 
	      *a = new snmpCounter((unsigned int)
				icmpstat_p->icps_outhist[ICMP_ECHO]);
	      break;

	case icmpOutEchoReps: 
	      *a = new snmpCounter((unsigned int)
				icmpstat_p->icps_outhist[ICMP_ECHOREPLY]);
	      break;

	case icmpOutTimestamps: 
	      *a = new snmpCounter((unsigned int)
				icmpstat_p->icps_outhist[ICMP_TSTAMP]);
	      break;

	case icmpOutTimestampReps:
	      *a = new snmpCounter((unsigned int)
				icmpstat_p->icps_outhist[ICMP_TSTAMPREPLY]);
	      break;

	case icmpOutAddrMasks: 
	      *a = new snmpCounter((unsigned int)
				icmpstat_p->icps_outhist[ICMP_MASKREQ]);
	      break;

	case icmpOutAddrMaskReps:
	      *a = new snmpCounter((unsigned int)
				icmpstat_p->icps_outhist[ICMP_MASKREPLY]);
	      break;

	default:
	    return SNMP_ERR_noSuchName;
    }
    return SNMP_ERR_noError;
}


int
icmpSubAgent::getNext(asnObjectIdentifier *o, asnObject **a, int *t)
{
    return simpleGetNext(o, a, t, icmpOutAddrMaskReps);
}


int
icmpSubAgent::set(asnObjectIdentifier *, asnObject **, int *)
{
    // There are no setable variables
    return SNMP_ERR_noSuchName;
}
