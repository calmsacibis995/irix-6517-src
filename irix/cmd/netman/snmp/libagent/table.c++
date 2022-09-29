/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	MIB tables
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

#include <string.h>
#include <bstring.h>
#include "oid.h"
#include "asn1.h"
#include "snmp.h"
#include "table.h"

/*
 * MIB Table Entries
 */
mibTableEntry::mibTableEntry(subID s, unsigned int d)
{
    next = prev = parent = child = 0;
    subid = s;
    depth = d;
    value = 0;
}

mibTableEntry::~mibTableEntry(void)
{
    if (value != 0)
	delete value;
}


/*
 * MIB Tables
 */
mibTable::mibTable(unsigned int t, unsigned int k) : table(1, t)
{
    start = t;
    end = t + k - 1;
}

mibTable::~mibTable(void)
{
    // XXX - delete the table
    delete table.child;
}

unsigned int
mibTable::match(oid *oi, mibTableEntry **e)
{
    unsigned int len;
    subID *subid;
    int rc;
    mibTableEntry *t = &table;

    oi->getValue(&subid, &len);

    for (*e = t; t != 0 && t->depth < len; ) {
	rc = subid[t->depth] - t->subid;
	if (rc > 0) {
	    *e = t;
	    t = t->next;
	    continue;
	} else if (rc == 0) {
	    *e = t;
	    if (t->depth == end)
		return 1;
	    t = t->child;
	    continue;
	}
        break;
    }

    return 0;
}

int
mibTable::get(oid *oi, asnObject **a)
{
    mibTableEntry *e;
    if (!match(oi, &e))
	return SNMP_ERR_noSuchName;

    *a = e->value->dup();
    return SNMP_ERR_noError;
}

int
mibTable::set(oid *oi, asnObject **a)
{
    mibTableEntry *e;

    // If no match, create a new table entry
    if (!match(oi, &e)) {
	mibTableEntry *t;
	unsigned int len, d;
	subID *subid;

	// Return if bad length or if delete (*a == 0)
	if (e == 0 || *a == 0)
	    return SNMP_ERR_noSuchName;

	oi->getValue(&subid, &len);

	// Create new children
	if (e->depth != end) {
	    // Create a new node at this level if necessary
	    if (subid[e->depth] != e->subid) {
		t = new mibTableEntry(subid[e->depth], e->depth);
		t->prev = e;
		t->next = e->next;
		t->parent = e->parent;
		if (e->next != 0)
		    e->next->prev = t;
		e->next = t;
		e = t;
	    } else if (e->child != 0) {
		// Create a new node at the depth of our child, but before it
		d = e->depth + 1;
		t = new mibTableEntry(subid[d], d);
		t->parent = e;
		t->next = e->child;
		e->child->prev = t;
		e->child = t;
		e = t;
	    }

	    // Create child nodes
	    for (d = e->depth + 1; d <= end; d++) {
		t = new mibTableEntry(subid[d], d);
		t->parent = e;
		// t->child = e->child;
		e->child = t;
		e = t;
	    }
	    t->value = (*a)->dup();
	    return SNMP_ERR_noError;
	}

	// Create entry
	t = new mibTableEntry(subid[end], end);
	t->prev = e;
	t->next = e->next;
	t->parent = e->parent;
	t->value = (*a)->dup();
	if (e->next != 0)
	    e->next->prev = t;
	e->next = t;
	return SNMP_ERR_noError;
    }

    // If value is 0, delete
    if (*a == 0) {
	mibTableEntry *p;
	unsigned int done;
	
	for (done = 0; !done && e != &table; ) {
	    if (e->next != 0) {
		e->next->prev = e->prev;
		done = 1;
	    }
	    if (e->prev != 0) {
		e->prev->next = e->next;
		done = 1;
	    } else
		e->parent->child = e->next;

	    p = e->parent;
	    delete e;
	    e = p;
	}
	return SNMP_ERR_noError;
    }

    // Set the new value deleting the old one if necessary
    if (e->value->getTag() == (*a)->getTag())
	e->value->setValue(*a);
    else {
	delete e->value;
	e->value = (*a)->dup();
    }
    return SNMP_ERR_noError;
}

int
mibTable::formNextOID(oid *oi)
{
    unsigned int len;
    subID *subid, *s;
    mibTableEntry *e;

    // Check for empty table
    if (table.child == 0)
	return SNMP_ERR_noSuchName;

    oi->getValue(&subid, &len);

    // Find pointer to next mibTableEntry
    match(oi, &e); 
    if (e->depth == end) {
	// Go over and up if necessary
	for ( ; ; ) {
	    if (e->next != 0) {
		e = e->next;
		break;
	    }
	    e = e->parent;
	    if (e == 0)
		return SNMP_ERR_noSuchName;	// End of table
	}
    }

    // Go down
    for ( ; e->depth != end; e = e->child)
	;
    
    // Fix oid
    if (len == end + 1)
	s = subid;
    else {
	len = end + 1;
	s = new subID[len];
	bcopy(subid, s, start * sizeof(subID));
    }
    for (len--; len >= start; len--) {
	s[len] = e->subid;
	e = e->parent;
    }
    if (s != subid) {
	oi->setValue(s, end + 1);
	delete s;			// Avoid a memory leak
    }

    return SNMP_ERR_noError;
}
