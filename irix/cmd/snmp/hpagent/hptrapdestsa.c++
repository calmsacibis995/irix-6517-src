/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	HP Trap subagent
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
#include <mntent.h>	// for MNTMAXSTR definition

#include "asn1.h"
#include "packet.h"
#include "subagent.h"
#include "sat.h"
#include "table.h"
#include "agent.h"
#include "hptrapdestsa.h"
#include "sgisa.h"
#include "traphandler.h"

extern "C" {
#include "exception.h"
}

extern snmpTrapHandler *traph;
hpTrapDestSubAgent *hptrapdestsa;

const subID trapDestNumber = 1;
const subID trapDestTable  = 2;
   // const subID trapDest = 1;	

hpTrapDestSubAgent::hpTrapDestSubAgent(void)
{
    // Store address for table update routines
    hptrapdestsa = this;

    // Export subtree
    subtree = "1.3.6.1.4.1.11.2.13.1";
    int rc = export("hpTrapDest", &subtree);
    if (rc != SNMP_ERR_genErr) {
	exc_errlog(LOG_ERR, 0, 
            "hptrapdestsa: error exporting HP fileSystem subagent");
	return;
    }

    // Define table
    trapdesttable = table(trapDestTable, 5, updateTrapDestTable);
    trapdestnumber = 0;
}

hpTrapDestSubAgent::~hpTrapDestSubAgent(void)
{
    unexport(&subtree);
    delete trapdesttable;
}

int
hpTrapDestSubAgent::get(asnObjectIdentifier *o, asnObject **a, int *)
{
    // Pull out the subID array and length
    subID *subid;
    unsigned int len;
    oid *oi = o->getValue();
    oi->getValue(&subid, &len);

    // Check that the subID array is of valid size
    if (len == sublen)
	return SNMP_ERR_noSuchName;

    // Handle the trapdesttable table separately
    if (subid[sublen] == trapDestTable) {  // subtree.trapDestTable
	if (len != sublen + 8		   // subtree.table.entry.column.idx
	    || subid[sublen + 1] != 1	   // entry
	    || subid[sublen + 2] != 1)	   // Only one column in the table
	    return SNMP_ERR_noSuchName;

	updateTrapDestTable();
	return trapdesttable->get(oi, a);
    }

    // Check that the subID array is of valid size for the scalar objects
    // There is only one scalar object in this MIB group: trapDestNumber.

    if (len != sublen + 2 || subid[sublen + 1] != 0)
	return SNMP_ERR_noSuchName;

    // Switch on the subID and assign the value to *a
    switch (subid[sublen]) {
	case trapDestNumber:
	    updateTrapDestTable();
	    *a = new snmpGauge(trapdestnumber);
	    break;
	default:
	    return SNMP_ERR_noSuchName;
    }

    return SNMP_ERR_noError;
}

int
hpTrapDestSubAgent::getNext(asnObjectIdentifier *o, asnObject **a, int *t)
{
    return simpleGetNext(o, a, t, trapDestTable);
}

int
hpTrapDestSubAgent::set(asnObjectIdentifier *, asnObject **, int *)
{
        // No sets in this MIB group
        return SNMP_ERR_noSuchName;
}

void
updateTrapDestTable(void)
{
    // Only update once per agent to avoid malicious modifications.
    // No need to free the structure since it is filled only once per agent.
    static int scanCount = 0;

    if (scanCount != 0)
        return;

    scanCount = 1;

    snmpNetworkAddress 	ne;
    asnObject 		*a;
    subID 		*subid;
    unsigned int 	len;
    int 		rc;
    struct in_addr 	*addrp, *prev_addrp;;
    unsigned long	addr;

    // iso(1).org(3).dod(6).internet(1).private(4).enterprises(1).
    // hp(11).nm(2).snmp(13).trap(1).trapTable(2).trapEntry(1).
    // trapDestination(1).index_encoding_type(1).x.x.x.x
    // See RFC 1212 page 9 for more info.
    oid oi = "1.3.6.1.4.1.11.2.13.1.2.1.1.1.0.0.0.0";
    oi.getValue(&subid, &len);

    // Scan structure to fill up table a column at a time
    for (addrp = (traph->getFirstUniqueAddr()); 
	    addrp != NULL;
	    addrp = traph->getNextUniqueAddr(prev_addrp)) {

	prev_addrp = addrp;
	addr = addrp->s_addr;
	subid[len - 4] = (subID) ((addr & 0xFF000000) >> 24);
        subid[len - 3] = (subID) ((addr & 0x00FF0000) >> 16);
        subid[len - 2] = (subID) ((addr & 0x0000FF00) >> 8);
        subid[len - 1] = (subID) (addr & 0x000000FF);

        rc = hptrapdestsa->trapdesttable->get(&oi, &a);
	if (rc == SNMP_ERR_noError)	// Entry already exists
            continue;

	ne.setValue(addrp->s_addr);
        a = &ne;
        hptrapdestsa->trapdesttable->set(&oi, &a);
    }
    hptrapdestsa->trapdestnumber = traph->getUniqueTrapCount();
}
