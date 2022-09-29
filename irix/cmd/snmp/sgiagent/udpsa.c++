/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	UDP sub-agent
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
#include <sys/socket.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip_var.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/udp.h>
#include <sys/tcpipstats.h>
#include <sys/sysmp.h>
#include <errno.h>
extern "C" {
#include "exception.h"
};
#include <time.h>
#include <stdio.h>
#include <syslog.h>

#include "oid.h"
#include "asn1.h"
#include "snmp.h"
#include "packet.h"
#include "subagent.h"
#include "sat.h"
#include "agent.h"
#include "table.h"
#include "udpsa.h"
#include "knlist.h"

const subID udpInDatagrams = 1;
const subID udpNoPorts = 2;
const subID udpInErrors = 3;
const subID udpOutDatagrams = 4;
const subID udpTable = 5;
    const subID udpLocalAddress = 1;
    const subID udpLocalPort = 2;
    const subID udpValid = 3;		// Internal

static udpSubAgent *udpsa;

udpSubAgent::udpSubAgent(void)
{
    // For table update
    udpsa = this;

    // Export subtree
    subtree = "1.3.6.1.2.1.7";
    int rc = export("udp", &subtree);
    if (rc != SNMP_ERR_genErr) {
	log(LOG_ERR, 0, "error exporting UDP subagent");
	return;
    }

    // Define table
    listable = table(udpTable, 5, updateUdpLisTable);
}

udpSubAgent::~udpSubAgent(void)
{
    unexport(&subtree);
    delete listable;
}

int
udpSubAgent::get(asnObjectIdentifier *o, asnObject **a, int *)
{
    // Must be static to work correctly. Otherwise, sysmp gives
    // errno = 14 (Bad address).
    static struct kna kna_s;
    struct kna *kna_p = &kna_s;
    struct udpstat *udpstat_p;

    subID *subid;
    unsigned int len;

    // Pull out the subID array and length
    oid *oi = o->getValue();
    oi->getValue(&subid, &len);

    // Check that the subID array is of valid size
    if (len == sublen)
	return SNMP_ERR_noSuchName;

    // Handle the UDP listener table separately
    if (subid[sublen] == udpTable) {
	if (len != sublen + 8
	    || subid[sublen + 1] != 1
	    || subid[sublen + 2] == 0
	    || subid[sublen + 2] > udpLocalPort)
	    return SNMP_ERR_noSuchName;

	updateUdpLisTable();
	return listable->get(oi, a);
    }

    // Check that the subID array is of valid size
    if (len != sublen + 2 || subid[sublen + 1] != 0)
	return SNMP_ERR_noSuchName;

    // Get the pointer to the ipstat structure
    int err = sysmp(MP_SAGET, MPSA_TCPIPSTATS, kna_p, sizeof(struct kna));
    if (err < 0) {
        exc_errlog(LOG_ERR, 0, "udp get: sysmp error: errno=%d", errno);
        // No UDP objects will be returned
        return SNMP_ERR_noSuchName;
    }
    udpstat_p = &(kna_p->udpstat);

    // Switch on the subID and assign the value to *a
    switch (subid[sublen]) {
	case udpInDatagrams:
	    *a = new snmpCounter((unsigned int) (udpstat_p->udps_ipackets
						- (udpstat_p->udps_hdrops
						   + udpstat_p->udps_badsum
						   + udpstat_p->udps_badlen
						   + udpstat_p->udps_noport
						   + udpstat_p->udps_noportbcast
						   + udpstat_p->udps_fullsock)));
	    break; 

	case udpNoPorts:
	    *a = new snmpCounter((unsigned int) udpstat_p->udps_noport);
	    break; 

	case udpInErrors:
	    *a = new snmpCounter((unsigned int) (udpstat_p->udps_hdrops
						 + udpstat_p->udps_badsum
						 + udpstat_p->udps_badlen
						 + udpstat_p->udps_fullsock));
	    break; 

	case udpOutDatagrams:
	    *a = new snmpCounter((unsigned int) udpstat_p->udps_opackets);
	    break; 

	default:
	    return SNMP_ERR_noSuchName;
    }

    return SNMP_ERR_noError;
}

