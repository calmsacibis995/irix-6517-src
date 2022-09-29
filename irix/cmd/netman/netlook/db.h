#ifndef DB_H
#define DB_H
/*
 * Copyright 1991-1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Network Configuration include file
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

extern "C" {
#include <index.h>
}

class InterfaceDB {
public:
	InterfaceDB(void);
	InterfaceDB(CfNode *);
	~InterfaceDB(void);

	void		create(CfNode *);
	void		destroy(void);

	InterfaceNode	*match(PhysAddr &p, NetworkNode *n)
		{ return (InterfaceNode *) in_matchqual(paindex, &p,(long) n); }

	InterfaceNode	*match(IPAddr &i)
		{ return (InterfaceNode *) in_match(ipindex, &i); }

	InterfaceNode	*match(DNAddr &d)
		{ return (InterfaceNode *) in_match(dnindex, &d); }

	void		enter(InterfaceNode *);

	void		remove(InterfaceNode *);

private:
	Index		*paindex;
	Index		*ipindex;
	Index		*dnindex;

};

class SegmentDB {
public:
	SegmentDB(void);
	SegmentDB(CfNode *);
	~SegmentDB(void);

	void		create(CfNode *);
	void		destroy(void);

	SegmentNode	*match(IPNetNum &i)
		{ return (SegmentNode *) in_match(ipindex, &i); }

	SegmentNode	*match(DNArea &d)
		{ return (SegmentNode *) in_match(dnindex, &d); }

	void		enter(SegmentNode *);

	void		remove(SegmentNode *);

private:
	Index		*ipindex;
	Index		*dnindex;
};

class NetworkDB {
public:
	NetworkDB(void);
	NetworkDB(CfNode *);
	~NetworkDB(void);

	void		create(CfNode *);
	void		destroy(void);

	NetworkNode	*match(IPNetNum &i)
		{ return (NetworkNode *) in_match(ipindex, &i); }

	void		enter(NetworkNode *);

	void		remove(NetworkNode *);

private:
	Index		*ipindex;
};

#endif	/* !DB_H */
