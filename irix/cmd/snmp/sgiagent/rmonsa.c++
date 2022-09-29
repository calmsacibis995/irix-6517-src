/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	RMON sub-agent
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
#include <sys/prctl.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <syslog.h>
#include <bstring.h>
#include <string.h>
#include <math.h>
#include "oid.h"
#include "asn1.h"
#include "snmp.h"
#include "packet.h"
#include "subagent.h"
#include "sat.h"
#include "agent.h"
#include "table.h"
#include "ifsa.h"
#include "rmonsa.h"

extern "C" {
#include <expr.h>
#include <protocol.h>
}
extern int _yp_disabled;

extern "C" {
int gethostname(char *, int);		// XXX - Where is this?
}

const subID statistics = 1;
    const subID statsIndex = 1;
    const subID statsDataSource = 2;
    const subID statsDropEvents = 3;
    const subID statsOctets = 4;
    const subID statsPkts = 5;
    const subID statsBroadcast = 6;
    const subID statsMulticast = 7;
    const subID statsAlignErrors = 8;
    const subID statsUndersize = 9;
    const subID statsOversize = 10;
    const subID statsFragments = 11;
    const subID statsJabbers = 12;
    const subID statsCollisions = 13;
    const subID stats64Octets = 14;
    const subID stats128Octets = 15;
    const subID stats256Octets = 16;
    const subID stats512Octets = 17;
    const subID stats1024Octets = 18;
    const subID stats1518Octets = 19;
    const subID statsOwner = 20;
    const subID statsStatus = 21;
    const subID statsHistIndex = 22;		// Internal Only!

const int valid = 1;
const int createRequest = 2;
const int underCreation = 3;
const int invalid = 4;

const float snmpMaxCounter = 4.2949672959e9;

const int rmon_timeout = 10;
const int rmon_interval = 10;

const unsigned int filterTotal = 0;
const unsigned int filterBroadcast = 1;
const unsigned int filterMulticast = 2;
const unsigned int filterCRCFrame = 3;
const unsigned int filterTooSmall = 4;
const unsigned int filterTooBig = 5;
const unsigned int filterFragment = 6;
const unsigned int filterJabber = 7;
const unsigned int filter64Octet = 8;
const unsigned int filter128Octet = 9;
const unsigned int filter256Octet = 10;
const unsigned int filter512Octet = 11;
const unsigned int filter1024Octet = 12;

// Thread for processing NetVisualyzer RPCs.
void nvprocess(void *);

// For NetVisualyzer thread
static rmonSubAgent *rmonsa;

// Need ifName from ifIndex 
extern ifSubAgent *ifsa;

rmonSubAgent::rmonSubAgent(void)
{
    protosinitted = 0;
    // Export subtree
    subtree = "1.3.6.1.2.1.16";
    int rc = export("rmon", &subtree);
    if (rc != SNMP_ERR_genErr) {
	log(LOG_ERR, 0, "error exporting rmon subagent");
	return;
    }

    // Define tables
    statTable = table(1, 2, 0);

    // Initialize variables
    bzero(nv, sizeof nv);
    hostname = 0;
    rmonsa = this;

    // Get host name
    char tmp[64];
    if (gethostname(tmp, 63) != 0) {
	fprintf(stderr, "error getting host name\n");
	return;
    }
    struct hostent *he = gethostbyname(tmp);
    if (he != 0) {
	hostname = new char[strlen(he->h_name) + 1];
	strcpy(hostname, he->h_name);
    }

}

rmonSubAgent::~rmonSubAgent(void)
{
    unexport(&subtree);
    delete hostname;
    delete statTable;
}

