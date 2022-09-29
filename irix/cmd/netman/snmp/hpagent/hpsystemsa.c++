/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	System sub-agent
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
#include <unistd.h>

#include "asn1.h"
#include "packet.h"
#include "subagent.h"
#include "sat.h"
#include "agent.h"
#include "hpsystemsa.h"
#include "sgisa.h"
#include "traphandler.h"

#include "hpsysmap.h"

// private functions
static double ulong_diff(ulong_t, ulong_t);
static void updateCPUcounts();

#define MAX_MID	100
#define firstval(p, id, sel) (p->vset[id]->numval > 0 ? p->vset[id]->vlist[0].value.sel : 0)

extern "C" {
#include "exception.h"
#include "pmapi.h"
}

const subID hpSysUpTime 	= 1;
const subID hpSysUsers 		= 2;
const subID hpSysAvgJobs1 	= 3;
const subID hpSysAvgJobs5 	= 4;
const subID hpSysAvgJobs15 	= 5;
const subID hpSysMaxProcs 	= 6;
const subID hpSysFreeMem 	= 7;
const subID hpSysPhysMem 	= 8;
const subID hpSysMaxUserMem 	= 9;
const subID hpSysSwapConfig 	= 10;
const subID hpSysEnabledSwap 	= 11;
const subID hpSysFreeSwap 	= 12;
const subID hpSysUserCPU 	= 13;
const subID hpSysSysCPU 	= 14;
const subID hpSysIdleCPU 	= 15;
const subID hpSysNiceCPU 	= 16;

extern sgiHWSubAgent *sgihwsa;
extern snmpTrapHandler *traph;

static pmResult	*resp = NULL, *cpu_resp = NULL, *cpu_prev = NULL;
static pmID	pmidlist[MAX_MID];
static double 	sum_time;
static int	sts;
static int	numpmid;

hpSystemSubAgent *hpsyssa;

hpSystemSubAgent::hpSystemSubAgent(void)
{
    // Store address for use by other subagents
    hpsyssa = this;

    // Export subtree
    subtree = "1.3.6.1.4.1.11.2.3.1.1";
    int rc = export("hpComputerSystem", &subtree);
    if (rc != SNMP_ERR_genErr) {
	exc_errlog(LOG_ERR, 0, 
	    "hpsystemsa: error exporting HP computerSystem subagent");
	return;
    }
}

hpSystemSubAgent::~hpSystemSubAgent(void)
{
    pmDestroyContext(pmWhichContext());

    if ( cpu_prev != NULL )
    	pmFreeResult(cpu_prev);
    if ( cpu_resp != NULL )
    	pmFreeResult(cpu_resp);
    if ( resp != NULL )
    	pmFreeResult(resp);
    unexport(&subtree);
}

