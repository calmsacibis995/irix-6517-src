#ifndef _CF_H_
#define _CF_H_
/*
 * Copyright 1991-1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Network Configuration include file
 *
 *	$Revision: 1.9 $
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

enum cftoken {
	TOKEN_NULL,
	TOKEN_NETWORK,
	TOKEN_IPNET,
	TOKEN_IPMASK,
	TOKEN_SEGMENT,
	TOKEN_DNAREA,
	TOKEN_TYPE,
	TOKEN_HOST,
	TOKEN_NISSERVER,
	TOKEN_NISMASTER,
	TOKEN_INTERFACE,
	TOKEN_PHYSADDR,
	TOKEN_IPADDR,
	TOKEN_DNADDR,
	TOKEN_LBRACE,
	TOKEN_RBRACE,
	TOKEN_ID,
	TOKEN_ERROR,
	TOKEN_EOF
};

#ifdef _LANGUAGE_C_PLUS_PLUS
extern "C" {
#include <sys/types.h>
#include <arpa/inet.h>
#include <bstring.h>
#include <protocols/ether.h>
#include <protocols/decnet.h>

char *ether_ntoa(const char *);
char *decnet_ntoa(dn_addr);
void cf_lex(struct __file_s *, int *, enum cftoken *, char **);
}

class DNAddr;

//
//	Physical Addresses
//
class PhysAddr {
public:
	PhysAddr(void);
	PhysAddr(PhysAddr &);
	PhysAddr(struct etheraddr &);
	PhysAddr(const char *);
	PhysAddr(DNAddr &);

	PhysAddr&	operator=(PhysAddr &);
	PhysAddr&	operator=(struct etheraddr &);
	PhysAddr&	operator=(const char *);
	PhysAddr&	operator=(DNAddr &);

	friend int	operator==(PhysAddr &p1, PhysAddr &p2)
		{ return p1.valid && p2.valid
			 && bcmp(p1.a, p2.a, sizeof p1.a) == 0; }

	inline unsigned int isValid(void);

	unsigned int	isBroad(void);
	unsigned int	isMulti(void);
	unsigned int	isGroup(void);

	inline char	*getAddr(void);
	char		*getName(void);
	inline char	*getString(void);

private:
	char		a[ETHERADDRLEN];
	unsigned short	valid;
};

//
//	IP Addresses
//
class IPAddr {
public:
	IPAddr(void);
	IPAddr(IPAddr &);
	IPAddr(const char *);
	IPAddr(struct in_addr);
	IPAddr(unsigned long);

	IPAddr&		operator=(IPAddr &);
	IPAddr&		operator=(const char *);
	IPAddr&		operator=(struct in_addr);
	IPAddr&		operator=(unsigned long);

	friend int	operator==(IPAddr &i1, IPAddr &i2)
		{ return i1.valid && i2.valid && i1.a.s_addr == i2.a.s_addr; }
	friend int	operator==(IPAddr &i1, struct in_addr i2)
		{ return i1.valid && i1.a.s_addr == i2.s_addr; }
	friend int	operator==(IPAddr &i1, unsigned long i2)
		{ return i1.valid && i1.a.s_addr == i2; }

	inline unsigned int isValid(void);

	inline struct in_addr *getAddr(void);
	char		*getName(void);
	inline char	*getString(void);

protected:
	struct in_addr	a;
	unsigned int	valid;
};

//
//	IP Network Numbers
//
class IPNetNum : public IPAddr {
protected:
	unsigned long	inet_netof(struct in_addr);

public:
	IPNetNum(void);
	IPNetNum(IPNetNum &);
	IPNetNum(IPAddr &);
	IPNetNum(const char *);
	IPNetNum(struct in_addr);
	IPNetNum(unsigned long);

	IPNetNum&	operator=(IPNetNum &);
	IPNetNum&	operator=(IPAddr &);
	IPNetNum&	operator=(const char *);
	IPNetNum&	operator=(struct in_addr);
	IPNetNum&	operator=(unsigned long);

	void		setAddr(IPAddr &, IPAddr &);
	void		setAddr(IPAddr &, struct in_addr);
	void		setAddr(IPAddr &, unsigned long);
	void		setAddr(struct in_addr, IPAddr &);
	void		setAddr(struct in_addr, struct in_addr);
	void		setAddr(unsigned long, IPAddr &);
	void		setAddr(unsigned long, unsigned long);

	char		*getName(void);
	char		*getString(void);
};

//
//	DECnet Addresses
//
class DNAddr {
public:
	DNAddr(void);
	DNAddr(DNAddr &);
	DNAddr(PhysAddr &);
	DNAddr(char *);
	DNAddr(dn_addr);

	DNAddr&		operator=(DNAddr &);
	DNAddr&		operator=(PhysAddr &);
	DNAddr&		operator=(char *);
	DNAddr&		operator=(dn_addr);

	friend int	operator==(DNAddr &d1, DNAddr &d2)
		{ return d1.valid && d2.valid && d1.a == d2.a; }
	friend int	operator==(DNAddr &d1, dn_addr d2)
		{ return d1.valid && d1.a == d2; }

	inline unsigned int isValid(void);

	inline dn_addr	*getAddr(void);
	char		*getName(void);
	inline char	*getString(void);

private:
	dn_addr		a;
	unsigned short	valid;
};

//
//	DECnet Areas
//
class DNArea {
public:
	DNArea(void);
	DNArea(DNArea &);
	DNArea(DNAddr &);
	DNArea(PhysAddr &);
	DNArea(dn_addr);

	DNArea&		operator=(DNArea &);
	DNArea&		operator=(DNAddr &);
	DNArea&		operator=(PhysAddr &);
	DNArea&		operator=(dn_addr);

	friend int	operator==(DNArea &d1, DNArea &d2)
		{ return d1.valid && d2.valid && d1.a == d2.a; }

	inline unsigned int isValid(void);

	inline dn_addr	*getAddr(void);
	char		*getName(void);
	inline char	*getString(void);

private:
	dn_addr		a;
	unsigned short	valid;
};

//
//	Standard Tree Node
//
class CfNode {
public:
	enum cftoken	token;
	char		*name;
	CfNode		*child;
	CfNode		*parent;
	CfNode		*next;
	CfNode		*prev;
	void		*data;

	CfNode(void);
	CfNode(enum cftoken);
	CfNode(CfNode *, enum cftoken);

	~CfNode(void);

	void		setName(const char *);
	char		*getName(void)
		{ return name; }
};

//
//	Network
//
class NetworkNode : public CfNode {
public:
	IPNetNum	ipnum;
	IPAddr		ipmask;

	NetworkNode(void);
	NetworkNode(CfNode *);

	void		complete(void);
};

enum segtype {
	SEG_NULL,
	SEG_ETHERNET,
	SEG_FDDI,
	SEG_SLIP,
	SEG_UNKNOWN,
};

//
//	Segment
//
class SegmentNode : public CfNode {
public:
	IPNetNum	ipnum;
	DNAddr		dnarea;

	SegmentNode(void);
	SegmentNode(NetworkNode *);

	enum segtype	setType(const char *);
	enum segtype	setType(enum segtype t)
		{ return type = t; }

	enum segtype	getType(void)
		{ return type; }

	char		*printType(void);

	void		complete(void);

private:
	enum segtype	type;
};

//
//	Host
//
class HostNode : public CfNode {
public:
	HostNode(unsigned int = 0, unsigned int = 0);

	void		setNISserver(unsigned int n)
		{ NISserver = n; }
	unsigned int	isNISserver(void)
		{ return (unsigned int) NISserver; }

	void		setNISmaster(unsigned int n)
		{ NISmaster = n; }
	unsigned int	isNISmaster(void)
		{ return (unsigned int) NISmaster; }

private:
	unsigned short	NISserver;
	unsigned short	NISmaster;
};

//
//	Interface
//
class InterfaceNode : public CfNode {
public:
	InterfaceNode	*nextif;
	InterfaceNode	*previf;

	PhysAddr	physaddr;
	IPAddr		ipaddr;
	DNAddr		dnaddr;

	InterfaceNode(void);
	InterfaceNode(SegmentNode *);

	void		setHost(HostNode *h);

	void		setPhysAddr(const char *s)
		{ physaddr = s; }
	void		setIPAddr(const char *s)
		{ ipaddr = s; }
	void		setDNAddr(char *s)
		{ dnaddr = s; }

	void		complete(void);

	HostNode	*getHost(void)	
		{ return host; }

private:
	HostNode	*host;
};


// Inline Functions
unsigned int
PhysAddr::isValid(void)
{
    return (unsigned int) valid;
}

char *
PhysAddr::getAddr(void)
{
    return a;
}

char *
PhysAddr::getString(void)
{
    return valid ? ether_ntoa(a) : 0;
}

unsigned int
IPAddr::isValid(void)
{
    return valid;
}

struct in_addr *
IPAddr::getAddr(void)
{
    return &a;
}

char *
IPAddr::getString(void)
{
    return valid ? inet_ntoa(a) : 0;
}

unsigned int
DNAddr::isValid(void)
{
    return (unsigned int) valid;
}

dn_addr	*
DNAddr::getAddr(void)
{
    return &a;
}

char *
DNAddr::getString(void)
{
    return valid ? decnet_ntoa(decnet_htons(a)) : 0;
}

unsigned int
DNArea::isValid(void)
{
    return (unsigned int) valid;
}

dn_addr	*
DNArea::getAddr(void)
{
    return &a;
}

char *
DNArea::getString(void)
{
    return valid ? decnet_ntoa(decnet_htons(a)) : 0;
}

int cf_parse(struct __file_s *, CfNode *, int *, char *);
void cf_print(struct __file_s *, CfNode *);
#endif	/* _LANGUAGE_C_PLUS_PLUS */
#endif	/* !CF_H */
