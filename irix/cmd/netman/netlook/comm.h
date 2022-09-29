/*
 * Copyright 1989-1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Communications class
 *
 *	$Revision: 1.4 $
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
#include "cf.h"

class Node;
struct alpacket;
struct alentry;

class Comm {
public:
	Comm(unsigned int buffer, unsigned int interval);
	~Comm(void);

	unsigned int open(Node *);
	unsigned int close(Node *);

	unsigned int changeFilter(Node *);
	unsigned int read(void);

	void setFilter(const char *c);
	inline char *getFilter(void);

	inline Node *getSnoopList(void);

	inline Node *getBadNode(void);

protected:
	unsigned int decode(struct alpacket *);
	unsigned int packetDecode(InterfaceNode *, struct alentry *);
	unsigned int get(NetworkNode *, SegmentNode *, InterfaceNode *,
			 PhysAddr &, IPAddr &, char *,
			 NetworkNode **, SegmentNode **, InterfaceNode **);
	unsigned int connect(Node *, Node *, struct counts *);
	unsigned int checkGateway(IPAddr&, NetworkNode *, InterfaceNode *,
				     struct counts *);
private:
	fd_set openfds;
	struct timeval timeout;
	Node *snoopList;
	Node *badNode;

	unsigned int buflen;
	unsigned int interval;
	unsigned int snooplen;

	struct alpacket	*alp;
	unsigned int interrupt;

	char filter[256];
};

// Inline functions
char *
Comm::getFilter(void)
{
    return filter;
}

Node *
Comm::getSnoopList(void)
{
    return snoopList;
}

Node *
Comm::getBadNode(void)
{
    return badNode;
}
