/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Interfaces sub-agent
 *
 *	$Revision: 1.6 $
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
#define if_mtu          if_data.ifi_mtu
#define if_type         if_data.ifi_type
#define if_addrlen      if_data.ifi_addrlen
#define if_hdrlen       if_data.ifi_hdrlen
#define if_metric       if_data.ifi_metric
#define if_baudrate     if_data.ifi_baudrate
#define if_ipackets     if_data.ifi_ipackets
#define if_ierrors      if_data.ifi_ierrors
#define if_opackets     if_data.ifi_opackets
#define if_oerrors      if_data.ifi_oerrors
#define if_collisions   if_data.ifi_collisions
#define if_ibytes       if_data.ifi_ibytes
#define if_obytes       if_data.ifi_obytes
#define if_imcasts      if_data.ifi_imcasts
#define if_omcasts      if_data.ifi_omcasts
#define if_iqdrops      if_data.ifi_iqdrops
#define if_odrops       if_data.ifi_odrops
#define if_noproto      if_data.ifi_noproto
#define if_lastchange   if_data.ifi_lastchange

#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/soioctl.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <bstring.h>
#include <stdio.h>

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
#include "knlist.h"
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
    const subID ifAddr = 24;		// Internal
    const subID ifAddrList = 25;	// Internal

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

    struct arpcom arpcom;
    struct ifnet ifnet;
    asnObject *a;
	 asnObject *sysUp;
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
    char buf[16];

    oid oi = "1.3.6.1.2.1.2.2.1.1.1";
    oi.getValue(&subid, &len);

    // XXX - What to do with nlist
    off_t ifnetaddr;
    
#ifdef sgi
    if (is_elf64)
       ifnetaddr = nl64[N_IFNET].n_value;
    else
