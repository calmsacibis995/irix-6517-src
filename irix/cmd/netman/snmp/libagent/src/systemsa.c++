/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	System sub-agent
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
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <syslog.h>
#include <bstring.h>
#include <string.h>
#include <unistd.h>
#include <sys/times.h>
#include <errno.h>
#include <fcntl.h>

#include "oid.h"
#include "asn1.h"
#include "snmp.h"
#include "packet.h"
#include "subagent.h"
#include "sat.h"
#include "agent.h"
#include "systemsa.h"
#include "sgisa.h"
#include "traphandler.h"

extern "C" {
int gethostname(char *, int);		// XXX - Where is this?
#include "exception.h"
}

const subID sysDescr = 1;
const subID sysObjectID = 2;
const subID sysUpTime = 3;
const subID sysContact = 4;
const subID sysName = 5;
const subID sysLocation = 6;
const subID sysServices = 7;

extern sgiHWSubAgent *sgihwsa;
extern snmpTrapHandler *traph;

systemSubAgent *syssa;

const char *datafile = "/etc/snmpd.data";

systemSubAgent::systemSubAgent(void)
{
	// Store address for use by other subagents
	syssa = this;

    // Export subtree
    subtree = "1.3.6.1.2.1.1";
    int rc = export("system", &subtree);
    if (rc != SNMP_ERR_genErr) {
	log(LOG_ERR, 0, "systemsa: error exporting system subagent");
	return;
    }

    char tmp[64];
    if (gethostname(tmp, 63) != 0)
	tmp[0] = 0;
    struct hostent *he = gethostbyname(tmp);
    if (he != 0) {
	sysname = new char[strlen(he->h_name) + 1];
	strcpy(sysname, he->h_name);
    } else {
	sysname = new char[1];
	*sysname = 0;
    }

    // Read contact and location from stable storage
    // XXX - make this more general
    FILE *fp = fopen(datafile, "r");
    if (fp == 0) {
	contact = new char[1];
	*contact = 0;
	location = new char[1];
	*location = 0;
        exc_errlog(LOG_WARNING, errno,
            "systemSubAgent: Cannot read config file %s. Assuming no Contact or Location entries.",
            datafile);
    }
    else {
	char line[512], *soid, *sval, *end;
	while (fgets(line, 511, fp) != 0) {
	    soid = strtok(line, " \t\n");
	    if (soid == 0)
		continue;
	    sval = soid + strlen(soid) + 1;
	    sval = strchr(sval, '\"');
	    if (sval == 0)
		continue;
	    sval++;
	    end = strchr(sval, '\"');
	    if (end == 0)
		continue;
	    *end = '\0';
	    if (strcmp(soid, "1.3.6.1.2.1.1.4.0") == 0) {
		contact = new char[strlen(sval) + 1];
		strcpy(contact, sval);
	    } else if (strcmp(soid, "1.3.6.1.2.1.1.6.0") == 0) {
		location = new char[strlen(sval) + 1];
		strcpy(location, sval);
	    }
	}
	fclose(fp);
    }
}

systemSubAgent::~systemSubAgent(void)
{
    unexport(&subtree);
}

int
systemSubAgent::get(asnObjectIdentifier *o, asnObject **a, int *)
{
    const int sysServicesValue = 72;

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
	case sysDescr:
	    *a = sgihwsa->getSystemDesc();
	    break;
	case sysObjectID:
	    *a = sgihwsa->getSystemType();
	    break;
	case sysUpTime:
	    {
		struct timeval now;
		if (gettimeofday(&now, 0) != 0)
		    return SNMP_ERR_genErr;
		*a = new snmpTimeticks((int) (100 * (now.tv_sec - boot.tv_sec) +
					(now.tv_usec - boot.tv_usec) / 10000));
	    }
	    break;
	case sysContact:
	    *a = new asnOctetString(contact);
	    break;
	case sysName:
	    *a = new asnOctetString(sysname);
	    break;
	case sysLocation:
	    *a = new asnOctetString(location);
	    break;
	case sysServices:
	    *a = new asnInteger(sysServicesValue);
	    break;
	default:
	    return SNMP_ERR_noSuchName;
    }
    return SNMP_ERR_noError;
}