int
hpSystemSubAgent::get(asnObjectIdentifier *o, asnObject **a, int *)
{
    static int nUsers, freeMem, physMem, maxUMem, maxSwap, maxProcs;
    static int loadAvg1, loadAvg5, loadAvg15;
    static int freeSwap;
    static int userCPU, sysCPU, idleCPU;

    int 		i, inst;
    pmDesc		desclist[MAX_MID];
    pmAtomValue		la;

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
	case hpSysUpTime: {
	    struct tms tm;
	    clock_t uptime;
	    if ((uptime = times(&tm)) == -1)
	        return SNMP_ERR_noSuchName;

	    // Timeticks are in one-hundredth of a second
            long tickspersec = sysconf(_SC_CLK_TCK);
	    // Leave the multiplication last or risk round-off error
	    *a = new snmpTimeticks((int) ((unsigned long)uptime / tickspersec * 100));
	    break;
	    }

	case hpSysUsers:
	    numpmid = sizeof(_hpSysUsers) / sizeof(char *);
	    if ((sts = pmLookupName(numpmid, _hpSysUsers, pmidlist)) < 0) {
		exc_errlog(LOG_ERR, 0,
            	    "hpsystemsa: pmLookupName of _hpSysUsers:  %s\n", pmErrStr(sts));
	    }

	    if ((sts = pmFetch(numpmid, pmidlist, &resp)) < 0) {
        	exc_errlog(LOG_ERR, 0,
		    "hpsystemsa: hpSysUsers pmFetch: %s\n", pmErrStr(sts));
		return SNMP_ERR_noSuchName;
    	    }
	    nUsers = firstval(resp, NUSERS, lval);
	    exc_errlog(LOG_DEBUG, 0, "hpsystemsa: hpSysUsers = %d\n", nUsers);
	    pmFreeResult(resp);
	    *a = new snmpGauge(nUsers);
	    break;

	case hpSysAvgJobs1:
	case hpSysAvgJobs5:
	case hpSysAvgJobs15:
	    numpmid = sizeof(_hpSysAvgJobs) / sizeof(char *);
            if ((sts = pmLookupName(numpmid, _hpSysAvgJobs, pmidlist)) < 0) {
                exc_errlog(LOG_ERR, 0,
                    "hpsystemsa: pmLookupName of _hpSysAvgJobs:  %s\n", pmErrStr(sts));
            }

	    if ((sts = pmFetch(numpmid, pmidlist, &resp)) < 0) {
        	exc_errlog(LOG_ERR, 0, 
		    "hpsystemsa: hpSysAvgJobs pmFetch: %s\n", pmErrStr(sts));
		return SNMP_ERR_noSuchName;
    	    }

	    // Need to verify the instance ids: 1, 5 and 15
            for (i = 0; i < numpmid; i++) {
                if ((sts = pmLookupDesc(pmidlist[i], &desclist[i])) < 0) {
                    exc_errlog(LOG_ERR, 0,
			"hpsystemsa: cannot retrieve description for metric \"%s\" (PMID: %s) -  Reason: %s\n",
                        _hpSysAvgJobs[i], pmIDStr(pmidlist[i]), pmErrStr(sts));
		    return SNMP_ERR_noSuchName;
		}
            }

	    switch (subid[sublen]) {
	        case hpSysAvgJobs1:
            	    if (desclist[LOADAV].indom != PM_INDOM_NULL) {
                	pmDelProfile(desclist[LOADAV].indom, 0, (int *)0);  // all off   
                	if ((inst = pmLookupInDom(desclist[LOADAV].indom, "1 minute")) >= 0) 
                            pmAddProfile(desclist[LOADAV].indom, 0, &inst);  // enable all
            	    }

	            if (resp->vset[LOADAV]->pmid != PM_ID_NULL) {
                        pmExtractValue(resp->vset[LOADAV]->valfmt,
                            &resp->vset[LOADAV]->vlist[0], desclist[LOADAV].type,
                            &la, PM_TYPE_FLOAT);
	 	        loadAvg1=(int)((la.f * 1000 + 5) / 10);   // to round off high
	    	    }
            	    else 	// load average, not available
                        loadAvg1 = 0;

            	    exc_errlog(LOG_DEBUG, 0, 
			"hpsystemsa: hpSysAvgJobs1: %d\n", loadAvg1);
	    	    *a = new snmpGauge(loadAvg1);
	    	    break;

	        case hpSysAvgJobs5:
                    if (desclist[LOADAV].indom != PM_INDOM_NULL) {
                        pmDelProfile(desclist[LOADAV].indom, 0, (int *)0);  // all off
                        if ((inst = pmLookupInDom(desclist[LOADAV].indom, "5 minute")) >= 0) 
                            pmAddProfile(desclist[LOADAV].indom, 0, &inst); // enable all
                    }

	            if (resp->vset[LOADAV]->pmid != PM_ID_NULL) {
                        pmExtractValue(resp->vset[LOADAV]->valfmt,
                            &resp->vset[LOADAV]->vlist[1], desclist[LOADAV].type,
                            &la, PM_TYPE_FLOAT);
	 	        // loadAvg5=(int) (la.f * 100);
	 	        loadAvg5=(int)((la.f * 1000 + 5) / 10);   // to round off high
	            }
                    else 	// load average, not available
                        loadAvg5 = 0;

            	    exc_errlog(LOG_DEBUG, 0, 
                        "hpsystemsa: hpSysAvgJobs5: %d\n", loadAvg5);
	    	    *a = new snmpGauge(loadAvg5);
	    	    break;

	        case hpSysAvgJobs15:
                    if (desclist[LOADAV].indom != PM_INDOM_NULL) {
                        pmDelProfile(desclist[LOADAV].indom, 0, (int *)0);  // all off   
                        if ((inst = pmLookupInDom(desclist[LOADAV].indom, "15 minute")) >= 0)
                            pmAddProfile(desclist[LOADAV].indom, 0, &inst); // enable all
                    }

	            if (resp->vset[LOADAV]->pmid != PM_ID_NULL) {
                        pmExtractValue(resp->vset[LOADAV]->valfmt,
                            &resp->vset[LOADAV]->vlist[2], desclist[LOADAV].type,
                            &la, PM_TYPE_FLOAT);
	 	        // loadAvg15=(int) (la.f * 100);
	 	        loadAvg15=(int)((la.f * 1000 + 5) / 10);   // to round off high
	            }
                    else 	// load average, not available
                        loadAvg15 = 0;

            	    exc_errlog(LOG_DEBUG, 0, 
                        "hpsystemsa: hpSysAvgJobs15: %d\n", loadAvg15);
	    	    *a = new snmpGauge(loadAvg15);
	    	    break;
	        }

	    pmFreeResult(resp);
	    break;

	case hpSysMaxProcs:
	    numpmid = sizeof(_hpSysMaxProcs) / sizeof(char *);
	    if ((sts = pmLookupName(numpmid, _hpSysMaxProcs, pmidlist)) < 0) {
		exc_errlog(LOG_ERR, 0,
            	    "hpsystemsa: pmLookupName of _hpSysMaxProcs:  %s\n", pmErrStr(sts));
	    }

	    if ((sts = pmFetch(numpmid, pmidlist, &resp)) < 0) {
        	exc_errlog(LOG_ERR, 0, 
                    "hpsystemsa: hpSysMaxProcs pmFetch: %s\n", pmErrStr(sts));
		return SNMP_ERR_noSuchName;
    	    }
	    maxProcs = firstval(resp, NPROCS, lval);
	    exc_errlog(LOG_DEBUG, 0, 
                "hpsystemsa: hpSysMaxProcs = %d\n", maxProcs);
	    pmFreeResult(resp);
	    *a = new asnInteger(maxProcs);
	    break;

	case hpSysFreeMem:
	    numpmid = sizeof(_hpSysFreeMem) / sizeof(char *);
	    if ((sts = pmLookupName(numpmid, _hpSysFreeMem, pmidlist)) < 0) {
		exc_errlog(LOG_ERR, 0,
            	    "hpsystemsa: pmLookupName of _hpSysFreeMem:  %s\n", pmErrStr(sts));
	    }

	    if ((sts = pmFetch(numpmid, pmidlist, &resp)) < 0) {
        	exc_errlog(LOG_ERR, 0, 
                    "hpsystemsa: hpSysFreeMem pmFetch: %s\n", pmErrStr(sts));
		return SNMP_ERR_noSuchName;
    	    }
	    freeMem = firstval(resp, FREEMEM, lval);
	    exc_errlog(LOG_DEBUG, 0, 
                "hpsystemsa: hpSysFreeMem = %d\n", freeMem);
	    pmFreeResult(resp);
	    *a = new snmpGauge(freeMem);
	    break;

	case hpSysPhysMem:
	    numpmid = sizeof(_hpSysPhysMem) / sizeof(char *);
	    if ((sts = pmLookupName(numpmid, _hpSysPhysMem, pmidlist)) < 0) {
		exc_errlog(LOG_ERR, 0,
            	    "hpsystemsa: pmLookupName of _hpSysPhysMem:  %s\n", pmErrStr(sts));
	    }

	    if ((sts = pmFetch(numpmid, pmidlist, &resp)) < 0) {
        	exc_errlog(LOG_ERR, 0, 
                    "hpsystemsa: hpSysPhysMem pmFetch: %s\n", pmErrStr(sts));
		return SNMP_ERR_noSuchName;
    	    }
	    physMem = firstval(resp, PHYSMEM, lval);
	    exc_errlog(LOG_DEBUG, 0, 
                "hpsystemsa: hpSysPhysMem = %d\n", physMem);
	    pmFreeResult(resp);
	    *a = new asnInteger(physMem);
	    break;

	case hpSysMaxUserMem:
	    numpmid = sizeof(_hpSysMaxUserMem) / sizeof(char *);
	    if ((sts = pmLookupName(numpmid, _hpSysMaxUserMem, pmidlist)) < 0) {
		exc_errlog(LOG_ERR, 0,
            	    "hpsystemsa: pmLookupName of _hpSysMaxUserMem:  %s\n", pmErrStr(sts));
	    }

	    if ((sts = pmFetch(numpmid, pmidlist, &resp)) < 0) {
        	exc_errlog(LOG_ERR, 0, 
                    "hpsystemsa: hpSysMaxUserMem pmFetch: %s\n", pmErrStr(sts));
		return SNMP_ERR_noSuchName;
    	    }
	    maxUMem = firstval(resp, USERMEM, lval);
	    exc_errlog(LOG_DEBUG, 0, 
                "hpsystemsa: hpSysMaxUserMem = %d\n", maxUMem);
	    pmFreeResult(resp);
	    *a = new snmpGauge(maxUMem);
	    break;

	case hpSysSwapConfig:
	    numpmid = sizeof(_hpSysSwapConfig) / sizeof(char *);
	    if ((sts = pmLookupName(numpmid, _hpSysSwapConfig, pmidlist)) < 0) {
		exc_errlog(LOG_ERR, 0,
            	    "hpsystemsa: pmLookupName of _hpSysSwapConfig:  %s\n", pmErrStr(sts));
	    }

	    if ((sts = pmFetch(numpmid, pmidlist, &resp)) < 0) {
        	exc_errlog(LOG_ERR, 0, 
                    "hpsystemsa: hpSysSwapConfig pmFetch: %s\n", pmErrStr(sts));
		return SNMP_ERR_noSuchName;
    	    }
	    maxSwap = firstval(resp, MAXSWAP, lval);
	    exc_errlog(LOG_DEBUG, 0, 
                "hpsystemsa: hpSysSwapConfig = %d\n", maxSwap);
	    pmFreeResult(resp);
	    *a = new asnInteger(maxSwap);
	    break;

	case hpSysEnabledSwap:
	    return SNMP_ERR_noSuchName;

	case hpSysFreeSwap:
	    numpmid = sizeof(_hpSysFreeSwap) / sizeof(char *);
	    if ((sts = pmLookupName(numpmid, _hpSysFreeSwap, pmidlist)) < 0) {
		exc_errlog(LOG_ERR, 0,
            	    "hpsystemsa: pmLookupName of _hpSysFreeSwap:  %s\n", pmErrStr(sts));
	    }

	    if ((sts = pmFetch(numpmid, pmidlist, &resp)) < 0) {
        	exc_errlog(LOG_ERR, 0, 
                    "hpsystemsa: hpSysFreeSwap pmFetch: %s\n", pmErrStr(sts));
		return SNMP_ERR_noSuchName;
    	    }
	    freeSwap = firstval(resp, FREESWAP, lval);
	    exc_errlog(LOG_DEBUG, 0, 
                "hpsystemsa: hpSysFreeSwap = %d\n", freeSwap);
	    pmFreeResult(resp);
	    *a = new snmpGauge(freeSwap);
	    break;

	case hpSysUserCPU:
	    updateCPUcounts();
	    if ( sum_time != 0 && cpu_prev != NULL) {
	        userCPU = (sum_time * (double) firstval(cpu_prev, CPU_USER, lval));
	    }
	    else
		userCPU = 0;
	    exc_errlog(LOG_DEBUG, 0, 
                "hpsystemsa: hpSysUserCPU = %d\n", userCPU);
	    *a = new snmpCounter(userCPU);
	    break;

	case hpSysSysCPU:
	    updateCPUcounts();
	    if ( sum_time != 0 && cpu_prev != NULL) {
	        sysCPU = (sum_time * 
			((double) firstval(cpu_prev, CPU_KERNEL, lval) +
                         (double) firstval(cpu_prev, CPU_SXBRK,  lval) +
                         (double) firstval(cpu_prev, CPU_INTR,   lval)));
	    }
	    else
		sysCPU = 0;
	    exc_errlog(LOG_DEBUG, 0, 
                "hpsystemsa: hpSysSysCPU = %d\n", sysCPU);
	    *a = new snmpCounter(sysCPU);
	    break;

	case hpSysIdleCPU:
	    updateCPUcounts();
	    if ( sum_time != 0 && cpu_prev != NULL) {
	        idleCPU = (sum_time * (double) firstval(cpu_prev, CPU_IDLE, lval));
	    }
	    else
		idleCPU = 0;
	    exc_errlog(LOG_DEBUG, 0, 
                "hpsystemsa: hpSysIdleCPU = %d\n", idleCPU);
	    *a = new snmpCounter(idleCPU);
	    break;

	case hpSysNiceCPU:
	    // *a = new snmpCounter(0);
	    return SNMP_ERR_noSuchName;
	    // break;

	default:
	    return SNMP_ERR_noSuchName;
    }
    return SNMP_ERR_noError;
}

