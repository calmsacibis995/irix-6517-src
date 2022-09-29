/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	SGI sub-agent
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
// #include <sys/pda.h>
#include <sys/sysmp.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <invent.h>
#include <stdio.h>
#include <syslog.h>
#include <bstring.h>
#include <string.h>

#include "oid.h"
#include "asn1.h"
#include "snmp.h"
#include "packet.h"
#include "subagent.h"
#include "sat.h"
#include "agent.h"
#include "table.h"
#include "sgisa.h"

#ifndef PDAF_MASTER	/* XXX! */
/*
 * sysmp(MP_STAT) structure
 */
struct pda_stat {
        int             p_cpuid;        /* processor ID */
        int             p_flags;        /* various flags */
};

/* values for p_flags */
#define PDAF_MASTER     0x0001
#define PDAF_CLOCK      0x0002
#define PDAF_ENABLED    0x0004          /* processor allowed to sched procs */
#define PDAF_FASTCLOCK  0x0008          /* processor handles fastimer */
#define PDAF_ISOLATED   0x0010          /* processor is isolated */
#define PDAF_BROADCAST_OFF      0x0020  /* broadcast intr is not received */
#define PDAF_NONPREEMPTIVE      0x0040  /* processor is not preemptive */
#endif /* !PDAF_MASTER */

/************************************************************************
* The following code was hacked out of os/machdep.c and should be updated
* accordingly.
*
*/

/*
 * coprocessor revision identifiers
 */
union rev_id {
        unsigned long	ri_ulong;
	struct {
// Work around a CC bug, _MIPSEB is not defined
#if defined(_MIPSEB) || defined(__sgi)
		unsigned int	Ri_fill:16,
				Ri_imp:8,		/* implementation id */
				Ri_majrev:4,		/* major revision */
				Ri_minrev:4;		/* minor revision */
#endif
#ifdef _MIPSEL
		unsigned int	Ri_minrev:4,		/* minor revision */
				Ri_majrev:4,		/* major revision */
				Ri_imp:8,		/* implementation id */
				Ri_fill:16;
#endif
	} Ri;
};
#define ri_imp		Ri.Ri_imp
#define ri_majrev	Ri.Ri_majrev
#define ri_minrev	Ri.Ri_minrev

// const subID products = 1;
    // See sgisa.h
// const subID mib = 2;
// const subID experimental = 3;
    // const subID hardware = 1;
	const subID processorTable = 1;
	    const subID processorIndex = 1;
	    const subID processorType = 2;
	    const subID processorFrequency = 3;
	    const subID cpuType = 4;
	    const subID cpuMajorRevision = 5;
	    const subID cpuMinorRevision = 6;
	    const subID fpuType = 7;
	    const subID fpuMajorRevision = 8;
	    const subID fpuMinorRevision = 9;
	    const subID processorEnabled = 10;
	    const subID processorMaster = 11;
	    const subID processorClock = 12;
	    const subID processorFastClock = 13;
	    const subID processorIsolated = 14;
	    const subID processorBroadcast = 15;
	    const subID processorPreemptive = 16;
#if 0
	    const subID processorTimeIdle = 17;
	    const subID processorTimeUser = 18;
	    const subID processorTimeKernel = 19;
	    const subID processorTimeWait = 20;
	    const subID processorTimeSxbrk = 21;
	    const subID processorTimeIntr = 22;
#endif
	const subID graphicsTable = 2;
	    const subID graphicsIndex = 1;
	    const subID graphicsType = 2;
	    const subID graphicsQual = 3;
// const subID netvis = 4;
    const subID nvStationType = 1;

// const int DataStation = 1;
// const int DisplayStation = 2;
// const int DisplayAndDataStation = 3;
const int none = 4;

sgiHWSubAgent *sgihwsa;

sgiHWSubAgent::sgiHWSubAgent(void)
{
    // Store address for table update routines
    sgihwsa = this;

    // Export subtree
    subtree = "1.3.6.1.4.1.59.3.1";
    int rc = export("sgi hardware", &subtree);
    if (rc != SNMP_ERR_genErr) {
	log(LOG_ERR, 0, "error exporting SGI hardware subagent");
	return;
    }

    // Define tables
    processortable = table(processorTable, 1, updateProcessorTable);
    graphicstable = table(graphicsTable, 1, 0);

    // Find number of processors
    processors = sysmp(MP_NPROCS);

    // Read hardware inventory
    systemType = 0;
    readInventory();

    // Create system description
    createSystemDesc();
}

