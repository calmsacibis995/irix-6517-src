/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Sub-Agent Table
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

class asnObject;
class asnObjectIdentifier;
class subAgent;

/*
 * A node in the sub-agent table
 */
class subAgentNode {
	friend class subAgentTable;

private:
	subAgentNode *	parent;
	subAgentNode *	child;
	subAgentNode *	next;
	subID		subid;
	subAgent *	subagent;

public:
	subAgentNode(void);

	subAgent *	getSubAgent(void)
				{ return subagent; }

	void		print(void);
};

/*
 * Sub-agent Table
 */
class subAgentTable {
private:
	subAgentNode *	table;

	void		rprint(subAgentNode *san);

public:
	subAgentTable(void);
	~subAgentTable(void);

	subAgentNode *	lookup(asnObjectIdentifier *o);

	subAgentNode *	lookupNext(asnObjectIdentifier *o);

	subAgentNode *	add(asnObjectIdentifier *o, subAgent *sa);

	subAgentNode *	remove(asnObjectIdentifier *o);

	// Print out the table

	void		print(void)
				{ rprint(table); }
};