int
systemSubAgent::getNext(asnObjectIdentifier *o, asnObject **a, int *t)
{
    return simpleGetNext(o, a, t, sysServices);
}

int
systemSubAgent::set(asnObjectIdentifier *o, asnObject **a, int *)
{
    char *v;

    // Pull out the subID array and length
    subID *subid;
    unsigned int len;
    oid *oi = o->getValue();
    oi->getValue(&subid, &len);

    // Check that the subID array is of valid size
    if (len != sublen + 2 || subid[sublen + 1] != 0)
	return SNMP_ERR_noSuchName;

    switch (subid[sublen]) {
	case sysContact:
	    // Check that the object has the right type
	    if ((*a)->getTag() != TAG_OCTET_STRING)
		return SNMP_ERR_badValue;

	    // Copy the new value
	    v = ((asnOctetString *) (*a))->getValue();
	    len = ((asnOctetString *) (*a))->getLength();
	    delete contact;
	    contact = new char[len + 1];
	    bcopy(v, contact, len);
	    contact[len] = '\0';
	    return writeDatafile();

	case sysName:
	    // Check that the object has the right type
	    if ((*a)->getTag() != TAG_OCTET_STRING)
		return SNMP_ERR_badValue;

	    // Copy the new value
	    v = ((asnOctetString *) (*a))->getValue();
	    len = ((asnOctetString *) (*a))->getLength();
	    delete sysname;
	    sysname = new char[len + 1];
	    bcopy(v, sysname, len);
	    sysname[len] = '\0';
	    return writeSysname();

	case sysLocation:
	    // Check that the object has the right type
	    if ((*a)->getTag() != TAG_OCTET_STRING)
		return SNMP_ERR_badValue;

	    // Copy the new value
	    v = ((asnOctetString *) (*a))->getValue();
	    len = ((asnOctetString *) (*a))->getLength();
	    delete location;
	    location = new char[len + 1];
	    bcopy(v, location, len);
	    location[len] = '\0';
	    return writeDatafile();

	default:
	    return SNMP_ERR_noSuchName;
    }
}

int
systemSubAgent::writeDatafile(void)
{
    FILE *fp = fopen(datafile, "w");
    if (fp == 0) {
        exc_errlog(LOG_WARNING, errno,
            "systemSubAgent: Cannot write to config file %s.",
            datafile);
	return SNMP_ERR_genErr;
    }

    int rc = SNMP_ERR_noError;

    if (fprintf(fp, "1.3.6.1.2.1.1.4.0 = \"%s\"\n", contact) < 0
	|| fprintf(fp, "1.3.6.1.2.1.1.6.0 = \"%s\"\n", location) < 0)
	rc = SNMP_ERR_genErr;

    if (fclose(fp) < 0)
	rc = SNMP_ERR_genErr;

    if (chmod(datafile, 00600) < 0)
	rc = SNMP_ERR_genErr;

    return rc;
}

int
systemSubAgent::writeSysname(void)
{
    FILE *fp = fopen("/etc/sys_id", "w");
    if (fp == 0) {
        exc_errlog(LOG_WARNING, errno,
            "systemSubAgent: Cannot write to file /etc/sys_id.");
	return SNMP_ERR_genErr;
    }

    int rc = SNMP_ERR_noError;

    if (fputs(sysname, fp) < 0)
	rc = SNMP_ERR_genErr;
    if (fputs("\n", fp) < 0)    // write a newline after system name --vaz
	rc = SNMP_ERR_genErr;

    if (fclose(fp) < 0)
	rc = SNMP_ERR_genErr;

    return rc;
}

void
systemSubAgent::setBoot(timeval *boottime)
{
    bcopy (boottime, &boot, sizeof boot);
}