#endif /* sgi */
    ifnetaddr = nl[N_IFNET].n_value;
    klseek(ifnetaddr);
    if (kread(&ifnetaddr, sizeof ifnetaddr) < 0) {
	fprintf(stderr, "error reading kmem\n");
	return;
    }

    ifsa->ifnumber = 0;
    while(ifnetaddr != 0) {
/*    for ( ; ifnetaddr != 0; ifnetaddr = (off_t) ifnet.if_next) {  */
	// Read ifnet structure from kernel
	klseek(ifnetaddr);
	if (kread(&ifnet, sizeof ifnet) < 0) {
	    fprintf(stderr, "error reading kmem\n");
	    return;
	}

	ifsa->ifnumber++;
	subid[len - 2] = ifIndex;
	subid[len - 1] = ifnet.if_index;
	rc = ifsa->iftable->get(&oi, &a);
	if (rc == SNMP_ERR_noError)
	    delete a;
	else {
	    // New interface
	    i = (unsigned int)ifnet.if_index;
	    a = &i;
	    ifsa->iftable->set(&oi, &a);

	    // Store name for internal use
	    subid[len - 2] = ifName;
	    char n[IFNAMSIZ], *p;
	    klseek((off_t) ifnet.if_name);
	    if (kread(n, IFNAMSIZ) < 0) {
		return;
	    }
	    for (p = n; *p != '\0'; p++)
		;
	    *p++ = ifnet.if_unit + '0';
	    *p = '\0';
	    o = n;
	    a = &o;
	    ifsa->iftable->set(&oi, &a);

	    subid[len - 2] = ifDesc;
	    switch (n[0]) {
		case 'e':
		    switch (n[1]) {
			case 'c':
			    o = "Silicon Graphics ec Ethernet controller";
			    break;
			case 'n':
			    o = "Silicon Graphics enp Ethernet controller";
			    break;
			case 't':
			    o = "Silicon Graphics et Ethernet controller";
			    break;
			default:
			n;
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
		default:
		    o = n;
		    break;
	    }
	    ifsa->iftable->set(&oi, &a);

	    subid[len - 2] = ifType;
	    i = (unsigned int)ifnet.if_type;
	    a = &i;
	    ifsa->iftable->set(&oi, &a);

	    subid[len - 2] = ifMtu;
	    i = ifnet.if_mtu;
	    ifsa->iftable->set(&oi, &a);

	    subid[len - 2] = ifSpeed;
	    snmpGauge g;
	    switch (ifnet.if_type) {
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
	}

	// For physical address, use knowledge the the ifnet structure
	// is really part of an arpcom structure for Ethernet and FDDI.
	subid[len - 2] = ifPhysAddr;
	if (ifnet.if_type != IFT_ETHER && ifnet.if_type != IFT_FDDI)
	    o = 0;
	else {
	    klseek(ifnetaddr);
	    if (kread(&arpcom, sizeof arpcom) < 0) {
		fprintf(stderr, "error reading kmem\n");
		return;
	    }
	    o.setValue((char *)arpcom.ac_enaddr, sizeof arpcom.ac_enaddr);
	}
	a = &o;
	ifsa->iftable->set(&oi, &a);

	subid[len - 2] = ifOperStatus;
	i = (ifnet.if_flags & IFF_UP) ? up : down;
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
	if (ifnet.if_lastchange <= start) // before the agent started?
		t = (int) 0;
	else
		t = (int) ((ifnet.if_lastchange - start) * 100);
	delete (snmpTimeticks *) sysUp;
	a = &t;
	ifsa->iftable->set(&oi, &a);

	subid[len - 2] = ifInOctets;
	c = (unsigned int) ifnet.if_ibytes;
	a = &c;
	ifsa->iftable->set(&oi, &a);

	subid[len - 2] = ifInUcast;
	c = (unsigned int) (ifnet.if_ipackets - ifnet.if_imcasts);
	ifsa->iftable->set(&oi, &a);

	subid[len - 2] = ifInNUcast;
	c = (unsigned int) ifnet.if_imcasts;
	ifsa->iftable->set(&oi, &a);

	subid[len - 2] = ifInDiscards;
	c = (unsigned int) ifnet.if_iqdrops;
	ifsa->iftable->set(&oi, &a);

	subid[len - 2] = ifInErrors;
	c = (unsigned int) ifnet.if_ierrors;
	ifsa->iftable->set(&oi, &a);

	subid[len - 2] = ifInUnknown;
	c = (unsigned int) ifnet.if_noproto;
	ifsa->iftable->set(&oi, &a);

	subid[len - 2] = ifOutOctets;
	c = (unsigned int) ifnet.if_obytes;
	ifsa->iftable->set(&oi, &a);

	subid[len - 2] = ifOutUcast;
	c = (unsigned int) (ifnet.if_opackets - ifnet.if_omcasts);
	ifsa->iftable->set(&oi, &a);

	subid[len - 2] = ifOutNUcast;
	c = (unsigned int) ifnet.if_omcasts;
	ifsa->iftable->set(&oi, &a);

	subid[len - 2] = ifOutDiscards;
	c = (unsigned int) ifnet.if_odrops;
	ifsa->iftable->set(&oi, &a);

	subid[len - 2] = ifOutErrors;
	c = (unsigned int) ifnet.if_oerrors;
	ifsa->iftable->set(&oi, &a);

	subid[len - 2] = ifOutQLen;
	g = (unsigned int) ifnet.if_snd.ifq_len;
	a = &g;
	ifsa->iftable->set(&oi, &a);

	// Store address for ipRouteTable
	subid[len - 2] = ifAddr;
	sprintf(buf, "%lx", ifnetaddr);
	o = buf;
	a = &o;
	ifsa->iftable->set(&oi, &a);

	// Store address list for ipAddrTable, we store this as a string
	// since there is no asn type for a pointer.
	subid[len - 2] = ifAddrList;
	unsigned long tmp = (unsigned long)ifnet.in_ifaddr;
	sprintf(buf, "%lx", tmp);
	o = buf;
	a = &o;
	ifsa->iftable->set(&oi, &a);

	ifnetaddr = (off_t) ifnet.if_next;
    }
}
