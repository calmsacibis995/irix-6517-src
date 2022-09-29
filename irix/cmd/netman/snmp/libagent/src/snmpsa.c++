/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	SNMP sub-agent
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
#include <netinet/in.h>
#include <bstring.h>
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
#include "snmpsa.h"
#include "traphandler.h"

extern "C" {
#include "exception.h"
};

extern snmpAgent agent;

const subID lastSnmpSubid = 30; 

snmpSubAgent::snmpSubAgent(void)
{
    // Export subtree
    subtree = "1.3.6.1.2.1.11";
    int rc = export("snmp", &subtree);
    if (rc != SNMP_ERR_genErr) {
	log(LOG_ERR, 0, "error exporting SNMP subagent");
	return;
    }
}

snmpSubAgent::~snmpSubAgent(void)
{
    unexport(&subtree);
}

snmpSubAgent::get(asnObjectIdentifier *o, asnObject **a, int *)
{
    // Pull out the subID array and length
    subID *subid;
    unsigned int len;
    oid *oi = o->getValue();
    oi->getValue(&subid, &len);

    // Check that the subID array is of valid size
    if (len != sublen + 2 || subid[sublen + 1] != 0)
	return SNMP_ERR_noSuchName;

    snmpCounter *c = new snmpCounter;
    switch (subid[sublen]) {
	case 1:
	    *c = agent.stats.snmpInPkts;
	    break;
	case 2:
	    *c = agent.stats.snmpOutPkts;
	    break;
	case 3:
	    *c = agent.stats.snmpInBadVersions;
	    break;
	case 4:
	    *c = agent.stats.snmpInBadCommunityNames;
	    break;
	case 5:
	    *c = agent.stats.snmpInBadCommunityUses;
	    break;
	case 6:
	    *c = agent.stats.snmpInASNParseErrs;
	    break;
	case 8:
	    *c = agent.stats.snmpInTooBigs;
	    break;
	case 9:
	    *c = agent.stats.snmpInNoSuchNames;
	    break;
	case 10:
	    *c = agent.stats.snmpInBadValues;
	    break;
	case 11:
	    *c = agent.stats.snmpInReadOnlys;
	    break;
	case 12:
	    *c = agent.stats.snmpInGenErrs;
	    break;
	case 13:
	    *c = agent.stats.snmpInTotalReqVars;
	    break;
	case 14:
	    *c = agent.stats.snmpInTotalSetVars;
	    break;
	case 15:
	    *c = agent.stats.snmpInGetRequests;
	    break;
	case 16:
	    *c = agent.stats.snmpInGetNexts;
	    break;
	case 17:
	    *c = agent.stats.snmpInSetRequests;
	    break;
	case 18:
	    *c = agent.stats.snmpInGetResponses;
	    break;
	case 19:
	    *c = agent.stats.snmpInTraps;
	    break;
	case 20:
	    *c = agent.stats.snmpOutTooBigs;
	    break;
	case 21:
	    *c = agent.stats.snmpOutNoSuchNames;
	    break;
	case 22:
	    *c = agent.stats.snmpOutBadValues;
	    break;
	case 24:
	    *c = agent.stats.snmpOutGenErrs;
	    break;
	case 25:
	    *c = agent.stats.snmpOutGetRequests;
	    break;
	case 26:
	    *c = agent.stats.snmpOutGetNexts;
	    break;
	case 27:
	    *c = agent.stats.snmpOutSetRequests;
	    break;
	case 28:
	    *c = agent.stats.snmpOutGetResponses;
	    break;
	case 29:
	    *c = agent.stats.snmpOutTraps;
	    break;
	case 30:
	    {
		delete c;
		asnInteger *i = new asnInteger;
		*i = agent.stats.snmpEnableAuthenTraps;
		*a = i;
		return SNMP_ERR_noError;
	    }
	default:
	    delete c;
	    return SNMP_ERR_noSuchName;
    }

    *a = c;
    return SNMP_ERR_noError;
}

int
snmpSubAgent::getNext(asnObjectIdentifier *o, asnObject **a, int *t)
{
    return simpleGetNext(o, a, t, lastSnmpSubid);
}

int
snmpSubAgent::set(asnObjectIdentifier *o, asnObject **a, int *)
{
    // Pull out the subID array and length
    subID *subid;
    unsigned int len;
    oid *oi = o->getValue();
    oi->getValue(&subid, &len);

    // Check that the subID array is of valid size and since the only setable
    // value is snmpEnableAuthenTraps, make sure that's what it is.
    if (len != sublen + 2 || subid[sublen + 1] != 0 || subid[sublen] != 30)
	return SNMP_ERR_noSuchName;

    // Check that the object has the right type
    if ((*a)->getTag() != TAG_INTEGER)
	return SNMP_ERR_badValue;

    // Copy the new value
    int v = ((asnInteger *)(*a))->getValue();
    if (v <= 0 || v > 2)
	return SNMP_ERR_badValue;
    agent.stats.snmpEnableAuthenTraps = v;

    // Save in permanent storage in the file /etc/snmpd.trap.conf
    FILE *fpin  = fopen (SNMP_TRAP_CONF_FILE, "r");
    if ( fpin == 0 ) {
	exc_errlog(LOG_WARNING, errno,
	    "trapHandler: Cannot open trap config file %s: ",
	    SNMP_TRAP_CONF_FILE);
	return SNMP_ERR_genErr;
    }

    FILE *fpout = fopen (TMP_SNMP_TRAP_CONF_FILE, "w");
    if ( fpout == 0 ) {
	exc_errlog(LOG_WARNING, errno,
	    "trapHandler: Cannot open temporary trap config file %s: ",
	    TMP_SNMP_TRAP_CONF_FILE);
	fclose (fpin);
	return SNMP_ERR_genErr;
    }


    if (fpout != 0 && fpout != 0) {
        char buf[TRAP_FILE_MAX_LINE];
	while ((fgets(buf, TRAP_FILE_MAX_LINE, fpin)) != NULL) {
	    char *line = strdup(buf);
	    char *tok = strtok(line, " \t\n"); 
	    if (tok != NULL) {
	        char *trapstr = strdup(tok);
	        if (strcasecmp(trapstr, "enableAuthenTraps") != 0) {
		    fprintf(fpout, "%s", buf);
	        }
	        else {
		    fprintf(fpout, "enableAuthenTraps\t%s\n",
		        agent.stats.snmpEnableAuthenTraps == ENABLE_AUTHEN_TRAPS
			     ? "yes" : "no");
	        delete line;
		continue;
	        }
	    }
	    else {	// Process empty strings
		fprintf(fpout, "%s", buf);
	    }
	}
        fclose(fpin);
        fclose(fpout);
	if (0 != rename(TMP_SNMP_TRAP_CONF_FILE, SNMP_TRAP_CONF_FILE)) {
	    exc_errlog(LOG_WARNING, 0,
                "trapHandler: Cannot save enableAuthenTraps to file %s: ",
                SNMP_TRAP_CONF_FILE);
            return SNMP_ERR_genErr;
	}
    }

    return SNMP_ERR_noError;
}