sgiHWSubAgent::~sgiHWSubAgent(void)
{
    unexport(&subtree);
    delete processortable;
    delete systemType;
}

void
sgiHWSubAgent::readInventory(void)
{
    subID *subid;
    unsigned int len;
    asnInteger i;
    asnObject *a;
    unsigned int grid = 0;
    oid prodoi = "1.3.6.1.4.1.59.1.1";
    oid proi = "1.3.6.1.4.1.59.3.1.1.1.1.1";
    oid groi = "1.3.6.1.4.1.59.3.1.2.1.1.1";

    setinvent();
    for (inventory_t *inv = getinvent(); inv != 0; inv = getinvent()) {
	switch (inv->inv_class) {
	    case INV_PROCESSOR:
		switch (inv->inv_type) {
		    case INV_CPUBOARD:
			{
			    minor_t cpuid = inv->inv_unit;
			    int prid = 0;
			    oid *noid;

			    if (systemType == 0) {
			        prodoi.getValue(&subid, &len);
				switch (inv->inv_state) {
				    case INV_IP19BOARD:
					subid[len - 1] = Challenge
								+ processors;
					break;
				    case INV_IP20BOARD:
					subid[len - 1] = IRIS_Indigo_R4000;
					break;
				    case INV_IP21BOARD:
				    case INV_IP25BOARD:
					subid[len - 1] = PowerChallenge
								+ processors;
					break;
				    case INV_IP22BOARD:
					//Assume it's an Indy. Then check to see if it is
					//an Indigo2 by looking for more than 1 integral
					//scsi controller.
					//XXX as Indigo2 and Indy hardware change, this test
					//will have to change as well.
					subid[len -1] = IRIS_Indy;
					inventory_t tinv;
					bcopy(inv, &tinv, sizeof tinv);
					inv = &tinv;
					setinvent(); // reset in case I passed it up
					for (inventory_t *pinv = getinvent(); pinv != 0; pinv = getinvent()) {
					    if (pinv->inv_class == INV_DISK
						&& pinv->inv_type == INV_SCSICONTROL
						    && pinv->inv_controller > 0) {
						subid[len - 1] = IRIS_Indigo2;
						break;
					    }
					}
					setinvent();	// reset in case further hardware had come before		
					break;
				    case INV_IP26BOARD:
					subid[len - 1] = IRIS_Indigo2_R8000;
				    case INV_IP28BOARD:
					subid[len - 1] = IRIS_Indigo2_R10000;
					break;
				}
				noid = new oid(prodoi);
				systemType = new asnObjectIdentifier(noid);
			    }

			    proi.getValue(&subid, &len);
			    
			    for ( ; ; ) {
				if (cpuid == (char) -1) {
				    // All processors are the same
				    if (prid == processors)
					break;
				    subid[len - 1] = ++prid;
				} else {
				    // Different processors
				    if (prid == 2)
					break;
				    subid[len - 1] = (subID) (cpuid + prid);
				    prid++;
				}

				subid[len - 2] = processorIndex;
				i = subid[len - 1];
				a = &i;
				processortable->set(&proi, &a);

				subid[len - 2] = processorType;
				i = (int) inv->inv_state;
				processortable->set(&proi, &a);

				subid[len - 2] = processorFrequency;
				i = (int) inv->inv_controller;
				processortable->set(&proi, &a);
			    }
			}
			break;

		    case INV_CPUCHIP:
			{
			    major_t cpuid = inv->inv_controller; 
			    int prid = 0;
			    union rev_id ri;

			    ri.ri_ulong = inv->inv_state;
			    if (ri.ri_imp == 0) {
				ri.ri_majrev = 1;
				ri.ri_minrev = 5;
			    }

			    proi.getValue(&subid, &len);
			    a = &i;

			    for ( ; prid < processors; prid++) {
				subid[len - 1] = (subID) (cpuid + prid + 1);

				subid[len - 2] = cpuType;
				i = ri.ri_imp + 1;
				processortable->set(&proi, &a);

				subid[len - 2] = cpuMajorRevision;
				i = ri.ri_majrev;
				processortable->set(&proi, &a);

				subid[len - 2] = cpuMinorRevision;
				i = ri.ri_minrev;
				processortable->set(&proi, &a);

				if (cpuid != 0)
				    break;
			    }
			}
			break;

		    case INV_FPUCHIP:
			{
			    major_t cpuid = inv->inv_controller; 
			    int prid = 0;
			    union rev_id ri;

			    ri.ri_ulong = inv->inv_state;

			    proi.getValue(&subid, &len);
			    a = &i;

			    for ( ; prid < processors; prid++) {
				subid[len - 1] = (subID) (cpuid + prid + 1);

				subid[len - 2] = fpuType;
				i = ri.ri_imp;
				processortable->set(&proi, &a);

				subid[len - 2] = fpuMajorRevision;
				i = ri.ri_majrev;
				processortable->set(&proi, &a);

				subid[len - 2] = fpuMinorRevision;
				i = ri.ri_minrev;
				processortable->set(&proi, &a);

				if (cpuid != 0)
				    break;
			    }
			}
			break;
		}
		break;

	    case INV_GRAPHICS:
		groi.getValue(&subid, &len);
		a = &i;
		subid[len - 1] = ++grid;

		subid[len - 2] = graphicsIndex;
		i = grid;
		graphicstable->set(&groi, &a);

		subid[len - 2] = graphicsType;
		i = inv->inv_type;
		graphicstable->set(&groi, &a);

		subid[len - 2] = graphicsQual;
		i = (int) inv->inv_state;
		graphicstable->set(&groi, &a);
		break;
	}
    }
}

