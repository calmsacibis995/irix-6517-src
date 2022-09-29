/*
 * Copyright 1991-1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Database classes
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

#include "cf.h"
#include "db.h"

extern "C" {
#include <netinet/in.h>
#include <netdb.h>
#include <strings.h>
#include <macros.h>
}

/*
 *	Index operations for PhysAddr class
 */
static unsigned int
pahash(PhysAddr *p, int)
{
    char *a = p->getAddr();
    return (a[2] << 24) | (a[3] << 16) | (a[4] << 8) | a[5];
}

static int
pacmp(PhysAddr *p1, PhysAddr *p2, int)
{
    return !(*p1 == *p2);
}

static indexops paops = { (unsigned int (*)()) pahash, (int (*)()) pacmp, 0 };


/*
 *	Index operations for IPAddr class
 */
static unsigned int
iphash(IPAddr *i, int)
{
    struct in_addr *a = i->getAddr();
    return a->s_addr;
}

static int
ipcmp(IPAddr *i1, IPAddr *i2, int)
{
    return !(*i1 == *i2);
}

static indexops ipops = { (unsigned int (*)()) iphash, (int (*)()) ipcmp, 0 };


/*
 *	Index operations for DNAddr class
 */
static unsigned int
dnhash(DNAddr *d, int)
{
    dn_addr *a = d->getAddr();
    return (int) a;
}

static int
dncmp(DNAddr *d1, DNAddr *d2, int)
{
    return !(*d1 == *d2);
}

static indexops dnops = { (unsigned int (*)()) dnhash, (int (*)()) dncmp, 0 };


/*
 *	InterfaceDB class
 */
InterfaceDB::InterfaceDB(void)
{
    paindex = 0;
    ipindex = 0;
    dnindex = 0;
}

InterfaceDB::InterfaceDB(CfNode *c)
{
    paindex = 0;
    ipindex = 0;
    dnindex = 0;
    create(c);
}

InterfaceDB::~InterfaceDB(void)
{
    destroy();
}

void
InterfaceDB::create(CfNode *root)
{
    NetworkNode *n;
    SegmentNode *s;
    InterfaceNode *i;
    unsigned int pa, ip, dn;
    const float HashFill = 1.2;
    const int MinHashSize = 1024;

    // Count the types of addresses to allocate a good-sized hash table
    pa = ip = dn = 0;
    for (n = (NetworkNode *) root->child; n != 0; n = (NetworkNode *) n->next) {
	for (s = (SegmentNode *) n->child;
		s != 0; s = (SegmentNode *) s->next) {
	    for (i = (InterfaceNode *) s->child;
		    i != 0; i = (InterfaceNode *) i->next) {
		if (i->physaddr.isValid())
		    pa++;
		if (i->ipaddr.isValid())
		    ip++;
		if (i->dnaddr.isValid())
		    dn++;
	    }
	}
    }

    pa = (int) (pa * HashFill);
    pa = MIN(pa, MinHashSize);
    if (paindex != 0)
	in_destroy(paindex);
    paindex = index(pa, sizeof(PhysAddr), &paops);

    ip = (int) (ip * HashFill);
    ip = MIN(ip, MinHashSize);
    if (ipindex != 0)
	in_destroy(ipindex);
    ipindex = index(ip, sizeof(IPAddr), &ipops);

    dn = (int) (dn * HashFill);
    dn = MIN(dn, MinHashSize);
    if (dnindex != 0)
	in_destroy(dnindex);
    dnindex = index(dn, sizeof(DNAddr), &dnops);

    for (n = (NetworkNode *) root->child; n != 0; n = (NetworkNode *) n->next) {
	for (s = (SegmentNode *) n->child;
		s != 0; s = (SegmentNode *) s->next) {
	    for (i = (InterfaceNode *) s->child;
		    i != 0; i = (InterfaceNode *) i->next)
		enter(i);
	}
    }
}

void
InterfaceDB::destroy(void)
{
    if (paindex != 0) {
	in_destroy(paindex);
	paindex = 0;
    }
    if (ipindex != 0) {
	in_destroy(ipindex);
	ipindex = 0;
    }
    if (dnindex != 0) {
	in_destroy(dnindex);
	dnindex = 0;
    }
}

void
InterfaceDB::enter(InterfaceNode *i)
{
    if (i->physaddr.isValid())
	(void) in_enterqual(paindex, &i->physaddr, i, (long) i->parent->parent);
    if (i->ipaddr.isValid())
	(void) in_enter(ipindex, &i->ipaddr, i);
    if (i->dnaddr.isValid())
	(void) in_enter(dnindex, &i->dnaddr, i);
}

