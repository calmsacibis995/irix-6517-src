/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Address Translation sub-agent
 *
 *	$Revision: 1.5 $
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
#include <sys/time.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <sys/sysctl.h>
#include <syslog.h>
#include <unistd.h>
#include <strings.h>
#include <stdio.h>
#include <alloca.h>
#include <arpa/inet.h>

#include "oid.h"
#include "asn1.h"
#include "snmp.h"
#include "packet.h"
#include "subagent.h"
#include "sat.h"
#include "agent.h"
#include "table.h"
#include "atsa.h"

const subID atTable = 1;
    const subID atIfIndex = 1;
    const subID atPhysAddress = 2;
    const subID atNetAddress = 3;
    const subID atValid = 4;		// Internal

static atSubAgent *atsa;

atSubAgent::atSubAgent(void)
{
    // Store address for table update routines
    atsa = this;

    // Export subtree
    subtree = "1.3.6.1.2.1.3";
    int rc = export("AddressTranslation", &subtree);
    if (rc != SNMP_ERR_genErr) {
	log(LOG_ERR, 0, "error exporting AT subagent");
	return;
    }

    // Define tables
    attable = table(atTable, 6, updateAtTable);
}

atSubAgent::~atSubAgent(void)
{
    unexport(&subtree);
    delete attable;
}

int
atSubAgent::get(asnObjectIdentifier *o, asnObject **a, int *)
{
    // Pull out the subID array and length
    subID *subid;
    unsigned int len;
    oid *oi = o->getValue();
    oi->getValue(&subid, &len);

    // Check that the subID array is of valid size
    if (len == sublen)
	return SNMP_ERR_noSuchName;

    if (subid[sublen] == atTable) {
	if (len != sublen + 9
	    || subid[sublen + 1] != 1
	    || subid[sublen + 2] == 0
	    || subid[sublen + 2] > atNetAddress)
	    return SNMP_ERR_noSuchName;

	updateAtTable();
	return attable->get(oi, a);
    }

    return SNMP_ERR_noSuchName;
}

int
atSubAgent::getNext(asnObjectIdentifier *o, asnObject **a, int *t)
{
    return simpleGetNext(o, a, t, atTable);
}

int
atSubAgent::set(asnObjectIdentifier *, asnObject **, int *)
{
    // No sets allowed here.  Sets should use the ipNetToMedia table.
    return SNMP_ERR_noSuchName;
}

void
updateAtTable(void)
{
	int mib[6];
	size_t needed;
	char *buffer, *lim, *next;
	struct rt_msghdr *rtm;
	struct sockaddr_inarp *sarp;
	struct sockaddr_dl *sdl;
	asnInteger ai;
	asnOctetString o;
	asnObject *a;
	snmpNetworkAddress ne; 
	subID *subid, s;
	oid oi;
	u_int len, i;
	int rc;
	time_t t;

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
	t = time(0);
	if (t - lastUpdateTime < mustUpdateTime
	    && t - lastRequestTime <= fastRequestTime) {
		lastRequestTime = t;
		return;
	}
	lastUpdateTime = lastRequestTime = t;

	oi.setValue("1.3.6.1.2.1.3.1.1.1.1.1.0.0.0.0");
	oi.getValue(&subid, &len);

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET;
	mib[4] = NET_RT_FLAGS;
	mib[5] = RTF_LLINFO;
	if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0)
		return;
	buffer = (char *)alloca(needed);
	if (sysctl(mib, 6, buffer, &needed, NULL, 0) < 0)
		return;
	lim = buffer + needed;
	for (next = buffer; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;
		sarp = (struct sockaddr_inarp *)(rtm + 1);
#ifdef _HAVE_SA_LEN
		sdl = (struct sockaddr_dl *)(sarp->sarp_len + (char *)sarp);
#else /* _HAVE_SA_LEN */
		sdl = (struct sockaddr_dl *)(_FAKE_SA_LEN_DST((struct sockaddr
		    *)sarp) + (char *)sarp);
#endif /* _HAVE_SA_LEN */
		if (sdl->sdl_alen) {
			subid[len - 4] =
			    (subID)((sarp->sarp_addr.s_addr & 0xFF000000) >>
			    24);
			subid[len - 3] =
			    (subID)((sarp->sarp_addr.s_addr & 0x00FF0000) >>
			    16);
			subid[len - 2] =
			    (subID)((sarp->sarp_addr.s_addr & 0x0000FF00) >>
			    8);
			subid[len - 1] =
			    (subID)(sarp->sarp_addr.s_addr & 0x000000FF);

			subid[len - 7] = atIfIndex;
			ai = 1;
			a = &ai;
			atsa->attable->set(&oi, &a);

			subid[len - 7] = atPhysAddress;
			o.setValue((char *)LLADDR(sdl), sdl->sdl_alen);
			a = &o;
			atsa->attable->set(&oi, &a);

			subid[len - 7] = atNetAddress;
			ne.setValue(sarp->sarp_addr.s_addr);
			a = &ne;
			atsa->attable->set(&oi, &a);

			subid[len - 7] = atValid;
			ai = validCounter;
			a = &ai;
			atsa->attable->set(&oi, &a);
		}
	}

	// Remove invalid entries
	for (i = len - 6; i < len; i++) {
		subid[i] = 0;
	}
	subid[len - 7] = atValid;
	for ( ; ; ) {
		rc = atsa->attable->formNextOID(&oi);
		if (rc != SNMP_ERR_noError)
			break;
		rc = atsa->attable->get(&oi, &a);
		if (rc == SNMP_ERR_noError
		    && ((asnInteger *) a)->getValue() == validCounter) {
			delete (asnInteger *) a;
			continue;
		}
		delete (asnInteger *) a;
		a = 0;

		// Entries is invalid, delete it
		for (s = atIfIndex; s <= atValid; s++) {
			subid[len - 7] = s;
			atsa->attable->set(&oi, &a);
		}
	}
}