int
rmonSubAgent::get(asnObjectIdentifier *o, asnObject **a, int *)
{
    if (!protosinitted)
	initprotos();
	
    // Pull out the subID array and length
    subID *subid;
    unsigned int len;
    oid *oi = o->getValue();
    oi->getValue(&subid, &len);

    // Check that the subID array is of valid size
    if (len == sublen)
	return SNMP_ERR_noSuchName;

    // All tables
    switch (subid[sublen]) {
	case statistics:
	    {
		if (len != sublen + 5 || subid[sublen + 1] != 1
		    || subid[sublen + 2] != 1
		    || subid[sublen + 3] == 0
		    || subid[sublen + 3] > statsStatus)
		return SNMP_ERR_noSuchName;

		subID *sub;
		unsigned int l;
		int rc;
		snmpCounter *c;
		float t1, t2;
		asnObject *ao = 0;
		oid iioi = "1.3.6.1.2.1.16.1.1.1.1.1";
		iioi.getValue(&sub, &l);
		sub[l - 2] = statsHistIndex;
		sub[l - 1] = subid[len - 1];
		rc = statTable->get(&iioi, &ao);
		if (rc != SNMP_ERR_noError)
		    return rc;
		struct histogram *h = &nv[((asnInteger *) ao)->getValue()].hist;

		switch (subid[sublen + 3]) {
		    case statsIndex:
		    case statsDataSource:
		    case statsOwner:
		    case statsStatus:
			return statTable->get(oi, a);

		    case statsDropEvents:
		    case statsCollisions:
			// XXX - return 0 for now
			*a = new snmpCounter(0);
			return SNMP_ERR_noError;

		    case statsOctets:
			*a = c = new snmpCounter;
			t1 = h->h_count[filterTotal].c_bytes;
			t2 = h->h_count[filterCRCFrame].c_bytes;
			t1 += t2;
			t2 = h->h_count[filterTooSmall].c_bytes;
			t1 += t2;
			t2 = h->h_count[filterTooBig].c_bytes,
			t1 = fmodf(t1 + t2, snmpMaxCounter);
			*c = (int) t1;
			return SNMP_ERR_noError;

		    case statsPkts:
			*a = c = new snmpCounter;
			t1 = h->h_count[filterTotal].c_packets;
			t2 = h->h_count[filterCRCFrame].c_packets;
			t1 += t2;
			t2 = h->h_count[filterTooSmall].c_packets;
			t1 += t2;
			t2 = h->h_count[filterTooBig].c_packets,
			t1 = fmodf(t1 + t2, snmpMaxCounter);
			*c = (int) t1;
			return SNMP_ERR_noError;

		    case statsBroadcast:
			*a = c = new snmpCounter;
			t1 = fmodf(h->h_count[filterBroadcast].c_packets,
				   snmpMaxCounter);
			*c = (int) t1;
			return SNMP_ERR_noError;

		    case statsMulticast:
			*a = c = new snmpCounter;
			t1 = h->h_count[filterMulticast].c_packets;
			t2 = h->h_count[filterBroadcast].c_packets;
			t1 = fmodf(t1 - t2, snmpMaxCounter);
			*c = (int) t1;
			return SNMP_ERR_noError;

		    case statsAlignErrors:
			*a = c = new snmpCounter;
			t1 = h->h_count[filterCRCFrame].c_packets;
			t2 = h->h_count[filterFragment].c_packets;
			t1 -= t2;
			t2 = h->h_count[filterJabber].c_packets;
			t1 = fmodf(t1 - t2, snmpMaxCounter);
			*c = (int) t1;
			return SNMP_ERR_noError;

		    case statsUndersize:
			*a = c = new snmpCounter;
			t1 = fmodf(h->h_count[filterTooSmall].c_packets,
				   snmpMaxCounter);
			*c = (int) t1;
			return SNMP_ERR_noError;

		    case statsOversize:
			*a = c = new snmpCounter;
			t1 = fmodf(h->h_count[filterTooBig].c_packets,
				   snmpMaxCounter);
			*c = (int) t1;
			return SNMP_ERR_noError;

		    case statsFragments:
			*a = c = new snmpCounter;
			t1 = fmodf(h->h_count[filterFragment].c_packets,
				   snmpMaxCounter);
			*c = (int) t1;
			return SNMP_ERR_noError;

		    case statsJabbers:
			*a = c = new snmpCounter;
			t1 = fmodf(h->h_count[filterJabber].c_packets,
				   snmpMaxCounter);
			*c = (int) t1;
			return SNMP_ERR_noError;

		    case stats64Octets:
			*a = c = new snmpCounter;
			t1 = fmodf(h->h_count[filter64Octet].c_packets,
				   snmpMaxCounter);
			*c = (int) t1;
			return SNMP_ERR_noError;

		    case stats128Octets:
			*a = c = new snmpCounter;
			t1 = h->h_count[filter128Octet].c_packets;
			t2 = h->h_count[filter64Octet].c_packets;
			t1 = fmodf(t1 - t2, snmpMaxCounter);
			*c = (int) t1;
			return SNMP_ERR_noError;

		    case stats256Octets:
			*a = c = new snmpCounter;
			t1 = h->h_count[filter256Octet].c_packets;
			t2 = h->h_count[filter128Octet].c_packets;
			t1 = fmodf(t1 - t2, snmpMaxCounter);
			*c = (int) t1;
			return SNMP_ERR_noError;

		    case stats512Octets:
			*a = c = new snmpCounter;
			t1 = h->h_count[filter512Octet].c_packets;
			t2 = h->h_count[filter256Octet].c_packets;
			t1 = fmodf(t1 - t2, snmpMaxCounter);
			*c = (int) t1;
			return SNMP_ERR_noError;

		    case stats1024Octets:
			*a = c = new snmpCounter;
			t1 = h->h_count[filter1024Octet].c_packets;
			t2 = h->h_count[filter512Octet].c_packets;
			t1 = fmodf(t1 - t2, snmpMaxCounter);
			*c = (int) t1;
			return SNMP_ERR_noError;

		    case stats1518Octets:
			*a = c = new snmpCounter;
			t1 = h->h_count[filterTotal].c_packets;
			t2 = h->h_count[filter1024Octet].c_packets;
			t1 = fmodf(t1 - t2, snmpMaxCounter);
			*c = (int) t1;
			return SNMP_ERR_noError;
		}
	    }

    }

    return SNMP_ERR_noSuchName;
}

