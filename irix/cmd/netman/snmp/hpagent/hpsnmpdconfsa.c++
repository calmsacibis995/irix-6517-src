/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	HP snmpd conf sub-agent
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

// #include <sys/time.h>
#include <netinet/in.h>		// for struct sockaddr_in addr in agent.h
#include <stdio.h>
#include <syslog.h>
// #include <string.h>
// #include <sys/times.h>
// #include <unistd.h>
#include <paths.h>
#include <sys/dir.h>
#include <sys/procfs.h>
#include <errno.h>
#include <fcntl.h>

#include "asn1.h"
#include "packet.h"
#include "subagent.h"
#include "sat.h"
#include "agent.h"
#include "hpsnmpdconfsa.h"
#include "sgisa.h"
#include "traphandler.h"

extern "C" {
#include "exception.h"
#include "pmapi.h"
}

const subID hpSnmpdConfRespond 	= 1;
const subID hpSnmpdConfReConf 	= 2;
const subID hpSnmpdConfFlag 	= 3;
const subID hpSnmpdConfLogMask 	= 4;
const subID hpSnmpdConfVersion 	= 5;
const subID hpSnmpdConfStatus 	= 6;
const subID hpSnmpdConfSize 	= 7;
const subID hpSnmpdConfWhatStr 	= 9;	// There is no entry #8 in the MIB

#define HP_VERSION              101     // Represents the agent version number. Ex: 1.0.0

char hpsnmpd_what_string[]=	"Copyright 1994 Silicon Graphics, Inc.  All rights reserved.";
// char hpsnmpd_what_string[]=	"$Revision: 1.1 $ Copyright 1994 Silicon Graphics, Inc.  All rights reserved.";

extern sgiHWSubAgent *sgihwsa;
extern snmpTrapHandler *traph;

hpSnmpdConfSubAgent *hpsnmpdconfsa;

hpSnmpdConfSubAgent::hpSnmpdConfSubAgent(void)
{
    // Store address for use by other subagents
    hpsnmpdconfsa = this;

    // Export subtree
    subtree = "1.3.6.1.4.1.11.2.13.2";
    int rc = export("hpSnmpdConf", &subtree);
    if (rc != SNMP_ERR_genErr) {
	exc_errlog(LOG_ERR, 0, 
	    "hpsnmpdconfsa: error exporting HP snmpdConf subagent");
	return;
    }
}

hpSnmpdConfSubAgent::~hpSnmpdConfSubAgent(void)
{
    unexport(&subtree);
}

int
hpSnmpdConfSubAgent::get(asnObjectIdentifier *o, asnObject **a, int *)
{
    // Pull out the subID array and length
    subID *subid;
    unsigned int len;
    oid *oi = o->getValue();
    oi->getValue(&subid, &len);

    // Check that the subID array is of valid size
    if (len != sublen + 2 || subid[sublen + 1] != 0)
	return SNMP_ERR_noSuchName;

    // Switch on the subID and assign the value to *a
    switch (subid[sublen]) {
	case hpSnmpdConfRespond:
	    return SNMP_ERR_noSuchName;

	case hpSnmpdConfReConf:
	    return SNMP_ERR_noSuchName;

	case hpSnmpdConfFlag:
	    return SNMP_ERR_noSuchName;

	case hpSnmpdConfLogMask:
	    *a = new asnInteger(exc_level);
	    break;

	case hpSnmpdConfVersion:
	    *a = new asnInteger(HP_VERSION);
	    break;

	case hpSnmpdConfStatus:
	    *a = new asnInteger(1);	// Always "up" if agent is running
	    break;

	case hpSnmpdConfSize:
	    char    pname[100];
	    int	procfd;		/* fd for /proc/pinfo/nnnnn */
	    struct prpsinfo info;   /* process information structure from /proc */

	    (void) sprintf(pname, "%s%c%05d", _PATH_PROCFSPI, '/', get_pid());

	    if ((procfd = open(pname, O_RDONLY)) == -1)
		return SNMP_ERR_noSuchName;

	    /*
	     * Get the info structure for the process and close quickly.
	     */
	    if (ioctl(procfd, PIOCPSINFO, (char *) &info) == -1) {
		int	saverr = errno;

	    (void) close(procfd);
	    if ((saverr == EACCES) || (saverr == EAGAIN) || (saverr != ENOENT))
	        exc_errlog(LOG_ERR, 0, "PIOCPSINFO on %s: %s",
	    	    pname, strerror(saverr));
	    return SNMP_ERR_noSuchName;
	    }

	    (void) close(procfd);

	    if (info.pr_state == 0)		/* can't happen? */
		return SNMP_ERR_noSuchName;

	    //printf("%6d:%-5d", info.pr_size, info.pr_rssize);	/* SZ:RSS */
	    *a = new asnInteger(info.pr_rssize);
	    break;

	case hpSnmpdConfWhatStr:
	    *a = new asnOctetString(hpsnmpd_what_string);
	    break;

	default:
	    return SNMP_ERR_noSuchName;
	}
    return SNMP_ERR_noError;
}

int
hpSnmpdConfSubAgent::getNext(asnObjectIdentifier *o, asnObject **a, int *t)
{
    return simpleGetNext(o, a, t, hpSnmpdConfWhatStr);
}

int
hpSnmpdConfSubAgent::set(asnObjectIdentifier *o, asnObject **a, int *)
{
    int i;

    // Pull out the subID array and length
    subID *subid;
    unsigned int len;
    oid *oi = o->getValue();
    oi->getValue(&subid, &len);

    // Check that the subID array is of valid size
    if (len != sublen + 2 || subid[sublen + 1] != 0)
        return SNMP_ERR_noSuchName;

    switch (subid[sublen]) {
        case hpSnmpdConfStatus:
            // Check that the object has the right type
            if ((*a)->getTag() != TAG_INTEGER)
                return SNMP_ERR_badValue;

            // Copy the new value
            i = ((asnInteger *) *a)->getValue();
	    if ( i == 2) {
		// Log message with ERR priority, which is the default level
		// so that it will be recorded by default.
	    	exc_errlog(LOG_ERR, 0,
		    "hpsnmpdconfsa: set: No error: Received snmpdStatus command to shut down\n");
	        exit(0);	// A value of 2 (down) shuts down the agent.
	    }
	    else
		return SNMP_ERR_badValue;

        default:
            return SNMP_ERR_noSuchName;
    }
}



