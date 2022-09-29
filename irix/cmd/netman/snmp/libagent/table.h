#ifndef __TABLE_H__
#define __TABLE_H__
/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	MIB Table Support
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

class oid;
class asnObject;

/*
 * MIB Table Entries
 */
class mibTableEntry {
	friend class mibTable;

private:
	mibTableEntry *	parent;
	mibTableEntry *	child;
	mibTableEntry *	next;
	mibTableEntry *	prev;

	unsigned int	depth;
	asnObject *	value;
	subID 		subid;

	mibTableEntry(subID s, unsigned int d);
	~mibTableEntry(void);
};

/*
 * MIB Tables
 */
class mibTable {
private:
	unsigned int	start;
	unsigned int	end;
	mibTableEntry	table;

	unsigned int	match(oid *oi, mibTableEntry **e);

public:
	mibTable(unsigned int tablelen, unsigned int keylen);
	~mibTable(void);

	int		formNextOID(oid *);

	// Get the table entry for oi and return a pointer to in *a.
	// Return value is a SNMP error status.

	int		get(oid *oi, asnObject **a);

	// Set the table entry for oi to the value in pointed to by *a.
	// If *a is 0, then the entry is deleted.  Return value is a
	// SNMP error status.

	int		set(oid *oi, asnObject **a);
};
#endif /* __TABLE_H__ */