int
rmonSubAgent::getNext(asnObjectIdentifier *o, asnObject **a, int *t)
{
    if (!protosinitted)
	initprotos();
	
    return simpleGetNext(o, a, t, statistics);
}

int
rmonSubAgent::set(asnObjectIdentifier *o, asnObject **a, int *)
{
    if (!protosinitted)
	initprotos();
	
    // Pull out the subID array and length
    subID *subid;
    unsigned int len;
    oid *oi = o->getValue();
    oi->getValue(&subid, &len);

    // Check that the subID array is of valid size
    if (len == sublen)
	return SNMP_ERR_noSuchName;

    // All tables
    switch (subid[sublen]) {
	case statistics:
	    if (len != sublen + 5 || subid[sublen + 1] != 1)
		return SNMP_ERR_noSuchName;

	    switch (subid[sublen + 3]) {
		case statsDataSource:
		    {
			if ((*a) != 0
			    && (*a)->getTag() != TAG_OBJECT_IDENTIFIER)
			    return SNMP_ERR_badValue;

			// Check that row is in underCreation state
			asnInteger *i;
			subid[len - 2] = statsStatus;
			int rc = statTable->get(oi, (asnObject **) &i);
			subid[len - 2] = statsDataSource;
			if (rc != SNMP_ERR_noError)
			    return rc;
			rc = i->getValue();
			if (rc != underCreation)
			    return SNMP_ERR_badValue;

			return statTable->set(oi, a);
		    }

		case statsOwner:
		    {
			if ((*a) != 0
			    && (*a)->getTag() != TAG_OCTET_STRING)
			    return SNMP_ERR_badValue;

			// Check that row is in underCreation state
			asnInteger *i;
			subid[len - 2] = statsStatus;
			int rc = statTable->get(oi, (asnObject **) &i);
			subid[len - 2] = statsOwner;
			if (rc != SNMP_ERR_noError)
			    return rc;
			rc = i->getValue();
			if (rc != underCreation)
			    return SNMP_ERR_badValue;

			return statTable->set(oi, a);
		    }

		case statsStatus:
		    {
			if ((*a) != 0 && (*a)->getTag() != TAG_INTEGER)
			    return SNMP_ERR_badValue;
			int av = ((asnInteger *) *a)->getValue();
			switch (av) {
			    case valid:
				{
				    // XXX - What error to return when
				    // this row is in error?

				    // Check that status is underCreation
				    asnInteger *i;
				    int rc = statTable->get(oi,
							    (asnObject **) &i);
				    if (rc != SNMP_ERR_noError)
					return rc;
				    if (i->getValue() == valid)
					return SNMP_ERR_noError;
				    delete i;

				    // Find which interface to use
				    // Get object identifier for data source
				    subID *subds, *subif;
				    unsigned int lds, lif;
				    asnObjectIdentifier *ifo = 0;

				    subid[len - 2] = statsDataSource;
				    rc = statTable->get(oi,
							(asnObject **) &ifo);
				    subid[len - 2] = statsStatus;
				    if (rc != SNMP_ERR_noError)
					return rc;

				    // Now get the name of that interface
				    oid *ifoid = ifo->getValue();
				    if (ifoid == 0)
					return SNMP_ERR_badValue;
				    ifoid->getValue(&subds, &lds);

				    oid ifindoid = "1.3.6.1.2.1.2.2.1.1.1";
				    ifindoid.getValue(&subif, &lif);
				    if (lif != lds
					|| bcmp(subif, subds,
						(lif-1) * sizeof(subID)) != 0) {
					delete ifo;
					return SNMP_ERR_badValue;
				    }

				    // XXX - Yuck!  Need internal name of if
				    asnOctetString *os;
				    unsigned int ifindex = subds[lds - 1];
				    os = ifsa->getIfName(ifindex);
				    if (os == 0) {
					delete ifo;
					return SNMP_ERR_badValue;
				    }

				    // Store index
				    subid[len - 2] = statsHistIndex;
				    asnInteger in = ifindex;
				    asnObject *ao = &in;
				    rc = statTable->set(oi, &ao);
				    subid[len - 2] = statsStatus;
				    if (rc != SNMP_ERR_noError)
					return rc;
				    delete ifo;

				    // Start NetVisualyzer
				    char *ifn = new char[os->getLength() + 1];
				    bcopy(os->getValue(), ifn, os->getLength());
				    ifn[os->getLength()] = '\0';
				    rc = startNetVis(ifindex, ifn);
				    delete os;
				    delete ifn;
				    if (rc != SNMP_ERR_noError)
					return rc;

				    return statTable->set(oi, a);
				}

			    case createRequest:
				{
				    // Fill in the row
				    asnObject *ao;
				    if (statTable->get(oi, &ao) !=
							SNMP_ERR_noSuchName) {
					delete ao;
					return SNMP_ERR_badValue;
				    }

				    unsigned int l;
				    subID *sub;
				    oid toi = "1.3.6.1.2.1.16.1.1.1.1.1";
				    toi.getValue(&sub, &l);

				    sub[l - 1] = subid[len - 1];
				    sub[l - 2] = statsIndex;
				    asnInteger i = (int) subid[len - 1];
				    ao = &i;
				    statTable->set(&toi, &ao);

				    sub[l - 2] = statsDataSource;
				    asnObjectIdentifier o = "0.0";
				    ao = &o;
				    statTable->set(&toi, &ao);

				    // statsDropEvents to statsPkts1518
				    snmpCounter c = 0;
				    ao = &c;
				    for (int j = statsDropEvents;
						j < statsOwner; j++) {
					sub[l - 2] = j;
					statTable->set(&toi, &ao);
				    }

				    sub[l - 2] = statsOwner;
				    asnOctetString s;
				    ao = &s;
				    statTable->set(&toi, &ao);

				    sub[l - 2] = statsHistIndex;
				    ao = &i;
				    i = -1;
				    statTable->set(&toi, &ao);

				    sub[l - 2] = statsStatus;
				    i = underCreation;
				    statTable->set(&toi, &ao);
				}
				return SNMP_ERR_noError;

			    case invalid:
				{
				    asnObject *ao;
				    int rc, i;

				    // Get HistIndex
				    subid[len - 2] = statsHistIndex;
				    rc = statTable->get(oi, &ao);
				    subid[len - 2] = statsStatus;
				    if (rc != SNMP_ERR_noError)
					return rc;

				    // Stop netvis on this index
				    i = ((asnInteger *) ao)->getValue();
				    delete ao;
				    if (i >= 0) {
					rc = stopNetVis(i);
					if (rc != SNMP_ERR_noError)
					    return rc;
				    }

				    // Delete row
				    ao = 0;
				    for (i = 1; i <= statsHistIndex; i++) {
					subid[len - 2] = i;
					statTable->set(oi, &ao);
				    }
				    subid[len - 2] = statsStatus;
				}
				return SNMP_ERR_noError;
			}

			return SNMP_ERR_badValue;
		    }
	    }
    }
    return SNMP_ERR_noSuchName;
}