void
InterfaceDB::remove(InterfaceNode *i)
{
    if (i->physaddr.isValid())
	in_removequal(paindex, &i->physaddr, (long) i->parent->parent);
    if (i->ipaddr.isValid())
	in_remove(ipindex, &i->ipaddr);
    if (i->dnaddr.isValid())
	in_remove(dnindex, &i->dnaddr);
}


/*
 *	SegmentDB class
 */
SegmentDB::SegmentDB(void)
{
    ipindex = 0;
    dnindex = 0;
}

SegmentDB::SegmentDB(CfNode *c)
{
    ipindex = 0;
    dnindex = 0;
    create(c);
}

SegmentDB::~SegmentDB(void)
{
    destroy();
}

void
SegmentDB::create(CfNode *root)
{
    NetworkNode *n;
    SegmentNode *s;
    unsigned int ip, dn;
    const float HashFill = 1.2;
    const int MinHashSize = 256;

    // Count the types of segments to allocate a good-sized hash table
    ip = dn = 0;
    for (n = (NetworkNode *) root->child; n != 0; n = (NetworkNode *) n->next) {
	for (s = (SegmentNode *) n->child;
		s != 0; s = (SegmentNode *) s->next) {
	    if (s->ipnum.isValid())
		ip++;
	    if (s->dnarea.isValid())
		dn++;
	}
    }

    ip = (int) (ip * HashFill);
    ip = MIN(ip, MinHashSize);
    if (ipindex != 0)
	in_destroy(ipindex);
    ipindex = index(ip, sizeof(IPNetNum), &ipops);

    dn = (int) (dn * HashFill);
    dn = MIN(dn, MinHashSize);
    if (dnindex != 0)
	in_destroy(dnindex);
    dnindex = index(dn, sizeof(DNAddr), &dnops);

    for (n = (NetworkNode *) root->child; n != 0; n = (NetworkNode *) n->next) {
	for (s = (SegmentNode *) n->child;
		s != 0; s = (SegmentNode *) s->next)
	    enter(s);
    }
}

void
SegmentDB::destroy(void)
{
    if (ipindex != 0) {
	in_destroy(ipindex);
	ipindex = 0;
    }
    if (dnindex != 0) {
	in_destroy(dnindex);
	dnindex = 0;
    }
}

void
SegmentDB::enter(SegmentNode *s)
{
    if (s->ipnum.isValid())
	in_enter(ipindex, &s->ipnum, s);
    if (s->dnarea.isValid())
	in_enter(dnindex, &s->dnarea, s);
}

void
SegmentDB::remove(SegmentNode *s)
{
    if (s->ipnum.isValid())
	in_remove(ipindex, &s->ipnum);
    if (s->dnarea.isValid())
	in_remove(dnindex, &s->dnarea);
}


/*
 *	NetworkDB class
 */
NetworkDB::NetworkDB(void)
{
    ipindex = 0;
}

NetworkDB::NetworkDB(CfNode *c)
{
    ipindex = 0;
    create(c);
}

NetworkDB::~NetworkDB(void)
{
    destroy();
}

void
NetworkDB::create(CfNode *root)
{
    NetworkNode *n;
    unsigned int ip;
    const float HashFill = 1.2;
    const int MinHashSize = 128;

    // Count the types of segments to allocate a good-sized hash table
    ip = 0;
    for (n = (NetworkNode *) root->child; n != 0; n = (NetworkNode *) n->next) {
	if (n->ipnum.isValid())
	    ip++;
    }

    ip = (int) (ip * HashFill);
    ip = MIN(ip, MinHashSize);
    if (ipindex != 0)
	in_destroy(ipindex);
    ipindex = index(ip, sizeof(IPNetNum), &ipops);

    for (n = (NetworkNode *) root->child; n != 0; n = (NetworkNode *) n->next)
	enter(n);
}

void
NetworkDB::destroy(void)
{
    if (ipindex != 0)
	in_destroy(ipindex);
}

void
NetworkDB::enter(NetworkNode *n)
{
    if (n->ipnum.isValid())
	in_enter(ipindex, &n->ipnum, n);
}

void
NetworkDB::remove(NetworkNode *n)
{
    if (n->ipnum.isValid())
	in_remove(ipindex, &n->ipnum);
}
