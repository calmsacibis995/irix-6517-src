/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	HP Auth Fail Table subagent
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
#include <stdlib.h>

#include "asn1.h"
#include "packet.h"
#include "subagent.h"
#include "sat.h"
#include "table.h"
#include "agent.h"
#include "hpauthfailsa.h"
#include "sgisa.h"
#include "traphandler.h"

extern "C" {
#include "exception.h"
}

static void     updateAuthFailTable();
extern snmpTrapHandler *traph;
hpAuthFailSubAgent *hpauthfailsa;

const subID authFailTable  = 1;
   const subID authIpAddress 	= 1;	
   const subID authTime 	= 2;	
   const subID authCommunity 	= 3;	

hpAuthFailSubAgent::hpAuthFailSubAgent(void)
{
    // Store address for table update routines
    hpauthfailsa = this;

    // Export subtree
    subtree = "1.3.6.1.4.1.11.2.13.4";
    int rc = export("hpAuthFail", &subtree);
    if (rc != SNMP_ERR_genErr) {
	exc_errlog(LOG_ERR, 0, 
            "hpauthfailsa: error exporting HP authFail subagent");
	return;
    }

    // Define table
    authfailtable = table(authFailTable, 5, updateAuthFailTable);
}

hpAuthFailSubAgent::~hpAuthFailSubAgent(void)
{
    unexport(&subtree);
    delete authfailtable;
}

int
hpAuthFailSubAgent::get(asnObjectIdentifier *o, asnObject **a, int *)
{
    // Pull out the subID array and length
    subID *subid;
    unsigned int len;
    oid *oi = o->getValue();
    oi->getValue(&subid, &len);

    // Check that the subID array is of valid size
    if (len == sublen)
	return SNMP_ERR_noSuchName;

    // Handle the authfailtable table separately
    if (subid[sublen] == authFailTable) {  // subtree.authFailTable
	if (len != sublen + 8		   // subtree.table.entry.column.1.idx
	    || subid[sublen + 1] != 1	   // entry
	    || subid[sublen + 2] < authIpAddress
	    || subid[sublen + 2] > authCommunity)
	    return SNMP_ERR_noSuchName;

	updateAuthFailTable();
	return authfailtable->get(oi, a);
    }

    return SNMP_ERR_noSuchName;
}

int
hpAuthFailSubAgent::getNext(asnObjectIdentifier *o, asnObject **a, int *t)
{
    return simpleGetNext(o, a, t, authFailTable);
}

int
hpAuthFailSubAgent::set(asnObjectIdentifier *, asnObject **, int *)
{
        // No sets in this MIB group
        return SNMP_ERR_noSuchName;
}

void
updateAuthFailTable(void)
{
    long now;
    static long         last_update_time = 0;

    // Only update only once per packet
    extern unsigned int packetCounter;
    static unsigned int validCounter = 0;

    if (validCounter == packetCounter)
        return;
    validCounter = packetCounter;

    now = time((time_t *)0);
    if (((now - last_update_time) < 15) ) // Refresh every 15 secs max.
        return;

    last_update_time = now;

    asnObject 		*a;
    asnInteger		i;
    asnOctetString	o;
    snmpNetworkAddress 	ne;
    snmpTimeticks	t;
    char		*p;
    subID 		*subid;
    unsigned int 	len;

    // iso(1).org(3).dod(6).internet(1).private(4).enterprises(1).
    // hp(11).nm(2).snmp(13).authfail(4).authFailTable(1).authFailEntry(1).
    // column_id(x).1.x.x.x.x
    oid oi = "1.3.6.1.4.1.11.2.13.4.1.1.1.1.0.0.0.0";
    oi.getValue(&subid, &len);

    // Clear the previous entries first
    a = 0;
    for (subID s = authIpAddress; s <= authCommunity; s++) {
        subid[ len - 1] = 0;	// Clear the OIDs to get the first oid
        subid[ len - 2] = 0;	// in the table.
        subid[ len - 3] = 0;
        subid[ len - 4] = 0;
        subid[len - 6] = s;	// Set the column oid
        for ( ; ; ) {
            int rc = hpauthfailsa->authfailtable->formNextOID(&oi);
            if (rc != SNMP_ERR_noError)
                break;
            hpauthfailsa->authfailtable->set(&oi, &a);
        }
    }

    // Scan file
    FILE *fp = fopen (AUTH_FAIL_DATAFILE, "r");
    if (fp != 0) {
        char line[512];
        char tmp1[128], ip_addr_s[16], comm[128], tmp2[10];
	in_addr ip_addr;
        char tstamp_s[20];

        while (fgets(line, 511, fp) != 0) {
	    int num_items = sscanf(line, "%s %s %s %s %s", 
		   	 tmp1, ip_addr_s, comm, tmp2, tstamp_s);
	    if (num_items == EOF)
		break;
	    if (num_items != 5)
		continue;	// Skip this entry, after all.
	
	    inet_aton(ip_addr_s, &ip_addr);
	    // Set the index
            subid[len - 4] = (subID) ((ip_addr.s_addr & 0xFF000000) >> 24);
            subid[len - 3] = (subID) ((ip_addr.s_addr & 0x00FF0000) >> 16);
            subid[len - 2] = (subID) ((ip_addr.s_addr & 0x0000FF00) >> 8);
            subid[len - 1] = (subID) (ip_addr.s_addr & 0x000000FF);

            int rc = hpauthfailsa->authfailtable->get(&oi, &a);
            if (rc == SNMP_ERR_noError)     // Entry already exists
                continue;

	    subid[len - 6] = authIpAddress;
	    ne.setValue(ip_addr.s_addr);
            a = &ne;
            hpauthfailsa->authfailtable->set(&oi, &a);

            subid[len - 6] = authTime;
	    t = (unsigned int) (now - atoi(tstamp_s)) * 100;
	    a = &t;
            hpauthfailsa->authfailtable->set(&oi, &a);

            subid[len - 6] = authCommunity;
	    p = comm;
	    o = p;
	    a = &o;
            hpauthfailsa->authfailtable->set(&oi, &a);
	}
    }
    fclose(fp);
}