int
udpSubAgent::getNext(asnObjectIdentifier *o, asnObject **a, int *t)
{
    return simpleGetNext(o, a, t, udpTable);
}

int
udpSubAgent::set(asnObjectIdentifier *, asnObject **, int *)
{
    // There are no setable variables
    return SNMP_ERR_noSuchName;
}

void
updateUdpLisTable(void)
{
    // If packets are coming every fastRequestTime, hold off updating
    // until mustUpdateTime.

    const int mustUpdateTime = 5;
    const int fastRequestTime = 2;

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

    struct inpcb pcb, *inp, *prev, *next;
    struct in_pcbhead hcb;
    off_t udb, hoff, table;
    snmpIPaddress ip;
    asnInteger i;
    asnObject *a;
    subID *subid;
    unsigned int len, n;
    unsigned long saddr;
    unsigned short sport;
    int rc, h, tsize;
    oid oi = "1.3.6.1.2.1.7.5.1.1.0.0.0.0.0";
    oi.getValue(&subid, &len);

#ifdef sgi
    if (is_elf64) 
       udb = nl64[N_UDB].n_value; 
    else
#endif /* sgi */
    udb = nl[N_UDB].n_value; 
    klseek(udb);
    if (kread(&pcb, sizeof pcb) < 0)
	return;
    tsize = pcb.inp_tablesz;
    table = (off_t)pcb.inp_table;
    for (h = 0; h < tsize; h++) {
	hoff = table + (h * sizeof(hcb));
	prev = (struct inpcb *)hoff;
	klseek(hoff);
	if (kread(&hcb, sizeof(hcb)) < 0) {
	    continue;
	}
	inp = (struct inpcb *)&hcb;
	if (inp->inp_next == (struct inpcb *)hoff) {
	    continue;
	}
	while (inp->inp_next != (struct inpcb *)hoff) {
	    next = inp->inp_next;
	    klseek((off_t)next);
	    if (kread(&pcb, sizeof(pcb)) < 0) {
		break;
	    }
	    inp = &pcb;
	    if (pcb.inp_prev != prev) {
		break;
	    }

	    // Form oid
	    saddr = pcb.inp_laddr.s_addr;
	    sport = pcb.inp_lport;

	    subid[len - 5] = (subID) ((saddr & 0xFF000000) >> 24);
	    subid[len - 4] = (subID) ((saddr & 0x00FF0000) >> 16);
	    subid[len - 3] = (subID) ((saddr & 0x0000FF00) >> 8);
	    subid[len - 2] = (subID) (saddr & 0x000000FF);
	    subid[len - 1] = sport;

	    // Set variables
	    subid[len - 6] = udpLocalAddress;
	    ip = saddr;
	    a = &ip;
	    udpsa->listable->set(&oi, &a);

	    subid[len - 6] = udpLocalPort;
	    i = sport;
	    a = &i;
	    udpsa->listable->set(&oi, &a);

	    subid[len - 6] = udpValid;
	    i = validCounter;
	    udpsa->listable->set(&oi, &a);
	}
    }

    // Remove invalid entries
    for (n = 5; n != 0; n--)
	subid[len - n] = 0;
    subid[len - 6] = udpValid;
    for ( ; ; ) {
	rc = udpsa->listable->formNextOID(&oi);
	if (rc != SNMP_ERR_noError)
	    break;
	rc = udpsa->listable->get(&oi, &a);
	if (rc == SNMP_ERR_noError
	    && ((asnInteger *) a)->getValue() == validCounter) {
	    delete (asnInteger *) a;
	    continue;
	}
	delete (asnInteger *) a;
	a = 0;

	// Entries is invalid, delete it
	for (subID s = udpLocalAddress; s <= udpValid; s++) {
	    subid[len - 6] = s;
	    udpsa->listable->set(&oi, &a);
	}
    }
}
