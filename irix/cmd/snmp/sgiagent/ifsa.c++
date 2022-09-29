/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Interfaces sub-agent
 *
 *	$Revision: 1.5 $
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
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/soioctl.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <bstring.h>
#include <stdio.h>
#include <alloca.h>

// this prototype no longer needed in sherwood
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
#include "ifsa.h"
#include "systemsa.h"

extern systemSubAgent	*syssa;

const subID ifNumber = 1;
const subID ifTable = 2;
    const subID ifIndex = 1;
    const subID ifDesc = 2;
    const subID ifType = 3;
    const subID ifMtu = 4;
    const subID ifSpeed = 5;
    const subID ifPhysAddr = 6;
    const subID ifAdminStatus = 7;
    const subID ifOperStatus = 8;
    const subID ifLastChange = 9;
    const subID ifInOctets = 10;
    const subID ifInUcast = 11;
    const subID ifInNUcast = 12;
    const subID ifInDiscards = 13;
    const subID ifInErrors = 14;
    const subID ifInUnknown = 15;
    const subID ifOutOctets = 16;
    const subID ifOutUcast = 17;
    const subID ifOutNUcast = 18;
    const subID ifOutDiscards = 19;
    const subID ifOutErrors = 20;
    const subID ifOutQLen = 21;
    const subID ifSpecific = 22;
    const subID ifName = 23;		// Internal

const unsigned int up = 1;
const unsigned int down = 2;
// const unsigned int testing = 3;


ifSubAgent *ifsa;

ifSubAgent::ifSubAgent(void)
{
    // Store address for table update routines
    ifsa = this;

    // Export subtree
    subtree = "1.3.6.1.2.1.2";
    int rc = export("interfaces", &subtree);
    if (rc != SNMP_ERR_genErr) {
	log(LOG_ERR, 0, "error exporting interfaces subagent");
	return;
    }

    // Define tables
    iftable = table(ifTable, 1, updateIfTable);
}

ifSubAgent::~ifSubAgent(void)
{
    unexport(&subtree);
    delete iftable;
}

int
ifSubAgent::get(asnObjectIdentifier *o, asnObject **a, int *)
{
    // Pull out the subID array and length
    subID *subid;
    unsigned int len;
    oid *oi = o->getValue();
    oi->getValue(&subid, &len);

    // Check that the subID array is of valid size
    if (len == sublen)
	return SNMP_ERR_noSuchName;

    // Handle the if table separately
    if (subid[sublen] == ifTable) {
	if (len != sublen + 4
	    || subid[sublen + 1] != 1
	    || subid[sublen + 2] == 0
	    || subid[sublen + 2] > ifSpecific)
	    return SNMP_ERR_noSuchName;

	updateIfTable();
	return iftable->get(oi, a);
    }

    // Check that the subID array is of valid size
    if (len != sublen + 2 || subid[sublen + 1] != 0)
	return SNMP_ERR_noSuchName;

    // Switch on the subID and assign the value to *a
    switch (subid[sublen]) {
	case ifNumber:
	    updateIfTable();
	    *a = new asnInteger(ifnumber);
	    break;
	default:
	    return SNMP_ERR_noSuchName;
    }

    return SNMP_ERR_noError;
}

int
ifSubAgent::getNext(asnObjectIdentifier *o, asnObject **a, int *t)
{
    return simpleGetNext(o, a, t, ifTable);
}

int
ifSubAgent::set(asnObjectIdentifier *o, asnObject **a, int *)
{
    // Pull out the subID array and length
    subID *subid;
    unsigned int len;
    oid *oi = o->getValue();
    oi->getValue(&subid, &len);

    // Only ifAdminStatus is writable
    if (len != sublen + 4
	|| subid[sublen] != ifTable
	|| subid[sublen + 1] != 1
	|| subid[sublen + 2] != ifAdminStatus)
	return SNMP_ERR_noSuchName;

    if ((*a)->getTag() != TAG_INTEGER)
	return SNMP_ERR_badValue;

    int i = ((asnInteger *) *a)->getValue();
    switch (i) {
	case up:
	    i = IFF_UP;
	    break;
	case down:
	    i = 0;
	    break;
	default:
	    return SNMP_ERR_badValue;
    }

    updateIfTable();

    // Get the ifName
    asnOctetString *ao;
    subID *sub;
    unsigned int l;
    oid toi = "1.3.6.1.2.1.2.2.1.23.1";
    toi.getValue(&sub, &l);
    sub[l - 1] = subid[len - 1];
    int rc = iftable->get(&toi, (asnObject **) &ao);
    if (rc != SNMP_ERR_noError)
	return rc;

    // Open a socket and read flags
    struct ifreq ifr;
    bzero(&ifr, sizeof ifr);
    strncpy(ifr.ifr_name, ao->getValue(), ao->getLength());
    int s = socket(PF_INET, SOCK_DGRAM, 0);
    if (s < 0)
	return SNMP_ERR_genErr;
    if (ioctl(s, SIOCGIFFLAGS, &ifr) < 0) {
	close(s);
	return SNMP_ERR_genErr;
    }

    // Set to new value
    ifr.ifr_flags &= ~IFF_UP;
    ifr.ifr_flags |= i;
    if (ioctl(s, SIOCSIFFLAGS, &ifr) < 0) {
	close(s);
	return SNMP_ERR_genErr;
    }
    close(s);
    return iftable->set(oi, a);
}