void
sgiHWSubAgent::createSystemDesc(void)
{
    char *cpudesc = "";
    char *grdesc = "";
    char cpudescbuf[64];
    subID *subid;
    unsigned int len;
    oid *oi = systemType->getValue();
    oi->getValue(&subid, &len);
    subID sys = subid[len - 1];
    switch (sys) {
	case IRIS_Indigo_R4000:
	    cpudesc = "IRIS Indigo R4000 ";
	    break;
	case IRIS_Indigo2:
	    cpudesc = "IRIS Indigo2 ";
	    break;
	case IRIS_Indigo2_R8000:
	    cpudesc = "IRIS Indigo2 R8000 ";
	    break;
	case IRIS_Indy:
	    cpudesc = "IRIS Indy ";
	    break;
	case Challenge:
	    cpudesc = "Challenge ";
	    break;
	case PowerChallenge:
	    cpudesc = "Power Challenge ";
	    break;
	default:
	    if (sys - Challenge < PowerChallenge - Challenge) {
		sprintf(cpudescbuf, "Challenge/%d ", processors);
		cpudesc = cpudescbuf;
		sys = Challenge;
	    } else if (sys - PowerChallenge < NextChallenge - PowerChallenge) {
		sprintf(cpudescbuf, "Power Challenge/%d ", processors);
		cpudesc = cpudescbuf;
		sys = PowerChallenge;
	    }
    }

    asnInteger *i;
    oid groi = "1.3.6.1.4.1.59.3.1.2.1.2.1";
    if (graphicstable->get(&groi, (asnObject **) &i) != SNMP_ERR_noError) {
	switch (sys) {
	    case IRIS_Indigo_R4000:
		grdesc = "Server";
		break;
	    case IRIS_Indigo2:
		cpudesc = "Challenge ";
		grdesc = "M";
		break;
	    case Challenge:
	    case PowerChallenge:
		grdesc = "";
		break;
	    default:
		grdesc = "S";
		break;
	}
    } else {
	int rslt ;
	rslt = (int) i->getValue();
	switch (rslt) {
	    case INV_GR1BOARD:
		{
		    delete i;  // will re-use the pointer here
		    i = 0;
		    groi.getValue(&subid, &len);
		    subid[len - 2] = graphicsQual;
		    int rc = graphicstable->get(&groi, (asnObject **) &i);
		    if (rc == SNMP_ERR_noError) {
			if (i->getValue() & INV_GR1TURBO)
			    grdesc = "TG";
			else if (i->getValue() & INV_GR1BIT24
				 && i->getValue() & INV_GR1ZBUF24)
			    grdesc = "G";
		    }
		}
		break;
	    case INV_GR2:
		{
		    delete i;  // will re-use the pointer here
		    i = 0;
		    groi.getValue(&subid, &len);
		    subid[len - 2] = graphicsQual;
		    int rc = graphicstable->get(&groi, (asnObject **) &i);
		    if (rc == SNMP_ERR_noError) {
			switch (i->getValue()) {
			    case INV_GU1_EXTREME:
				grdesc = "Extreme";
				break;
			    case INV_GR3_ELAN:
				grdesc = "Elan";
				break;
			    case INV_GR3_XSM:
				grdesc = "XSM";
				break;
			    case INV_GR2_XZ:
				grdesc = "XZ";
				break;
			    case INV_GR2_ELAN:
				grdesc = "Elan";
				break;
			    case INV_GR2_XSM:
				grdesc = "XSM";
				break;
			    case INV_GR2_XS24Z:
				grdesc = "XS24Z";
				break;
			    case INV_GR2_XS24:
				grdesc = "XS24";
				break;
			    case INV_GR2_XSZ:
				grdesc = "XSZ";
				break;
			    case INV_GR2_XS:
				grdesc = "XS";
				break;
			}
		    }
		}
		break;
	    case INV_GRODEV:
		grdesc = "G";
		break;
	    case INV_VGX:
		grdesc = "VGX";
		break;
	    case INV_VGXT:
		switch (sys) {
		    case Challenge:
		    case PowerChallenge:
			sprintf(cpudescbuf, "Onyx/%d ", processors);
			cpudesc = cpudescbuf;
			grdesc = "VTX";
			break;
		    default:
			grdesc = "VGXT";
		}
		break;
	    case INV_RE:
		switch (sys) {
		    case Challenge:
		    case PowerChallenge:
			sprintf(cpudescbuf, "Onyx/%d", processors);
			cpudesc = cpudescbuf;
			grdesc = " RealityEngine2";
			break;
		    default:
			grdesc = " RealityEngine";
		}
		break;
	}
	delete i;
    }

    struct utsname uts;
    if (uname(&uts) < 0)
	bzero(&uts, sizeof uts);
    char *sd = new char[strlen(cpudesc) + strlen(grdesc)
			+ strlen(uts.sysname) + strlen(uts.release) + 32];
    sprintf(sd, "Silicon Graphics %s%s running %s %s",
		cpudesc, grdesc, uts.sysname, uts.release);
    systemDesc = new asnOctetString(sd);
}