int
rmonSubAgent::startNetVis(int index, char *ifname)
{
    // If already monitored, just increment refcount
    if (nv[index].refcount != 0) {
	nv[index].refcount++;
	return SNMP_ERR_noError;
    }

    ExprSource src;
    ExprError err;
    int bin;
    struct snoopstream *s = &(nv[index].ss);
    struct timeval timeout;

    // Subscribe to histogram service
    timeout.tv_sec = rmon_timeout;
    timeout.tv_usec = 0;
    if (!ss_open(s, hostname, 0, &timeout))
	return SNMP_ERR_genErr;
    if (!ss_subscribe(s, SS_HISTOGRAM, ifname, 0, 0, rmon_interval)) {
	ss_close(s);
	return SNMP_ERR_genErr;
    }

    // Add filters

    // Total
    bin = ss_compile(s, 0, &err);
    if (bin < 0) {
	ss_close(s);
	return SNMP_ERR_genErr;
    }

    // Broadcast
    src.src_path = name;
    src.src_line = filterBroadcast;
    src.src_buf = "dst == BROADCAST";
    bin = ss_compile(s, &src, &err);
    if (bin < 0) {
	ss_close(s);
	return SNMP_ERR_genErr;
    }

    // Multicast (including broadcast)
    src.src_line = filterMulticast;
    src.src_buf = "(dst & 1:0:0:0:0:0) == 1:0:0:0:0:0";
    bin = ss_compile(s, &src, &err);
    if (bin < 0) {
	ss_close(s);
	return SNMP_ERR_genErr;
    }

    // CRC and framing errors
    src.src_line = filterCRCFrame;
    src.src_buf = "snoop.flags & (CHECKSUM | FRAME)";
    bin = ss_compile(s, &src, &err);
    if (bin < 0) {
	ss_close(s);
	return SNMP_ERR_genErr;
    }

    // Too small errors
    src.src_line = filterTooSmall;
    src.src_buf = "snoop.flags & TOOSMALL";
    bin = ss_compile(s, &src, &err);
    if (bin < 0) {
	ss_close(s);
	return SNMP_ERR_genErr;
    }

    // Too big errors
    src.src_line = filterTooBig;
    src.src_buf = "snoop.flags & TOOBIG";
    bin = ss_compile(s, &src, &err);
    if (bin < 0) {
	ss_close(s);
	return SNMP_ERR_genErr;
    }

    // Too small packets with checksum or framing errors (fragment)
    src.src_line = filterFragment;
    src.src_buf = "(snoop.flags & TOOSMALL) && (snoop.flags & (CHECKSUM | FRAME))";
    bin = ss_compile(s, &src, &err);
    if (bin < 0) {
	ss_close(s);
	return SNMP_ERR_genErr;
    }

    // Too bit packets with checksum or framing errors (jabbers)
    src.src_line = filterJabber;
    src.src_buf = "(snoop.flags & TOOBIG) && (snoop.flags & (CHECKSUM | FRAME))";
    bin = ss_compile(s, &src, &err);
    if (bin < 0) {
	ss_close(s);
	return SNMP_ERR_genErr;
    }

    // 64 byte packets with FCS
    src.src_line = filter64Octet;
    src.src_buf = "snoop.len == 60";
    bin = ss_compile(s, &src, &err);
    if (bin < 0) {
	ss_close(s);
	return SNMP_ERR_genErr;
    }

    // Less than 128 byte packets with FCS
    src.src_line = filter128Octet;
    src.src_buf = "snoop.len < 124";
    bin = ss_compile(s, &src, &err);
    if (bin < 0) {
	ss_close(s);
	return SNMP_ERR_genErr;
    }

    // Less than 256 byte packets with FCS
    src.src_line = filter256Octet;
    src.src_buf = "snoop.len < 252";
    bin = ss_compile(s, &src, &err);
    if (bin < 0) {
	ss_close(s);
	return SNMP_ERR_genErr;
    }

    // Less than 512 byte packets with FCS
    src.src_line = filter512Octet;
    src.src_buf = "snoop.len < 508";
    bin = ss_compile(s, &src, &err);
    if (bin < 0) {
	ss_close(s);
	return SNMP_ERR_genErr;
    }

    // Less than 1024 byte packets with FCS
    src.src_line = filter1024Octet;
    src.src_buf = "snoop.len < 1020";
    bin = ss_compile(s, &src, &err);
    if (bin < 0) {
	ss_close(s);
	return SNMP_ERR_genErr;
    }

    // Start it up
    if (!ss_start(s)) {
	ss_close(s);
	return SNMP_ERR_genErr;
    }

    // Sproc thread to read service
    nv[index].refcount++;
    nv[index].pid = sproc(nvprocess, PR_SALL, (void *) &index);
    if (nv[index].pid == -1) {
	ss_close(s);
	nv[index].refcount--;
	return SNMP_ERR_genErr;
    }
    return SNMP_ERR_noError;
}

int
rmonSubAgent::stopNetVis(int index)
{
    // Let the sproc processes unsubscribe itself
    nv[index].refcount--;
    if (waitpid(nv[index].pid, 0, 0) < 0)
	return SNMP_ERR_genErr;
    return SNMP_ERR_noError;
}

void
nvprocess(void *arg)
{
    struct snoopstream *s = &(rmonsa->nv[*(int *)arg].ss);
    struct histogram *h = &(rmonsa->nv[*(int *)arg].hist);
    unsigned int *refc = &(rmonsa->nv[*(int *)arg].refcount);

    while (*refc != 0) {
	if (!ss_read(s, (xdrproc_t) xdr_histogram, h))
	    exit(0);
    }
    if (ss_stop(s))
	(void) ss_unsubscribe(s);
    ss_close(s);
    exit(0);
}

void
rmonSubAgent::initprotos(void) {
    
    // Initialize Protocols
    _yp_disabled = 1;
    initprotocols();
    _yp_disabled = 0;
    
    protosinitted = 1;

}
