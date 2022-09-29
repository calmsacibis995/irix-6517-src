/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	HP Filesystem sub-agent
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

#include <sys/time.h>
#include <netinet/in.h>
#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include <sys/times.h>
#include <mntent.h>	// for MNTMAXSTR definition
#include <sys/types.h>	// for statfs
#include <sys/statfs.h>

#include "asn1.h"
#include "packet.h"
#include "subagent.h"
#include "sat.h"
#include "table.h"
#include "agent.h"
#include "hpfssa.h"
#include "sgisa.h"
#include "traphandler.h"

extern "C" {
#include "exception.h"
#include "pmapi.h"
}

#include "hpfsmap.h"
#define NMETRICS	(sizeof(_hpfilesys) / sizeof(_hpfilesys[0]))

static void 	updateFsTable();
static pmResult	*resp = NULL;

#define firstval(p, id, sel) (p->vset[id]->numval > 0 ? p->vset[id]->vlist[0].value.sel : 0)

const subID fsMounted	= 1;
const subID fsTable 	= 2;
    const subID fsID1 		= 1;
    const subID fsID2 		= 2;
    const subID fsName 		= 3;
    const subID fsBlock 	= 4;
    const subID fsBfree 	= 5;
    const subID fsBavail 	= 6;
    const subID fsBsize 	= 7;
    const subID fsFiles 	= 8;
    const subID fsFfree 	= 9;
    const subID fsDir 		= 10;

hpFsSubAgent *hpfssa;

hpFsSubAgent::hpFsSubAgent(void)
{
    // Store address for table update routines
    hpfssa = this;

    // Export subtree
    subtree = "1.3.6.1.4.1.11.2.3.1.2";
    int rc = export("hpFileSystem", &subtree);
    if (rc != SNMP_ERR_genErr) {
	exc_errlog(LOG_ERR, 0, 
            "hpfssa: error exporting HP fileSystem subagent");
	return;
    }

    // Define table
    fstable = table(fsTable, 2, updateFsTable);
    fsnumber = 0;
}

hpFsSubAgent::~hpFsSubAgent(void)
{
    unexport(&subtree);
    delete fstable;
    pmDestroyContext(pmWhichContext());
    pmFreeResult(resp);
}

int
hpFsSubAgent::get(asnObjectIdentifier *o, asnObject **a, int *)
{
    // Pull out the subID array and length
    subID *subid;
    unsigned int len;
    oid *oi = o->getValue();
    oi->getValue(&subid, &len);

    // Check that the subID array is of valid size
    if (len == sublen)
	return SNMP_ERR_noSuchName;

    // Handle the fstable table separately
    if (subid[sublen] == fsTable) {	   // subtree.fsTable
	if (len != sublen + 5		   // subtree.table.entry.column.id1.id2
	    || subid[sublen + 1] != 1	   // subtree.table.1.column.id1.id2
	    || subid[sublen + 2] == 0	   // subtree.table.1.(>0).id1.id2
	    || subid[sublen + 2] > fsDir)  // subtree.table.1.(<fsDir).id1.id2
	    return SNMP_ERR_noSuchName;

	updateFsTable();
	return fstable->get(oi, a);
    }

    // Check that the subID array is of valid size for the scalar objects
    // There is only one scalar object in this MIB group: fsMounted.
    if (len != sublen + 2 || subid[sublen + 1] != 0)
	return SNMP_ERR_noSuchName;

    // Switch on the subID and assign the value to *a
    switch (subid[sublen]) {
	case fsMounted:
	    updateFsTable();
	    *a = new snmpGauge(fsnumber);
	    break;
	default:
	    return SNMP_ERR_noSuchName;
    }

    return SNMP_ERR_noError;
}

int
hpFsSubAgent::getNext(asnObjectIdentifier *o, asnObject **a, int *t)
{
    return simpleGetNext(o, a, t, fsTable);
}

int
hpFsSubAgent::set(asnObjectIdentifier *, asnObject **, int *)
{
        // No sets in this MIB group
        return SNMP_ERR_noSuchName;
}