int
sgiHWSubAgent::get(asnObjectIdentifier *o, asnObject **a, int *)
{
    // Pull out the subID array and length
    subID *subid;
    unsigned int len;
    oid *oi = o->getValue();
    oi->getValue(&subid, &len);

    // Check that the subID array is of valid size
    if (len == sublen)
	return SNMP_ERR_noSuchName;

    // Handle the tables separately
    switch (subid[sublen]) {
	case processorTable:
	    if (subid[sublen + 1] != 1
		|| subid[sublen + 2] == 0
		|| subid[sublen + 2] > processorPreemptive)
		return SNMP_ERR_noSuchName;
	    return processortable->get(oi, a);

	case graphicsTable:
	    if (subid[sublen + 1] != 1
		|| subid[sublen + 2] == 0
		|| subid[sublen + 2] > graphicsQual)
		return SNMP_ERR_noSuchName;
	    return graphicstable->get(oi, a);
    }

    // Check that the subID array is of valid size
    if (len != sublen + 2 || subid[sublen + 1] != 0)
	return SNMP_ERR_noSuchName;

    // Switch on the subID and assign the value to *a
    switch (subid[sublen]) {
	// case nProcessors:
	//     *a = new asnInteger(processors);
	//     break;
	default:
	    return SNMP_ERR_noSuchName;
    }
}

int
sgiHWSubAgent::getNext(asnObjectIdentifier *o, asnObject **a, int *t)
{
    return simpleGetNext(o, a, t, graphicsTable);
}

int
sgiHWSubAgent::set(asnObjectIdentifier *o, asnObject **a, int *)
{
    // Pull out the subID array and length
    subID *subid;
    unsigned int len;
    oid *oi = o->getValue();
    oi->getValue(&subid, &len);

    // Check that the subID array is of valid size
    if (len == sublen)
	return SNMP_ERR_noSuchName;

    // Handle the CPU table separately
    if (subid[sublen] == processorTable) {
	if (subid[sublen + 1] != 1)
	    return SNMP_ERR_noSuchName;

	switch (subid[sublen + 2]) {
	    case 1:
		if ((*a) != 0 && (*a)->getTag() != TAG_INTEGER)
		    return SNMP_ERR_badValue;
		return processortable->set(oi, a);
	}
	return SNMP_ERR_noSuchName;
    }

    // Check that the subID array is of valid size
    if (len != sublen + 2 || subid[sublen + 1] != 0)
	return SNMP_ERR_noSuchName;

    // Switch on the subID and assign *a to the value
    switch (subid[sublen]) {
	default:
	    return SNMP_ERR_noSuchName;
    }

    return SNMP_ERR_noError;
}

