/*
 * Copyright 1991-1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	CfNode Class
 *
 *	$Revision: 1.11 $
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

extern "C" {
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <strings.h>
#include <protocols/dnhost.h>

char *ether_aton(const char *);
char *ether_ntoa(const char *);
char *ether_hostton(const char *, char *);
char *ether_ntohost(char *, const char *);
int inet_isaddr(const char *, struct in_addr *);
}

/*
 *	Static buffer for getName()'s
 */
static char buf[64];

/*
 *	Constant for physical broadcast address
 */
const char broadcastaddr[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

/*
 *	Physical Addresses
 */
PhysAddr::PhysAddr(void)
{
	valid = 0;
}

PhysAddr::PhysAddr(PhysAddr& p)
{
	*this = p;
}

PhysAddr::PhysAddr(struct etheraddr& e)
{
	*this = e;
}

PhysAddr::PhysAddr(const char *s)
{
	*this = s;
}

PhysAddr::PhysAddr(DNAddr &d)
{
	*this = d;
}

PhysAddr&
PhysAddr::operator=(PhysAddr& p)
{
	valid = p.valid;
	if (valid)
		bcopy(p.a, a, sizeof a);
	return *this;
}

PhysAddr&
PhysAddr::operator=(struct etheraddr& e)
{
	bcopy(e.ea_vec, a, sizeof a);
	valid = 1;
	return *this;
}

PhysAddr&
PhysAddr::operator=(const char *s)
{
	char *e = ether_aton(s);
	if (e != 0) {
		bcopy(e, a, sizeof a);
		valid = 1;
	} else if (ether_hostton(s, a) == 0)
		valid = 1;
	else
		valid = 0;
	return *this;
}

PhysAddr&
PhysAddr::operator=(DNAddr &d)
{
	valid = rp_etherfromaddr((Ether_addr *) a, RP_HOST, d.getAddr());
	return *this;
}

unsigned int
PhysAddr::isBroad(void)
{
	return a[0] == 0xFF && bcmp(a, broadcastaddr, sizeof a) == 0;
}

unsigned int
PhysAddr::isMulti(void)
{
	return (a[0] & 0x01) && bcmp(a, broadcastaddr, sizeof a) != 0;
}

unsigned int
PhysAddr::isGroup(void)
{
	return (a[0] & 0x01);
}

char *
PhysAddr::getName(void)
{
	if (valid) {
		if (ether_ntohost(buf, a) == 0)
			return buf;
	}
	return 0;
}


/*
 *	Internet Addresses
 */
IPAddr::IPAddr(void)
{
	valid = 0;
}

IPAddr::IPAddr(IPAddr& i)
{
	*this = i;
}

IPAddr::IPAddr(const char *s)
{
	*this = s;
}

IPAddr::IPAddr(struct in_addr ia)
{
	*this = ia;
}

IPAddr::IPAddr(unsigned long u)
{
	*this = u;
}

IPAddr&
IPAddr::operator=(IPAddr& i)
{
	valid = i.valid;
	if (valid)
		a = i.a;
	return *this;
}

IPAddr&
IPAddr::operator=(const char *s)
{
	valid = *s && inet_isaddr(s, &a);
	if (!valid) {
		struct hostent *he = gethostbyname(s);
		if (he != 0) {
			bcopy(he->h_addr, &a, sizeof a);
			valid = 1;
		}
	}
	return *this;
}

IPAddr&
IPAddr::operator=(struct in_addr ia)
{
	a.s_addr = ia.s_addr;
	if (a.s_addr == 0)
		valid = 0;
	else
		valid = 1;
	return *this;
}

IPAddr&
IPAddr::operator=(unsigned long u)
{
	a.s_addr = u;
	if (a.s_addr == 0)
		valid = 0;
	else
		valid = 1;
	return *this;
}

char *
IPAddr::getName(void)
{
	if (valid) {
		struct hostent *he = gethostbyaddr(&a, sizeof a, AF_INET);
		if (he != 0) {
			strcpy(buf, he->h_name);
			return buf;
		}
	}
	return 0;
}


/*
 *	IP Network Number
 */
IPNetNum::IPNetNum(void)
{
	valid = 0;
}

IPNetNum::IPNetNum(IPNetNum &i)
{
	*this = i;
}

IPNetNum::IPNetNum(IPAddr &i)
{
	*this = i;
}

IPNetNum::IPNetNum(const char *s)
{
	*this = s;
}

IPNetNum::IPNetNum(struct in_addr i)
{
	*this = i;
}

IPNetNum::IPNetNum(unsigned long u)
{
	*this = u;
}

IPNetNum&
IPNetNum::operator=(IPNetNum& i)
{
	valid = i.valid;
	if (valid)
		a = i.a;
	return *this;
}

IPNetNum&
IPNetNum::operator=(IPAddr& i)
{
	if (!i.isValid())
		valid = 0;
	else {
		a = *(i.getAddr());
		if (a.s_addr > 0x00FFFFFF)
			a.s_addr = inet_netof(a);
		valid = 1;
	}
	return *this;
}

IPNetNum&
IPNetNum::operator=(const char *s)
{
	a.s_addr = inet_network(s);
	if (a.s_addr != INADDR_NONE) {
		if (a.s_addr > 0x00FFFFFF)
			a.s_addr = inet_netof(a);
		valid = 1;
	} else {
		struct netent *ne = getnetbyname(s);
		if (ne != 0) {
			a.s_addr = ne->n_net;
			valid = 1;
		} else
			valid = 0;
	}
	return *this;
}

IPNetNum&
IPNetNum::operator=(struct in_addr i)
{
	a.s_addr = i.s_addr;
	if (a.s_addr == 0)
		valid = 0;
	else {
		if (a.s_addr > 0x00FFFFFF)
			a.s_addr = inet_netof(a);
		valid = 1;
	}
	return *this;
}

IPNetNum&
IPNetNum::operator=(unsigned long u)
{
	a.s_addr = u;
	if (a.s_addr == 0)
		valid = 0;
	else {
		if (a.s_addr > 0x00FFFFFF)
			a.s_addr = inet_netof(a);
		valid = 1;
	}
	return *this;
}

void
IPNetNum::setAddr(IPAddr &host, IPAddr &mask)
{
	if (!host.isValid() || !mask.isValid()) {
		valid = 0;
		return;
	}

	setAddr(host.getAddr()->s_addr, mask.getAddr()->s_addr);
}

void
IPNetNum::setAddr(IPAddr &host, struct in_addr mask)
{
	if (!host.isValid()) {
		valid = 0;
		return;
	}

	setAddr(host.getAddr()->s_addr, mask.s_addr);
}

void
IPNetNum::setAddr(IPAddr &host, unsigned long mask)
{
	if (!host.isValid()) {
		valid = 0;
		return;
	}

	setAddr(host.getAddr()->s_addr, mask);
}

void
IPNetNum::setAddr(struct in_addr host, IPAddr &mask)
{
	if (!mask.isValid()) {
		valid = 0;
		return;
	}
	setAddr(host.s_addr, mask.getAddr()->s_addr);
}

void
IPNetNum::setAddr(struct in_addr host, struct in_addr mask)
{
	setAddr(host.s_addr, mask.s_addr);
}

void
IPNetNum::setAddr(unsigned long host, IPAddr &mask)
{
	if (!mask.isValid()) {
		valid = 0;
		return;
	}
	setAddr(host, mask.getAddr()->s_addr);
}

void
IPNetNum::setAddr(unsigned long host, unsigned long mask)
{
	if (mask == 0)
		valid = 0;
	else {
		a.s_addr = (host & mask);
		for (unsigned int shift = 0; (mask & 1) == 0; mask >>= 1)
			shift++;
		a.s_addr >>= (8 * (shift / 8));
		valid = 1;
	}
}

char *
IPNetNum::getName(void)
{
	if (valid) {
		struct netent *n = getnetbyaddr(a.s_addr, AF_INET);
		if (n != 0) {
			strcpy(buf, n->n_name);
			return buf;
		}
	}
	return 0;
}

char *
IPNetNum::getString(void)
{
	static char b[18];

	if (!valid)
		return 0;

#define UC(b)   (((int)b)&0xff)
	char *p = (char *) &a;

	if (a.s_addr <= 0xFF)
		sprintf(b, "%d", UC(p[3]));
	else if (a.s_addr <= 0xFFFF)
		sprintf(b, "%d.%d", UC(p[2]), UC(p[3]));
	else if (a.s_addr <= 0xFFFFFF)
		sprintf(b, "%d.%d.%d", UC(p[1]), UC(p[2]), UC(p[3]));
	else
		sprintf(b, "%d.%d.%d.%d",
			   UC(p[0]), UC(p[1]), UC(p[2]), UC(p[3]));
#undef UC

	return b;
}

/*
 * Need this because the libc one doesn't do multicast.
 * Treat multicast like class A.  Treats unknowns like class C.
 */
unsigned long
IPNetNum::inet_netof(struct in_addr in)
{
	unsigned long i = ntohl(in.s_addr);

	if (IN_CLASSA(i) || IN_MULTICAST(i) || i == INADDR_BROADCAST)
		return (i & IN_CLASSA_NET) >> IN_CLASSA_NSHIFT;
	else if (IN_CLASSB(i))
		return (i & IN_CLASSB_NET) >> IN_CLASSB_NSHIFT;
	return (i & IN_CLASSC_NET) >> IN_CLASSC_NSHIFT;
}


/*
 *	DECnet Addresses
 */
DNAddr::DNAddr(void)
{
	valid = 0;
}

DNAddr::DNAddr(DNAddr& d)
{
	*this = d;
}

DNAddr::DNAddr(PhysAddr& p)
{
	*this = p;
}

DNAddr::DNAddr(char *s)
{
	*this = s;
}

DNAddr::DNAddr(dn_addr u)
{
	*this = u;
}

DNAddr&
DNAddr::operator=(DNAddr& d)
{
	valid = d.valid;
	if (valid)
		a = d.a;
	return *this;
}

DNAddr&
DNAddr::operator=(PhysAddr& p)
{
	valid = rp_addrfromether((Ether_addr *) p.getAddr(), RP_HOST, &a);
	return *this;
}

DNAddr&
DNAddr::operator=(char *s)
{
	if (rp_addr(s, &a) != 0) {
		valid = 1;
		return *this;
	}
	a = dn_getnodeaddr(s, ORD_HOST);
	if (a != 0) {
		valid = 1;
		return *this;
	}
	char *p = strchr(s, '.');
	if (p == 0 || (int) p - (int) s > 6)
		p = (char *) s + 6;
	char c = *p;
	*p = '\0';
	a = dn_getnodeaddr(s, ORD_HOST);
	*p = c;
	if (a == 0)
		valid = 0;
	else
		valid = 1;
	return *this;
}

DNAddr&
DNAddr::operator=(dn_addr d)
{
	a = d;
	if (a == 0)
		valid = 0;
	else
		valid = 1;
	return *this;
}

char *
DNAddr::getName(void)
{
	if (valid) {
		char *s = dn_getnodename(a, ORD_HOST);
		if (s != 0 && *s != '\0')
			return s;
	}
	return 0;
}


/*
 *	DECnet Areas
 */
DNArea::DNArea(void)
{
	valid = 0;
}

DNArea::DNArea(DNArea& d)
{
	*this = d;
}

DNArea::DNArea(DNAddr& d)
{
	*this = d;
}

DNArea::DNArea(PhysAddr& p)
{
	*this = p;
}

DNArea::DNArea(dn_addr u)
{
	*this = u;
}

DNArea&
DNArea::operator=(DNArea& d)
{
	valid = d.valid;
	if (valid)
		a = d.a;
	return *this;
}

DNArea&
DNArea::operator=(DNAddr& d)
{
	valid = d.isValid();
	if (valid)
		a = *d.getAddr() & 0xFC;
	return *this;
}

DNArea&
DNArea::operator=(PhysAddr& p)
{
	DNAddr d = p;
	*this = d;
	return *this;
}

DNArea&
DNArea::operator=(dn_addr d)
{
	DNAddr t = d;
	*this = t;
	return *this;
}

char *
DNArea::getName(void)
{
	if (valid) {
		char *s = dn_getnodename(a, ORD_HOST);
		if (s != 0 && *s != '\0')
			return s;
	}
	return 0;
}


/*
 *	Config Nodes
 */
CfNode::CfNode(void)
{
	child = parent = next = prev = 0;
	token = TOKEN_NULL;
	name = 0;
	data = 0;
}

CfNode::CfNode(enum cftoken t)
{
	child = parent = next = prev = 0;
	token = t;
	name = 0;
	data = 0;
}

CfNode::CfNode(CfNode *p, enum cftoken t)
{
	child = next = 0;
	parent = p;
	token = t;
	name = 0;
	data = 0;

	if (p->child == 0) {
		p->child = this;
		prev = 0;
		return;
	}
		
	for (CfNode *c = p->child; c->next != 0; c = c->next)
		;
	c->next = this;
	prev = c;
}

CfNode::~CfNode(void)
{
	if (next != 0)
		next->prev = prev;
	if (prev != 0)
		prev->next = next;

	for (CfNode *c = child; c != 0; c = child) {
		child = c->next;
		delete c;
	}
}

void
CfNode::setName(const char *n)
{
	if (name != 0)
		delete name;

	if (n == 0)
		name = 0;
	else {
		name = new char[strlen(n)+1];
		strcpy(name, n);
	}
}


/*
 *	Networks
 */
NetworkNode::NetworkNode(void) : CfNode(TOKEN_NETWORK)
{
}

NetworkNode::NetworkNode(CfNode *r) : CfNode(r, TOKEN_NETWORK)
{
}

void
NetworkNode::complete(void)
{
	char *n = name;
	if (n == 0) {
		n = ipnum.getName();
		if (n == 0)
			return;
		setName(n);
	}

	if (!ipnum.isValid())
		ipnum = n;
}


/*
 *	Network Segments
 */
SegmentNode::SegmentNode(void) : CfNode(TOKEN_SEGMENT)
{
	type = SEG_NULL;
}

SegmentNode::SegmentNode(NetworkNode *n) : CfNode(n, TOKEN_SEGMENT)
{
	type = SEG_NULL;
}

static char *_segname[] = {
		"",
		"Ethernet",
		"FDDI",
		"SLIP",
		"Unknown",
};

enum segtype
SegmentNode::setType(const char *s)
{
	for (enum segtype t = SEG_NULL; t <= SEG_UNKNOWN;
		t = (enum segtype) (((unsigned int) t) + 1)) {
		if (strcasecmp(_segname[t], s) == 0)
			return type = t;
	}

	return type = SEG_NULL;
}

char *
SegmentNode::printType(void)
{
	return _segname[type];
}

void
SegmentNode::complete(void)
{
	char *n = name;
	if (n == 0) {
		n = ipnum.getName();
		if (n == 0)
			return;
		setName(n);
	}

	if (!ipnum.isValid())
		ipnum = n;
}


/*
 *	Hosts
 */
HostNode::HostNode(unsigned int s, unsigned int m) : CfNode(TOKEN_HOST)
{
	NISserver = s;
	NISmaster = m;
}


/*
 *	Interfaces
 */
InterfaceNode::InterfaceNode(void) : CfNode(TOKEN_INTERFACE)
{
	nextif = 0;
	previf = 0;
}

InterfaceNode::InterfaceNode(SegmentNode *s) : CfNode(s, TOKEN_INTERFACE)
{
	nextif = 0;
	previf = 0;
}

void
InterfaceNode::setHost(HostNode *h)
{
	host = h;

	if (h->child == 0) {
		h->child = this;
		previf = 0;
		nextif = 0;
		return;
	}
		
	for (InterfaceNode *i = (InterfaceNode *) h->child;
					i->nextif != 0; i = i->nextif)
		;
	i->nextif = this;
	previf = i;
}

void
InterfaceNode::complete(void)
{
	unsigned int work = 1;
	char *n = name;

	while (work) {
		work = 0;

		if (n == 0) {
			n = physaddr.getName();
			if (n != 0)
				setName(n);
			else {
				n = ipaddr.getName();
				if (n != 0)
					setName(n);
				else {
					n = dnaddr.getName();
					if (n != 0)
						setName(n);
				}
			}
		}

		if (!physaddr.isValid()) {
			if (dnaddr.isValid()) {
				physaddr = dnaddr;
				work = 1;
			} else if (n != 0) {
				physaddr = n;
				if (physaddr.isValid())
					work = 1;
			}
		}
		if (!ipaddr.isValid() && n != 0) {
			ipaddr = n;
			if (ipaddr.isValid())
				work = 1;
		}
		if (!dnaddr.isValid()) {
			if (physaddr.isValid()) {
				dnaddr = physaddr;
				if (dnaddr.isValid()) {
					work = 1;
					continue;
				}
			}
			if (n != 0) {
				dnaddr = n;
				if (dnaddr.isValid())
					work = 1;
			}
		}
	}
}