int
hpSystemSubAgent::getNext(asnObjectIdentifier *o, asnObject **a, int *t)
{
    return simpleGetNext(o, a, t, hpSysNiceCPU);
}

int
hpSystemSubAgent::set(asnObjectIdentifier *, asnObject **, int *)
{
	// No sets in this MIB group
	return SNMP_ERR_noSuchName;
}

void
updateCPUcounts()
{
    int i;
    long now;
    static long         last_update_time = 0;
    static int		valid_cpu_resp = 0;

    // Only update once per packet
    extern unsigned int packetCounter;
    static unsigned int validCounter = 0;

    if (validCounter == packetCounter)
        return;
    validCounter = packetCounter;

    now = time((time_t *)0);
    if (((now - last_update_time) < 2) ) // Refresh every 2 secs max.
        return;

    last_update_time = now;

    if ( cpu_prev != NULL)
        pmFreeResult(cpu_prev);

    if ( valid_cpu_resp == 1 )
        cpu_prev = cpu_resp;

    numpmid = sizeof(_hpSysCPUCounts) / sizeof(char *);
    if ((sts = pmLookupName(numpmid, _hpSysCPUCounts, pmidlist)) < 0) {
	exc_errlog(LOG_ERR, 0,
       	    "hpsystemsa: pmLookupName of _hpSysCPUCounts:  %s\n", pmErrStr(sts));
    }

    if ((sts = pmFetch(numpmid, pmidlist, &cpu_resp)) < 0) {
        exc_errlog(LOG_ERR, 0, 
            "hpsystemsa: hpSysCPUCounts pmFetch: %s\n", pmErrStr(sts));
        valid_cpu_resp = 0;
    return;
    }
    valid_cpu_resp = 1;

    if ( cpu_prev == NULL ) {
	sum_time = 0.0;
    }
    else {
        // multiplier for percent cpu usage
        for (sum_time=0.0, i = CPU_USER; i <= CPU_WAIT; i++) {
            long        temp;
            temp = (double)ulong_diff(firstval(cpu_resp, i, lval), 
				      firstval(cpu_prev, i, lval));
            cpu_prev->vset[i]->vlist[0].value.lval = temp;
            sum_time += temp;
        }
	if ( sum_time > 0.0 )
            sum_time = 100.0 / sum_time;
	else
	    sum_time = 0.0;
    }
}

#define TWOx32 (double) 4.294967296e9

/* return ulong result of difference between two ulong */
static double
ulong_diff(ulong_t this_l, ulong_t prev)
{
    if (this_l < prev)
        /* wrapped */
        return TWOx32 - prev + this_l;
    else
        return (double)this_l - prev;
}