void
updateProcessorTable(void)
{
    //struct sysinfo s;
    subID *subid;
    unsigned int len;
    oid oi = "1.3.6.1.4.1.59.3.1.1.1.1.1";
    oi.getValue(&subid, &len);

    struct pda_stat *pda = new struct pda_stat[sgihwsa->processors];
    sysmp(MP_STAT, pda, sgihwsa->processors * sizeof(struct pda_stat));

    asnInteger i;
    asnObject *a = &i;
    for (int p = 0; p < sgihwsa->processors; p++) {
	subid[len - 1] = p + 1;

	subid[len - 2] = processorEnabled;
	i = (pda[p].p_flags & PDAF_ENABLED) ? 1 : 0;
	sgihwsa->processortable->set(&oi, &a);

	subid[len - 2] = processorMaster;
	i = (pda[p].p_flags & PDAF_MASTER) ? 1 : 0;
	sgihwsa->processortable->set(&oi, &a);

	subid[len - 2] = processorClock;
	i = (pda[p].p_flags & PDAF_CLOCK) ? 1 : 0;
	sgihwsa->processortable->set(&oi, &a);

	subid[len - 2] = processorFastClock;
	i = (pda[p].p_flags & PDAF_FASTCLOCK) ? 1 : 0;
	sgihwsa->processortable->set(&oi, &a);

	subid[len - 2] = processorIsolated;
	i = (pda[p].p_flags & PDAF_ISOLATED) ? 1 : 0;
	sgihwsa->processortable->set(&oi, &a);

	subid[len - 2] = processorBroadcast;
	i = (pda[p].p_flags & PDAF_BROADCAST_OFF) ? 0 : 1;
	sgihwsa->processortable->set(&oi, &a);

	subid[len - 2] = processorPreemptive;
	i = (pda[p].p_flags & PDAF_NONPREEMPTIVE) ? 0 : 1;
	sgihwsa->processortable->set(&oi, &a);

#if 0
	// Read sysinfo for this processor
	sysmp(MP_SAGET1, MPSA_SINFO, (char *) &s, sizeof s, p);

	subid[len - 2] = processorTimeIdle;
	i = s.cpu[CPU_IDLE];
	sgihwsa->processortable->set(&oi, &a);

	subid[len - 2] = processorTimeUser;
	i = s.cpu[CPU_USER];
	sgihwsa->processortable->set(&oi, &a);

	subid[len - 2] = processorTimeKernel;
	i = s.cpu[CPU_KERNEL];
	sgihwsa->processortable->set(&oi, &a);

	subid[len - 2] = processorTimeWait;
	i = s.cpu[CPU_WAIT];
	sgihwsa->processortable->set(&oi, &a);

	subid[len - 2] = processorTimeSxbrk;
	i = s.cpu[CPU_SXBRK];
	sgihwsa->processortable->set(&oi, &a);

	subid[len - 2] = processorTimeIntr;
	i = s.cpu[CPU_INTR];
	sgihwsa->processortable->set(&oi, &a);
#endif
    }
}

/*
 * NetVis subagent
 */
sgiNVSubAgent::sgiNVSubAgent(void)
{
    // Export subtree
    subtree = "1.3.6.1.4.1.59.4";
    int rc = export("sgi hardware", &subtree);
    if (rc != SNMP_ERR_genErr) {
	log(LOG_ERR, 0, "error exporting SGI hardware subagent");
	return;
    }
}

sgiNVSubAgent::~sgiNVSubAgent(void)
{
    unexport(&subtree);
}

int
sgiNVSubAgent::get(asnObjectIdentifier *o, asnObject **a, int *)
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
	case nvStationType:
	    {
		int i = access("/usr/etc/rpc.snoopd", X_OK) == 0 ? 1 : 0;
		i += access("/usr/sbin/netlook" , X_OK) == 0 ? 2 : 0;
		if (i == 0)
		    *a = new asnInteger(none);
		else
		    *a = new asnInteger(i);
	    }
	    return SNMP_ERR_noError;
    }

    return SNMP_ERR_noSuchName;
}

int
sgiNVSubAgent::getNext(asnObjectIdentifier *o, asnObject **a, int *t)
{
    return simpleGetNext(o, a, t, nvStationType);
}

int
sgiNVSubAgent::set(asnObjectIdentifier *, asnObject **, int *)
{
    return SNMP_ERR_noSuchName;
}
