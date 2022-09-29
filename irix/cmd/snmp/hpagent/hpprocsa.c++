/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	HP processes subagent
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
#include <sys/time.h>
#include <netinet/in.h>
#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include <sys/times.h>
#include <sys/mkdev.h>		// for major() and minor()
#include <sys/proc.h>
#include <pwd.h>		// to read the user name from the password struct


#include "asn1.h"
#include "packet.h"
#include "subagent.h"
#include "sat.h"
#include "table.h"
#include "agent.h"
#include "hpprocsa.h"
#include "sgisa.h"
#include "traphandler.h"

extern "C" {
#include "exception.h"
#include "pmapi.h"
#ifdef PURIFY
#include "purify.h"
#endif
}

#include "hpprocmap.h"
#define NMETRICS        (sizeof(_hpproc) / sizeof(_hpproc[0]))

#define DFLT_CACHE_TIME	15 // Update within DFLT_CACHE_TIME secs
extern int hpGetIntegerValue(pmAtomValue*, int);

static void 	updateProcTable();
static pmID	pmidlist[NMETRICS];
static pmDesc	desc;
static pmResult	*resp = NULL;
static int 	all_n;
static int 	*all_inst;
static char 	**all_names;

static int
int_compare(const void *a, const void *b)
{
    return *(int *)a - *(int *)b;
}

const subID procNumber		= 1;
const subID procTable 		= 2;
    const subID procPID			= 1;
    // const subID procIdx 		= 2;
    const subID procUID 		= 3;
    const subID procPPID 		= 4;
    const subID procDsize 		= 5;
    const subID procTsize 		= 6;
    const subID procSsize 		= 7;
    const subID procNice 		= 8;
    const subID procMajor 		= 9;
    const subID procMinor		= 10;
    const subID procPgrp		= 11;
    const subID procPrio		= 12;
    const subID procAddr		= 13;
    const subID procCPU			= 14;
    const subID procUtime		= 15;
    const subID procStime		= 16;
    const subID procStart		= 17;
    // const subID procFlags		= 18;
    const subID procStatus		= 19;
    const subID procWchan		= 20;
    // const subID procProcNum		= 21;
    const subID procCmd			= 22;
    // const subID procTime		= 23;	
    const subID procCPUticks		= 24;	
    const subID procCPUticksTotal	= 25;
    // const subID procFss		= 26;
    const subID procPctCPU		= 27;
    const subID procRssize		= 28;
    const subID procSUID		= 29;
    const subID procUname		= 30;
    const subID procTTY			= 31;

    // The following entries are added to calculate procPctCPU
    // between two samples.
    const subID procTimestamp		= 32;	// Internal use
    const subID procPrevTimestamp	= 33;	// Internal use
    const subID procPrevCPUticksTotal	= 34;	// Internal use
    const subID procValid		= 35;	// Internal use. 
						// A flag to avoid clearing the whole table
						// each time updateProcTable() is called.

hpProcSubAgent *hpprocsa;