void
updateFsTable(void)
{
    long now;
    static long         last_update_time = 0;
    struct statfs 	statfs_s;

    // Only update once per packet
    extern unsigned int packetCounter;
    static unsigned int validCounter = 0;

    if (validCounter == packetCounter)
	return;
    validCounter = packetCounter;

    now = time((time_t *)0);
    if (((now - last_update_time) < 10) ) // Refresh every 10 secs max.
        return;

    last_update_time = now;

    asnObject 		*a;
    asnInteger 		i;
    asnOctetString 	o;
    snmpCounter 	c;
    snmpGauge 		g;
    subID 		*subid;
    unsigned int 	len;
    int 		rc, sts, numVal, numEntries, fsindex;
    char 		*p;
    pmDesc		desc;
    pmID		pmidlist[NMETRICS];
    pmAtomValue		atomval;

    //iso(1).org(3).dod(6).internet(1).private(4).enterprises(1).
    //	hp(11).nm(2).system(3).general(1).fileSystem(2).
    //	fileSystemTable(2).fileSystemEntry(1).
    //			<columnName>.fileSystemID1(1).fileSystemID2(2)
    oid oi = "1.3.6.1.4.1.11.2.3.1.2.2.1.1.0.0";
    oi.getValue(&subid, &len);


    hpfssa->fsnumber = 0;

    // Re-create a new table from scratch since entries can be in any order
    a = 0;
    for (subID s = fsID1; s <= fsDir; s++) {
        subid[ len - 1] = 0;
        subid[ len - 2] = 0;
	subid[len - 3] = s;
    	for ( ; ; ) {
	    rc = hpfssa->fstable->formNextOID(&oi);
	    if (rc != SNMP_ERR_noError)
	        break;
	    hpfssa->fstable->set(&oi, &a);
	}
    }

    if ((sts = pmLookupName(NMETRICS, _hpfilesys, pmidlist)) < 0) {
        exc_errlog(LOG_ERR, 0,
            "hpfssa: pmLookupName: %s\n", pmErrStr(sts));
        if ((sts = pmLookupDesc(pmidlist[1], &desc)) < 0) {
            exc_errlog(LOG_ERR, 0,
                "hpprocsa: pmLookupDesc: PMID: %s\nReason: %s\n",
                pmIDStr(pmidlist[1]), pmErrStr(sts));
	}
    }

    if (resp != NULL)
	pmFreeResult(resp);

    // Read fileSystem structure from pm subsystem
    if ((sts = pmFetch(NMETRICS, pmidlist, &resp)) < 0) {
        exc_errlog(LOG_ERR, 0,
            "hpfssa: hpfilesys pmFetch: %s\n", pmErrStr(sts));
        return;
    }

#ifdef DEBUG
    _pmDumpResult(stdout, resp);
#endif

    // Scan structure returned from pmFetch
    for ( numEntries = 0; numEntries < resp->numpmid; numEntries++ ) {
	pmValueSet	*vsp = resp->vset[numEntries];

	if (pmLookupDesc(vsp->pmid, &desc) < 0)
	    continue;

	// Fill up table a column at a time
	for ( numVal = 0; numVal < vsp->numval; numVal++) {
	    pmValue *vp = &vsp->vlist[numVal];
#if DEBUG
printf("\nhpfssa: LOOP: numEntries=%d\tnumVal=%d\n", numEntries, numVal);
#endif
	    if (vsp->numval <= 1 && desc.indom == PM_INDOM_NULL)
		continue;

	    fsindex = vp->inst;
            subid[len - 3] = fsID1;
            subid[len - 2] = fsindex;		// fsID1 index value
            subid[len - 1] = 1;			// fsID2 index value
						// Always 1 until I know better
	    i = fsindex;
            a = &i;
            hpfssa->fstable->set(&oi, &a);

	    subid[len - 3] = fsID2;
	    i = 1;				// Always 1 until I know better
            a = &i;
            hpfssa->fstable->set(&oi, &a);

	    // The name of the disk partition is the instance name in PCP
	    subid[len - 3] = fsName;
            pmNameInDom(desc.indom, vp->inst, &p);
            o = p;
            a = &o;
            hpfssa->fstable->set(&oi, &a);
            delete(p);

	    switch (numEntries) 
	    {

	    case FS_CAPACITY:

		subid[len - 3] = fsBlock;
		pmExtractValue(vsp->valfmt, vp, desc.type, &atomval, desc.type);
		i = (unsigned int) 2 * atomval.ul;
		a = &i;
		hpfssa->fstable->set(&oi, &a);
		continue;

	    case FS_FREE:
		subid[len - 3] = fsBfree;
		pmExtractValue(vsp->valfmt, vp, desc.type, &atomval, desc.type);
		i = (unsigned int) 2 * atomval.ul;
		a = &i;
		hpfssa->fstable->set(&oi, &a);

		subid[len - 3] = fsBavail;
		a = &i;
		hpfssa->fstable->set(&oi, &a);
		continue;

	    case FS_MAXFILES:
		subid[len - 3] = fsFiles;
		pmExtractValue(vsp->valfmt, vp, desc.type, &atomval, desc.type);
		i = (unsigned int) atomval.ul;
		a = &i;
		hpfssa->fstable->set(&oi, &a);
		continue;

	    case FS_FREEFILES:
		subid[len - 3] = fsFfree;
		pmExtractValue(vsp->valfmt, vp, desc.type, &atomval, desc.type);
		i = (unsigned int) atomval.ul;
		a = &i;
		hpfssa->fstable->set(&oi, &a);
		continue;

	    case FS_MOUNTDIR:
                subid[len - 3] = fsDir;
		pmExtractValue(vsp->valfmt, vp, desc.type, &atomval, desc.type);
		p = atomval.cp;
                o = p;
                a = &o;
                hpfssa->fstable->set(&oi, &a);

	        // Fill in fsBsize until PCP supports them
		subid[len - 3] = fsBsize;
		sts = statfs (atomval.cp, &statfs_s, sizeof(struct statfs), 0);
		i = (unsigned int) statfs_s.f_bsize;
		a = &i;
		hpfssa->fstable->set(&oi, &a);

	        delete(p);
		continue;
	    }
	}
    }
    hpfssa->fsnumber = numVal;
}
