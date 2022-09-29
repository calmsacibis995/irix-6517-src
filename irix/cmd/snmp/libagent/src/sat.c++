/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Sub-Agent Table
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

#include <stdio.h>
#include "oid.h"
#include "asn1.h"
#include "sat.h"

/*
 * SubAgentNode
 */
subAgentNode::subAgentNode(void)
{
    child = next = 0;
    subid = 0;
    subagent = 0;
}

void
subAgentNode::print(void)
{
    printf("subAgentNode: %#0lx\n", this);
    printf("    parent:   %#0lx\n", parent);
    printf("    child:    %#0lx\n", child);
    printf("    next:     %#0lx\n", next);
    printf("    subID:    %d\n", subid);
    if (subagent != 0)
	printf("    subAgent: %#0lx\n", subagent);
}


/*
 * Sub-agent Table
 */
subAgentTable::subAgentTable(void)
{
    table = 0;
}

subAgentTable::~subAgentTable(void)
{
    if (table != 0)
	delete table;
}

subAgentNode *
subAgentTable::lookup(asnObjectIdentifier *o)
{
    unsigned int len;
    subID *sid;
    oid *oi = o->getValue();
    if (oi == 0)
	return 0;
    oi->getValue(&sid, &len);

    // Walk down table matching subid at each level
    for (subAgentNode *san = table; len != 0; len--, sid++, san = san->child) {
	// Search for matching subid at this level
	for ( ; ; san = san->next) {
	    if (san == 0 || san->subid > *sid)
		return 0;
	    if (san->subid == *sid) {
		// Matched, if there is a subagent, return
		if (san->subagent != 0)
		    return san;
		break;
	    }
	}
    }

    return 0;
}

subAgentNode *
subAgentTable::lookupNext(asnObjectIdentifier *o)
{
    unsigned int len;
    subID *sid;
    oid *oi = o->getValue();
    if (oi == 0)
	return 0;
    oi->getValue(&sid, &len);

    // Walk down table matching subid at each level
    subAgentNode *san, *parent;
    subID tmp[OID_MAX_LENGTH];
    unsigned int tlen = 0;
    unsigned int matched = 0;
    for (san = table, parent = 0; san != 0 && len != 0; ) {
	if (san->subid < *sid) {
	    san = san->next;
	    continue;
	}
	if (san->subid == *sid) {
	    // Matched, if there is a subagent, return
	    if (san->subagent != 0) {
		// Copy over any remaining subids
		for ( ; len != 0; len--)
		    tmp[tlen++] = *sid++;
		matched = 1;
		break;
	    }
	    parent = san;
	    san = san->child;
	    tmp[tlen++] = *sid++;
	    len--;
	    continue;
	}
	break;
    }

    if (!matched) {
	// Find the next subagent
	for ( ; ; ) {
	    if (san == 0) {
		// Got to the end of the line, go back up and try again
		san = parent;
		if (san == 0)
		    return 0;
		parent = san->parent;
		san = san->next;
		tlen--;
	    } else {
		if (san->subagent != 0)
		    break;
		tmp[tlen++] = san->subid;
		parent = san;
		san = san->child;
	    }
	}
        tmp[tlen++] = san->subid;
    }

    // Fix object identifier
    oi->setValue(tmp, tlen);
    return san;
}

subAgentNode *
subAgentTable::add(asnObjectIdentifier *o, subAgent *sa)
{
    unsigned int len;
    subID *sid;
    oid *oi = o->getValue();
    if (oi == 0)
	return 0;
    oi->getValue(&sid, &len);
    if (len == 0)
	return 0;

    subAgentNode **san, *parent;
    for (san = &table, parent = 0; ; parent = *san, san = &((*san)->child)) {
	for ( ; *san != 0; san = &((*san)->next)) {
		if ((*san)->subid >= *sid)
		    break;
	}
	if (*san == 0 || (*san)->subid != *sid) {
	    // Add a new node
	    subAgentNode *n = new subAgentNode;
	    n->subid = *sid;
	    n->parent = parent;
	    n->next = *san;
	    *san = n;
	}
	if (--len == 0)
	    break;
	sid++;
    }

    // Fill in the info
    (*san)->subagent = sa;
    return *san;
}

subAgentNode *
subAgentTable::remove(asnObjectIdentifier *o)
{
    unsigned int len;
    subID *sid;
    oid *oi = o->getValue();
    if (oi == 0)
	return 0;
    oi->getValue(&sid, &len);
    if (len == 0)
	return 0;

    for (subAgentNode **san = &table; ; sid++, san = &((*san)->child)) {
	for ( ; *san != 0; san = &((*san)->next)) {
		if ((*san)->subid >= *sid)
		    break;
	}
	if (*san == 0 || (*san)->subid != *sid)
	    return 0;
	if (--len == 0)
	    break;
    }

    // XXX - Be lazy and just zero it
    (*san)->subagent = 0;
    return *san;
}

void
subAgentTable::rprint(subAgentNode *san)
{
    if (san == 0)
	return;
    san->print();
    printf("----------\n");
    rprint(san->child);
    if (san->next != 0) {
	printf("##########\n");
	rprint(san->next);
    }
}