hpProcSubAgent::hpProcSubAgent(void)
{
    int 		i, sts;
    struct timeval  	boot;

    // Store address for table update routines
    hpprocsa = this;

    // Export subtree
    subtree = "1.3.6.1.4.1.11.2.3.1.4";
    int rc = export("hpProcesses", &subtree);
    if (rc != SNMP_ERR_genErr) {
	exc_errlog(LOG_ERR, 0, 
            "hpprocsa: error exporting HP processes subagent");
	return;
    }

    if (gettimeofday(&boot, 0) != 0) {
        exc_errlog(LOG_ERR, 0, "hpprocsa: cannot gettimeofday");
        timerclear(&boot);
	sysboottime_secs = 0;		// For the sake of it.
    }
    else {
        tickspersec = sysconf(_SC_CLK_TCK);
	struct tms tm;
        clock_t uptime = times(&tm);
        sysboottime_secs = boot.tv_sec - uptime / tickspersec;
    }

    if ((sts = pmLookupName(NMETRICS, _hpproc, pmidlist)) < 0) {
        exc_errlog(LOG_ERR, 0,
            "hpprocsa: pmLookupName: %s\n", pmErrStr(sts));
    }

    for (i = 0; i < NMETRICS; i++) {
        if (pmidlist[i] == PM_ID_NULL) {
            exc_errlog(LOG_ERR, 0,
		"hpprocsa: metric not in name space\n");
        }

        if ((sts = pmLookupDesc(pmidlist[1], &desc)) < 0) {
            exc_errlog(LOG_ERR, 0,
		"hpprocsa: pmLookupDesc: PMID: %s\nReason: %s\n",
                pmIDStr(pmidlist[i]), pmErrStr(sts));
        }
    }

    // Define table. Index is only a single integer.
    proctable = table(procTable, 1, updateProcTable);
    procnumber = 0;

    char *cache_timep = getenv("HPSNMPD_PS_CACHE");
    if ((cache_timep != NULL) && (atoi(cache_timep) != 0))
	cache_time = atoi(cache_timep);
    else
	cache_time = DFLT_CACHE_TIME;
    exc_errlog(LOG_INFO, 0, "PS cache time = %d seconds\n", cache_time);
#ifdef MALLOC_AUDIT
    _malloc_reset_();
    atexit(_malloc_audit_);
#endif
}

hpProcSubAgent::~hpProcSubAgent(void)
{
    unexport(&subtree);
    delete proctable;
    pmDestroyContext(pmWhichContext());
    pmFreeResult(resp);
}

int
hpProcSubAgent::get(asnObjectIdentifier *o, asnObject **a, int *)
{
    // Pull out the subID array and length
    subID *subid;
    unsigned int len;
    oid *oi = o->getValue();
    oi->getValue(&subid, &len);

    // Check that the subID array is of valid size
    if (len == sublen)
	return SNMP_ERR_noSuchName;

    // Handle the proctable table separately
    // Note that the columns that are added to compute PctCPU will not
    // show up since we do an OID range check here.
    if (subid[sublen] == procTable) {	   // subtree.procTable
	if (len != sublen + 4		   // subtree.table.entry.column.id1
	    || subid[sublen + 1] != 1	   // subtree.table.1.column.id1
	    || subid[sublen + 2] == 0	   // subtree.table.1.(>0).id1
	    || subid[sublen + 2] > procTTY)  // subtree.table.1.(<procTTY).id1
	    return SNMP_ERR_noSuchName;

	updateProcTable();
	return proctable->get(oi, a);
    }

    // Check that the subID array is of valid size for the scalar objects
    // There is only one scalar object in this MIB group: fsMounted.
    if (len != sublen + 2 || subid[sublen + 1] != 0)
	return SNMP_ERR_noSuchName;

    // Switch on the subID and assign the value to *a
    switch (subid[sublen]) {
	case procNumber:
	    updateProcTable();
	    *a = new snmpGauge(procnumber);
	    break;
	default:
	    return SNMP_ERR_noSuchName;
    }

    return SNMP_ERR_noError;
}

int
hpProcSubAgent::getNext(asnObjectIdentifier *o, asnObject **a, int *t)
{
    return simpleGetNext(o, a, t, procTable);
}

int
hpProcSubAgent::set(asnObjectIdentifier *, asnObject **, int *)
{
        // No sets in this MIB group
        return SNMP_ERR_noSuchName;
}

