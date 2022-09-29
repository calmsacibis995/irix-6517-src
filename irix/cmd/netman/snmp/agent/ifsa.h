/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Interfaces sub-agent
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

class mibTable;

class ifSubAgent : public subAgent {
	friend void updateIfTable(void);
	friend void updateIpAddrTable(void);
	friend void updateIpRouteTable(void);

private:
	asnObjectIdentifier	subtree;

	unsigned int		ifnumber;
	mibTable *		iftable;

public:
	ifSubAgent(void);
	~ifSubAgent(void);

	// RMON subagent needs the ifname from the ifindex

	asnOctetString *	getIfName(unsigned int);

	virtual int	get(asnObjectIdentifier *, asnObject **, int *);
	virtual int	getNext(asnObjectIdentifier *, asnObject **, int *);
	virtual int	set(asnObjectIdentifier *, asnObject **, int *);
};