asnOctetString *
ifSubAgent::getIfName(unsigned int i)
{
    subID *subid;
    unsigned int len;
    asnOctetString *o;
    oid oi = "1.3.6.1.2.1.2.2.1.23.1";
    oi.getValue(&subid, &len);

    updateIfTable();

    subid[len - 1] = (subID) i;
    if (iftable->get(&oi, (asnObject **) &o) != SNMP_ERR_noError)
	return 0;
    return o;
}

void
updateIfTable(void)
{
    // Only update once per packet
    extern unsigned int packetCounter;
    static unsigned int validCounter = 0;
    if (validCounter == packetCounter)
	return;
    validCounter = packetCounter;

    asnObject *a, *sysUp;
    asnInteger i;
    asnLong l;
    asnOctetString o;
    snmpCounter c;
    snmpGauge g;
    subID *subid;
    unsigned int len;
    int rc, good;
    time_t now, start; // current time and agent start time in sse
    asnObjectIdentifier sysUpoid = "1.3.6.1.2.1.1.3.0";
    oid oi = "1.3.6.1.2.1.2.2.1.1.1";
    int mib[6];
    size_t needed;
    char *buf, *next, *lim;
    struct if_msghdr *ifm;
    struct sockaddr_dl *sdl;

    oi.getValue(&subid, &len);

    mib[0] = CTL_NET;
    mib[1] = PF_ROUTE;
    mib[2] = 0;
    mib[3] = 0;
    mib[4] = NET_RT_IFLIST;
    mib[5] = 0;

    if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0) {
	perror("sysctl");
	return;
    }
    buf = (char *)alloca(needed);
    if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0) {
	perror("sysctl");
	return;
    }

    ifsa->ifnumber = 0;
    lim = buf + needed;
    for (next = buf; next < lim; next += ifm->ifm_msglen) {
	ifm = (struct if_msghdr *)next;
	if (ifm->ifm_type != RTM_IFINFO) {
	    continue;
	}
	ifsa->ifnumber++;
	subid[len - 2] = ifIndex;
	subid[len - 1] = ifm->ifm_index;
	i = (unsigned int)ifm->ifm_index;
	a = &i;
	ifsa->iftable->set(&oi, &a);

	// Store name for internal use
	subid[len - 2] = ifName;
	sdl = (struct sockaddr_dl *)(ifm + 1);
	o.setValue(sdl->sdl_data, sdl->sdl_nlen);
	a = &o;
	ifsa->iftable->set(&oi, &a);

	subid[len - 2] = ifDesc;
	switch (sdl->sdl_data[0]) {
	    case 'e':
		switch (sdl->sdl_data[1]) {
		    case 'c':
			o = "Silicon Graphics ec Ethernet controller";
			break;
		    case 'n':
			o = "Silicon Graphics enp Ethernet controller";
			break;
		    case 't':
			o = "Silicon Graphics et Ethernet controller";
			break;
		    case 'p':
			o = "Silicon Graphics ep Ethernet controller";
			break;
		}
		break;
	    case 'f':
		o = "Silicon Graphics fxp Ethernet controller";
		break;
	    case 'i':
		o = "Silicon Graphics ipg FDDI controller";
		break;
	    case 'l':
		o = "Silicon Graphics lo Loopback interface";
		break;
	    case 's':
		o = "Silicon Graphics sl Serial Line IP interface";
		break;
	    case 'x':
		o = "Silicon Graphics xpi FDDI controller";
		break;
	    default:
		o.setValue(sdl->sdl_data, sdl->sdl_nlen);
		break;
	}
	ifsa->iftable->set(&oi, &a);

	subid[len - 2] = ifType;
	i = (unsigned int)ifm->ifm_data.ifi_type;
	a = &i;
	ifsa->iftable->set(&oi, &a);

	subid[len - 2] = ifMtu;
	i = ifm->ifm_data.ifi_mtu;
	ifsa->iftable->set(&oi, &a);

	subid[len - 2] = ifSpeed;
	snmpGauge g;
	switch (ifm->ifm_data.ifi_type) {
	    case IFT_ETHER:
		g = 10000000;
		break;
	    case IFT_FDDI:
		g = 100000000;
		break;
	    case IFT_LOOP:
		g = 200000000;
		break;
	    case IFT_SLIP:
		// XXX - From baudrate;
		g = 1024;
		break;
	    default:
		g = 0;
	}
	a = &g;
	ifsa->iftable->set(&oi, &a);

	// XXX - Specific is always 0.0
	subid[len - 2] = ifSpecific;
	asnObjectIdentifier ao("0.0");
	a = &ao;
	ifsa->iftable->set(&oi, &a);

	subid[len - 2] = ifPhysAddr;
	if (ifm->ifm_data.ifi_type != IFT_ETHER &&
	  ifm->ifm_data.ifi_type != IFT_FDDI)
	    o = 0;
	else {
	    o.setValue(sdl->sdl_data + sdl->sdl_nlen, sdl->sdl_alen);
	}
	a = &o;
	ifsa->iftable->set(&oi, &a);

	subid[len - 2] = ifOperStatus;
	i = (ifm->ifm_flags & IFF_UP) ? up : down;
	a = &i;
	ifsa->iftable->set(&oi, &a);

	// Start ifAdminStatus equal to interface status
	subid[len - 2] = ifAdminStatus;
	rc = ifsa->iftable->get(&oi, &a);
	if (rc == SNMP_ERR_noSuchName) {
	    a = &i;
	    ifsa->iftable->set(&oi, &a);
	} else if (rc == SNMP_ERR_noError)
	    delete a;

	// To show ifLastChange, the agent must know when it started and
	// compare that to its sysUpTime to the interface change.  Therefore,
	// the following calculation must follow.  Note that the ifnet
	// lastchange is kept in seconds since epoch where snmpTimeTicks
	// is kept in hundredths of seconds.
	subid[len - 2] = ifLastChange;
	snmpTimeticks t;
	syssa->get(&sysUpoid, &sysUp, &good);
	now = time(0); 
	start = now - ((snmpTimeticks *) sysUp)->getValue() / 100;
	if (ifm->ifm_data.ifi_lastchange <= start) // before the agent started?
		t = (int) 0;
	else
		t = (int) ((ifm->ifm_data.ifi_lastchange - start) * 100);
	delete (snmpTimeticks *) sysUp;
	a = &t;
	ifsa->iftable->set(&oi, &a);

	subid[len - 2] = ifInOctets;
	c = (unsigned int) ifm->ifm_data.ifi_ibytes;
	a = &c;
	ifsa->iftable->set(&oi, &a);

	subid[len - 2] = ifInUcast;
	c = (unsigned)(ifm->ifm_data.ifi_ipackets - ifm->ifm_data.ifi_imcasts);
	ifsa->iftable->set(&oi, &a);

	subid[len - 2] = ifInNUcast;
	c = (unsigned int) ifm->ifm_data.ifi_imcasts;
	ifsa->iftable->set(&oi, &a);

	subid[len - 2] = ifInDiscards;
	c = (unsigned int) ifm->ifm_data.ifi_iqdrops;
	ifsa->iftable->set(&oi, &a);

	subid[len - 2] = ifInErrors;
	c = (unsigned int) ifm->ifm_data.ifi_ierrors;
	ifsa->iftable->set(&oi, &a);

	subid[len - 2] = ifInUnknown;
	c = (unsigned int) ifm->ifm_data.ifi_noproto;
	ifsa->iftable->set(&oi, &a);

	subid[len - 2] = ifOutOctets;
	c = (unsigned int) ifm->ifm_data.ifi_obytes;
	ifsa->iftable->set(&oi, &a);

	subid[len - 2] = ifOutUcast;
	c = (unsigned)(ifm->ifm_data.ifi_opackets - ifm->ifm_data.ifi_omcasts);
	ifsa->iftable->set(&oi, &a);

	subid[len - 2] = ifOutNUcast;
	c = (unsigned int) ifm->ifm_data.ifi_omcasts;
	ifsa->iftable->set(&oi, &a);

	subid[len - 2] = ifOutDiscards;
	c = (unsigned int) ifm->ifm_data.ifi_odrops;
	ifsa->iftable->set(&oi, &a);

	subid[len - 2] = ifOutErrors;
	c = (unsigned int) ifm->ifm_data.ifi_oerrors;
	ifsa->iftable->set(&oi, &a);

	subid[len - 2] = ifOutQLen;
	// Can't figure this out from sysctl.
	//g = (unsigned int) ifm->ifm_data.if_snd.ifq_len;
	g = 0;
	a = &g;
	ifsa->iftable->set(&oi, &a);
    }
}
