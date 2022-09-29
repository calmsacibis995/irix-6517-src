/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Address Translation sub-agent
 *
 *	$Revision: 1.2 $
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
#include <net/if_types.h>
#include <net/soioctl.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <syslog.h>
#include <unistd.h>
#include <strings.h>
#include <stdio.h>

// prototype of ioctl no longer needed in sherwood
extern "C" {			// Where's the real one?
// int ioctl(int, int, void *);
}

#include "oid.h"
#include "asn1.h"
#include "snmp.h"
#include "packet.h"
#include "subagent.h"
#include "sat.h"
#include "agent.h"
#include "table.h"
#include "atsa.h"
#include "knlist.h"

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

    asnObject *a;
    snmpIPaddress p;
    snmpNetworkAddress ne; 
    asnInteger ai;
    asnOctetString o;
    subID *subid;
    unsigned int len, size, i, arptab_size;
    unsigned long addr;
    int rc;
    struct arptab *arptab;

    oid oi = "1.3.6.1.2.1.3.1.1.1.1.1.0.0.0.0";
    oi.getValue(&subid, &len);

    // Find ARP table size
#ifdef sgi
    if (is_elf64)
       klseek(nl64[N_ARPTAB_SIZE].n_value);
    else
#endif /* sgi */
    klseek(nl[N_ARPTAB_SIZE].n_value);
    if (kread(&arptab_size, sizeof arptab_size) < 0)
	return;
    if (arptab_size <= 0 || arptab_size > 1000)
	return;

    // Allocate space and read table from kernel
    arptab = new struct arptab[arptab_size];
    size = arptab_size * sizeof(struct arptab);
#ifdef sgi
    if (is_elf64)
       klseek(nl64[N_ARPTAB].n_value);
    else
#endif /* sgi */
    klseek(nl[N_ARPTAB].n_value);
    if (kread(arptab, size) != size) {
	delete arptab;
	return;
    }

    // Loop through entries
    for (i = 0; i < arptab_size; i++) {
	addr = arptab[i].at_iaddr.s_addr;
	if (addr == 0 || arptab[i].at_flags == 0)
		continue;
	subid[len - 4] = (subID) ((addr & 0xFF000000) >> 24);
	subid[len - 3] = (subID) ((addr & 0x00FF0000) >> 16);
	subid[len - 2] = (subID) ((addr & 0x0000FF00) >> 8);
	subid[len - 1] = (subID) (addr & 0x000000FF);

	subid[len - 7] = atIfIndex;
	ai = 1;
	a = &ai;
	atsa->attable->set(&oi, &a);

	subid[len - 7] = atValid;
	ai = validCounter;
	atsa->attable->set(&oi, &a);

	subid[len - 7] = atPhysAddress;
	o.setValue((char *) arptab[i].at_enaddr, sizeof arptab[i].at_enaddr);
	a = &o;
	atsa->attable->set(&oi, &a);

	subid[len - 7] = atNetAddress;
	ne.setValue(addr);
	a = &ne;
	atsa->attable->set(&oi, &a);
    } 

    // Remove invalid entries
    for (i = 6; i != 0; i--)
	subid[len - i] = 0;
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
	for (subID s = atIfIndex; s <= atValid; s++) {
	    subid[len - 7] = s;
	    atsa->attable->set(&oi, &a);
	}
    }

    delete arptab;
}
