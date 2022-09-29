#ifndef __SUBAGENT_H__
#define __SUBAGENT_H__
/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Sub-agents
 *
 *	$Revision: 1.2 $
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
class varBindList;
class mibTable;

class mibTableList {
	friend class subAgent;

private:
	mibTableList *	next;
	subID		subid;
	void 		(*update)(void);
	mibTable *	table;
};


/*
 * Sub-agent
 */
class subAgent {
private:
	mibTableList *	mtl;

public:
	char *		name;		// Name of the subagent
	unsigned int	sublen;		// Length from the root


	subAgent(void);
	virtual ~subAgent(void);

	// Useful routines that a derived class may call

	// Log a message to the agent.

	void		log(int level, int error, char *format, ...);

	// When a getNext goes over the end of the MIB supported by
	// the subagent, return the value of this routine.  Pass to
	// it a pointer to an oid of the request.

	int		endOfMib(oid *oi);

	// Form the next object identifier after the given one, assuming
	// no tables.  Pass the current oid and the last subid that
	// the subagent handles within its subtree.  If SNMP_ERR_noSuchName
	// is returned, the end of the MIB was reached.

	int		formNextOID(oid *oi, subID last);

	// A subagent can use this routine to handle tables.  Pass the
	// subID of the table and the length of the key used.

	mibTable *	table(subID id, unsigned int keylen, void (*f)(void));

	// A simple getNext for subagents that do not handle tables.
	// The variables are the same as getNext plus one more.
	// Pass the last subid that the subagent handles within its subtree.

	int		simpleGetNext(asnObjectIdentifier *o, asnObject **a,
				      int *t, subID last);

	// Export a variable to the agent.  The char * is the name of the
	// subagent.  The object identifier o is the root of the subtree
	// that is supported by this agent.

	int		export(char *n, asnObjectIdentifier *o);
	int		unexport(asnObjectIdentifier *o);

	// Send a trap message.  The integer generic is the generic trap
	// code as listed in RFC 1157.  The integer specific is the
	// specific code.  Any interesting variables are passed
	// through vbl.

	int		trap(int generic, int specific, varBindList *vbl);

	// Agent call backs.  The return values represent error codes.

	// Get the variable o and return the value through a.  The value
	// of t tells the agent how long this variable value is good for.

	virtual int	get(asnObjectIdentifier *o, asnObject **a, int *t);

	// For the getNext request, the object identifier o should be
	// modified to the variable returned in a.  The value of t
	// tells the agent how long this variable value is good for.

	virtual int	getNext(asnObjectIdentifier *o, asnObject **a, int *t);

	// Set the object identifier o to the value of a.  The value of t
	// tells the agent how long this variable value is good for.

	virtual int	set(asnObjectIdentifier *o, asnObject **a, int *t);
};

#if 0
class localSubAgent : public subAgent {
private:
	int		sock;
public:
	localSubAgent();
	~localSubAgent();

	// Service a pending request.  If no request is pending, this call
	// will block.

	int		service(void);

	// Returns non-zero if a request is pending.

	int		pending(void);

	// Return the socket used for communication.  This can be used in
	// a call to select to determine when a request is pending.

	int		getSocket(void);
};
#endif

#endif /* __SUBAGENT_H__ */