void
updateProcTable(void)
{
    extern unsigned 	int packetCounter;
    static unsigned 	int validCounter = 0;
    static long     	last_update_time = 0;
    unsigned long   	ps_starttime;
    // unsigned long 	totalProcCPUticks;
    long            	now;
    static unsigned 	int proc_timestamp = 0;


    if (validCounter == packetCounter)
	return;
    validCounter = packetCounter;

    now=time((time_t *)0);

    if (((now - last_update_time) < hpprocsa->cache_time) )
        return;

#ifdef PURIFY
    purify_new_leaks();
#endif

#ifdef MALLOC_AUDIT
    _malloc_audit_();
#endif

    last_update_time = now;
    // totalProcCPUticks = 0;

    asnObject 		*a;
    asnInteger 		i;
    asnOctetString 	o;
    snmpCounter 	c;
    snmpGauge 		g;
    snmpTimeticks 	t;

    subID 		*subid;
    unsigned int 	len;
    int 		rc, sts, numVal, numEntries;
    unsigned int	tmpVal;
    int			itmpVal;
    char                *p;
    pmAtomValue		atomval;
    pmValueSet		*vsp;
    pmValue 		*vp;

    //iso(1).org(3).dod(6).internet(1).private(4).enterprises(1).
    //	hp(11).nm(2).system(3).general(1).processes(4).
    //	processTable(2).processEntry(1).<columnName>.processPID(1)
    oid oi = "1.3.6.1.4.1.11.2.3.1.4.2.1.1.1";
    oi.getValue(&subid, &len);

    hpprocsa->procnumber = 0;

    // Re-create a new table from scratch since entries can be in any order
    // a = 0;
    // for (subID s = procPID; s <= procTTY; s++) {
        // subid[ len - 1] = 0;
        // subid[ len - 2] = s;
        // for ( ; ; ) {
            // rc = hpprocsa->proctable->formNextOID(&oi);
            // if (rc != SNMP_ERR_noError)
                // break;
            // hpprocsa->proctable->set(&oi, &a);
        // }
    // }

    if (desc.indom != PM_INDOM_NULL) {
        all_n = pmGetInDom(desc.indom, &all_inst, &all_names);
        if (all_n < 0) {
            exc_errlog(LOG_ERR, 0,
		"hpprocsa: pmGetInDom: %s\n", pmErrStr(all_n));
            return;	// No need to kill agent just for that.
			// Agent will simply return noSuchName.
        }
    }

    // sort the instance identifiers
    qsort(all_inst, all_n, sizeof(int), int_compare);

    // establish an explicit instance profile
    pmDelProfile(desc.indom, 0, (int *)0); // exclude everything first
    pmAddProfile(desc.indom, all_n, all_inst); // and then explicitly include our list of pids

    if (resp != NULL)
	pmFreeResult(resp);

    // Read proc table structure from pm subsystem
    if ((sts = pmFetch(NMETRICS, pmidlist, &resp)) < 0) {
        exc_errlog(LOG_ERR, 0,
            "hpprocsa: hpproc pmFetch: %s\n", pmErrStr(sts));
        return;
    }

#if 0
    // Use this section for debugging
    FILE *fd = fopen("hpproc.out", "w");
    _pmDumpResult(fd, resp);
    fclose(fd);
#endif

    // Scan structure returned from pmFetch
    for ( numEntries = 0; numEntries < resp->numpmid; numEntries++ ) {
	ps_starttime = 0;
	vsp = resp->vset[numEntries];

	if (pmLookupDesc(vsp->pmid, &desc) < 0) {
            exc_errlog(LOG_ERR, 0,
		"hpprocsa: pmLookupDesc: PMID: %s\nReason: %s\n",
                pmIDStr(vsp->pmid), pmErrStr(sts));
	    continue;		// Means: noSuchName
	}

	// Fill up table a column at a time
	for ( numVal = 0; numVal < vsp->numval; numVal++) {
	    vp = &vsp->vlist[numVal];
	    if (vsp->numval <= 1 && desc.indom == PM_INDOM_NULL)
		continue;

	    subid[len - 1] = vp->inst;	// Set the index to be the PID

	    // Set this entry to be valid and move its previous timestamp and CPU time
	    // into internal MIB variables.

	    subid[len - 2] = procValid;
	    i = validCounter;
	    a = &i;
	    hpprocsa->proctable->set(&oi, &a);

	    // Scan the metrics returned by the PCP pmFetch() call.

	    switch (numEntries) 
	    {
	    // Entries not defined are not supported by SGI and will end up
	    // as noSuchName to the SNMP Manager.

            case PT_PID:
                subid[len - 2] = procPID;
		pmExtractValue(vsp->valfmt, vp, desc.type, &atomval, desc.type);
		i = (unsigned int) hpGetIntegerValue(&atomval, desc.type);
                a = &i;
                hpprocsa->proctable->set(&oi, &a);
		continue;

	    case PT_UID:
		subid[len - 2] = procUID;
		pmExtractValue(vsp->valfmt, vp, desc.type, &atomval, desc.type);
		i = (unsigned int) hpGetIntegerValue(&atomval, desc.type);
		a = &i;
		hpprocsa->proctable->set(&oi, &a);
		continue;

	    case PT_UNAME:
		subid[len - 2] = procUname;
		pmExtractValue(vsp->valfmt, vp, desc.type, &atomval, desc.type);
		p = atomval.cp;
            	o = p;
            	a = &o;
                hpprocsa->proctable->set(&oi, &a);
            	delete(atomval.cp);
		continue;

	    case PT_PPID:
		subid[len - 2] = procPPID;
		pmExtractValue(vsp->valfmt, vp, desc.type, &atomval, desc.type);
		i = (unsigned int) hpGetIntegerValue(&atomval, desc.type);
		a = &i;
		hpprocsa->proctable->set(&oi, &a);
		continue;

	    case PT_SIZE_DAT:
		subid[len - 2] = procDsize;
		pmExtractValue(vsp->valfmt, vp, desc.type, &atomval, desc.type);
		tmpVal = (unsigned int) hpGetIntegerValue(&atomval, desc.type);

		// Need to read two more metrics to add to.

		vsp = resp->vset[PT_SIZE_SHM];
		vp = &vsp->vlist[numVal];
        	if (pmLookupDesc(vsp->pmid, &desc) >= 0) {
		    pmExtractValue(vsp->valfmt, vp, desc.type, &atomval, desc.type);
		    tmpVal += (unsigned int) hpGetIntegerValue(&atomval, desc.type);
		}

		vsp = resp->vset[PT_SIZE_BSS];
		vp = &vsp->vlist[numVal];
        	if (pmLookupDesc(vsp->pmid, &desc) >= 0) {
		    pmExtractValue(vsp->valfmt, vp, desc.type, &atomval, desc.type);
		    tmpVal += (unsigned int) hpGetIntegerValue(&atomval, desc.type);
		}

		// Metric is in Kbytes. MIB object is in pages. 1 page = 4 Kbytes
		g = (unsigned int) 1024 * tmpVal / getpagesize();
		a = &g;
		hpprocsa->proctable->set(&oi, &a);
		// Restore the orignal vsp and vp pointers
		vsp = resp->vset[numEntries];
                vp = &vsp->vlist[numVal];
		continue;

	    case PT_SIZE_TXT:
		subid[len - 2] = procTsize;
		pmExtractValue(vsp->valfmt, vp, desc.type, &atomval, desc.type);
		// Metric is in Kbytes. MIB object is in pages. 1 page = 4 Kbytes
		g = (unsigned int) 1024 * hpGetIntegerValue(&atomval, desc.type) / getpagesize();
		a = &g;
		hpprocsa->proctable->set(&oi, &a);
		continue;

	    case PT_SIZE_STACK:
		subid[len - 2] = procSsize;
		pmExtractValue(vsp->valfmt, vp, desc.type, &atomval, desc.type);
		// Metric is in Kbytes. MIB object is in pages. 1 page = 4 Kbytes
		g = (unsigned int) 1024 * hpGetIntegerValue(&atomval, desc.type) / getpagesize();
		a = &g;
		hpprocsa->proctable->set(&oi, &a);
		continue;

	    case PT_NICE:
		subid[len - 2] = procNice;
		pmExtractValue(vsp->valfmt, vp, desc.type, &atomval, desc.type);
		g = (unsigned int) hpGetIntegerValue(&atomval, desc.type);
		a = &g;
		hpprocsa->proctable->set(&oi, &a);
		continue;

	    case PT_TTYNAME:
		subid[len - 2] = procTTY;
		pmExtractValue(vsp->valfmt, vp, desc.type, &atomval, desc.type);
		p = atomval.cp;
            	o = p;
            	a = &o;
                hpprocsa->proctable->set(&oi, &a);
            	delete(atomval.cp);
		continue;

	    case PT_TTYMAJOR:
		subid[len - 2] = procMajor;
		pmExtractValue(vsp->valfmt, vp, desc.type, &atomval, desc.type);
		i = (unsigned int) hpGetIntegerValue(&atomval, desc.type);
		a = &i;
		hpprocsa->proctable->set(&oi, &a);
		continue;

	    case PT_TTYMINOR:
		subid[len - 2] = procMinor;
		pmExtractValue(vsp->valfmt, vp, desc.type, &atomval, desc.type);
		i = (unsigned int) hpGetIntegerValue(&atomval, desc.type);
		a = &i;
		hpprocsa->proctable->set(&oi, &a);
		continue;

            // case PT_PGRP:	// Did not seem to report the correct group id
            case PT_GID:
		subid[len - 2] = procPgrp;
		pmExtractValue(vsp->valfmt, vp, desc.type, &atomval, desc.type);
		i = (unsigned int) hpGetIntegerValue(&atomval, desc.type);
		a = &i;
		hpprocsa->proctable->set(&oi, &a);
		continue;

            // case PT_OLDPRI:
            case PT_PRI:
		subid[len - 2] = procPrio;
		pmExtractValue(vsp->valfmt, vp, desc.type, &atomval, desc.type);
		i = (unsigned int) hpGetIntegerValue(&atomval, desc.type);
		a = &i;
		hpprocsa->proctable->set(&oi, &a);
		continue;

            case PT_ADDR:
		subid[len - 2] = procAddr;
		pmExtractValue(vsp->valfmt, vp, desc.type, &atomval, desc.type);
		i = (unsigned int) hpGetIntegerValue(&atomval, desc.type);
		a = &i;
		hpprocsa->proctable->set(&oi, &a);
		continue;

            case PT_CPU:
		subid[len - 2] = procCPU;
		pmExtractValue(vsp->valfmt, vp, desc.type, &atomval, desc.type);
		g = (unsigned int) hpGetIntegerValue(&atomval, desc.type);
		a = &g;
		hpprocsa->proctable->set(&oi, &a);
		continue;

            case PT_UTIME:
		subid[len - 2] = procUtime;
		pmExtractValue(vsp->valfmt, vp, desc.type, &atomval, desc.type);
		// Metric is in msec while Timetick = 1/100th sec
		t = (unsigned int) (hpGetIntegerValue(&atomval, desc.type)) / 10;
		a = &t;
		hpprocsa->proctable->set(&oi, &a);
		continue;

            case PT_STIME:
		subid[len - 2] = procStime;
		pmExtractValue(vsp->valfmt, vp, desc.type, &atomval, desc.type);
		// Metric is in msec while Timetick = 1/100th sec
		t = (unsigned int) (hpGetIntegerValue(&atomval, desc.type)) / 10;
		a = &t;
		hpprocsa->proctable->set(&oi, &a);
		continue;

            case PT_STARTTIME:
		//
		//		host	  agent	       process
		// 		boot	  boot		start
		//    epoch     time      time          time         now
		// -----+--------+----------+-------------+-----------+----->
		//
		//      <------------ gettimeofday() ----------------->
		//               <---- times() ----------------------->
		//
		//                          <------- sysUpTime ------->
		//	         <---- processStart ------>
		//	  	 <--------- hpSysUpTime -------------->
		//  

		subid[len - 2] = procStart;
		pmExtractValue(vsp->valfmt, vp, desc.type, &atomval, desc.type);
		
		ps_starttime = (unsigned long) hpGetIntegerValue(&atomval, desc.type);
		// Metric is in seconds while Timetick = 1/100th sec
		itmpVal = (int) 100 * 
		    ((time_t) hpGetIntegerValue(&atomval, desc.type) - (hpprocsa->sysboottime_secs));
		if ( itmpVal < 0 )	// Correct round-off errors
		    t = (unsigned int) 0;
		else
		    t = (unsigned int) itmpVal;
		a = &t;
		hpprocsa->proctable->set(&oi, &a);
		continue;

            case PT_TSTAMP:
		subid[len - 2] = procTimestamp;
		pmExtractValue(vsp->valfmt, vp, desc.type, &atomval, desc.type);
		proc_timestamp = (unsigned int) hpGetIntegerValue(&atomval, desc.type);
		c = (unsigned int) hpGetIntegerValue(&atomval, desc.type);
		a = &c;
		hpprocsa->proctable->set(&oi, &a);
		continue;

            case PT_STATE:
		subid[len - 2] = procStatus;
		pmExtractValue(vsp->valfmt, vp, desc.type, &atomval, desc.type);
		unsigned int ps_state = (unsigned int) hpGetIntegerValue(&atomval, desc.type);

		// See the stat codes in sys/proc.h for p_stat

		if (ps_state == SSLEEP)
		    i = (unsigned int) 1;	// sleep
		else if (ps_state == SRUN)
		    i = (unsigned int) 2;	// run
		else if (ps_state == SZOMB)
		    i = (unsigned int) 4;	// zombie
		else if (ps_state == SSTOP)
		    i = (unsigned int) 3;	// stop
		else if (ps_state == SIDL)
		    i = (unsigned int) 6;	// idle
		else
		    i = (unsigned int) 5;	// other

		a = &i;
		hpprocsa->proctable->set(&oi, &a);
		continue;

            case PT_WCHAN:
		subid[len - 2] = procWchan;
		pmExtractValue(vsp->valfmt, vp, desc.type, &atomval, desc.type);
		i = (unsigned int) hpGetIntegerValue(&atomval, desc.type);
		a = &i;
		hpprocsa->proctable->set(&oi, &a);
		continue;

	    case PT_FNAME:
		subid[len - 2] = procCmd;
		pmExtractValue(vsp->valfmt, vp, desc.type, &atomval, desc.type);
		p = atomval.cp;
            	o = p;
            	a = &o;
		hpprocsa->proctable->set(&oi, &a);
            	delete(atomval.cp);
		continue;

            case PT_TIME: 
		unsigned int proc_prev_ticks_total, proc_ticks_total;
		unsigned int proc_prev_tstamp, proc_tstamp;
		asnObject *procPrevCPUticks_a, *procPrevTstamp_a, *procTstamp_a;

		subid[len - 2] = procCPUticksTotal;
		pmExtractValue(vsp->valfmt, vp, desc.type, &atomval, desc.type);

                // Metric is in msec while Timetick = 1/100th sec
		// Save it for calculation of PctCPU
		proc_ticks_total = (unsigned int) (hpprocsa->tickspersec * hpGetIntegerValue(&atomval, desc.type)) / 1000;
		c = (unsigned int) proc_ticks_total;	
		a = &c;
		hpprocsa->proctable->set(&oi, &a);

		// Calculate procCPUticks as the difference between the previous value
		// and this value.

		subid[len -2] = procPrevCPUticksTotal;
		rc = hpprocsa->proctable->get(&oi, &procPrevCPUticks_a);
		if (rc == SNMP_ERR_noError) {
		    proc_prev_ticks_total = ((asnInteger *)procPrevCPUticks_a)->getValue();
		    delete procPrevCPUticks_a;			// Free local copy
		}
		else 
		    proc_prev_ticks_total = 0;

		c = (unsigned int) proc_ticks_total;		// Save value for next scan
		a = &c;
		hpprocsa->proctable->set(&oi, &a);

		subid[len -2] = procCPUticks;		// Save MIB object as delta value
		c = (unsigned int) proc_ticks_total - proc_prev_ticks_total;
		a = &c;
		hpprocsa->proctable->set(&oi, &a);

		// Calculate and save the procPctCPU MIB object
		// since we can get procPrevTimestamp, procTimestamp, procPrevCPUticks and
		// procCPUticks for this process instance (e.g., row) from the saved
		// MIB tree.
		  
		subid[len -2] = procPrevTimestamp;
		rc = hpprocsa->proctable->get(&oi, &procPrevTstamp_a);
		if (rc == SNMP_ERR_noError) {
		    proc_prev_tstamp = ((asnInteger *)procPrevTstamp_a)->getValue();
		    delete procPrevTstamp_a;			// Free local copy
		}
		else 
		    proc_prev_tstamp = 0;

		// We could get tprocTstamp_a procTimestamp value from the pmResult structure, but
		// this is simpler code.
		subid[len -2] = procTimestamp;
		rc = hpprocsa->proctable->get(&oi, &procTstamp_a);
		if (rc == SNMP_ERR_noError) {
		    proc_tstamp = ((asnInteger *)procTstamp_a)->getValue();
		    delete procTstamp_a;			// Free local copy
		}
		else 
		    proc_tstamp = 0;

		proc_tstamp = proc_timestamp;

		subid[len -2] = procPrevTimestamp;
		c = (unsigned int) proc_tstamp;
		a = &c;				// Save value for next scan
		hpprocsa->proctable->set(&oi, &a);

		subid[len -2] = procPctCPU;
		if (((int)(proc_tstamp - proc_prev_tstamp) <= 0) 
		    || ((int) (proc_ticks_total - proc_prev_ticks_total) < 0)
		    || (proc_prev_ticks_total == 0)
		    || (proc_prev_tstamp == 0))		// Ignore the first sample and set Pct to zero
		    g = (unsigned int) 0;
		else
		    g = (unsigned int) ( 100 * (proc_ticks_total - proc_prev_ticks_total) / (proc_tstamp - proc_prev_tstamp));

		a = &g;
		hpprocsa->proctable->set(&oi, &a);
		continue;

            case PT_RSS: 
		subid[len - 2] = procRssize;
		pmExtractValue(vsp->valfmt, vp, desc.type, &atomval, desc.type);
		// Metric is in Kbytes. MIB object is in pages. 1 page = 4 Kbytes
		g = (unsigned int) 1024 *
		    hpGetIntegerValue(&atomval, desc.type) / getpagesize();
		a = &g;
		hpprocsa->proctable->set(&oi, &a);
		continue;

            case PT_SUID:
		subid[len - 2] = procSUID;
		pmExtractValue(vsp->valfmt, vp, desc.type, &atomval, desc.type);
		i = (unsigned int) hpGetIntegerValue(&atomval, desc.type);
		a = &i;
		hpprocsa->proctable->set(&oi, &a);
		continue;
	    }
	}
    }

    // Remove invalid entries
    subid[len - 2] = procValid;
    subid[len - 1] = 0;

    // There is a known problem here that the agent core cannot set an oid
    // with a value of zero. So, the process with a PID of zero will never be checked.
    // On IRIX, there is always a process with PID of zero. If it dies and is
    // restarted, then the entry will remain forever in the process table. This
    // will cause a waste of memory but not a memory leak.
    for ( ; ; ) {
        rc = hpprocsa->proctable->formNextOID(&oi);
        if (rc != SNMP_ERR_noError)
            break;
        rc = hpprocsa->proctable->get(&oi, &a);
	// Delete all entries which are not marked with a procValid field
	// that was set or reset in this update pass.
        if (rc == SNMP_ERR_noError
            && ((asnInteger *) a)->getValue() != validCounter) {

	delete a;			// Free local copy
        // Entry is invalid, delete it
            a = 0;
            for (subID s = procPID; s <= procValid; s++) {
	        subid[len - 2] = s;
	        hpprocsa->proctable->set(&oi, &a);
            }
        }
	else
	    delete a;			// Free local copy
    }

    hpprocsa->procnumber = numVal;
    delete(all_inst);
    delete(all_names);
}
