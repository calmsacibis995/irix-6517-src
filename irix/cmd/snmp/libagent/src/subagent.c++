/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Sub-agent
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

#include <sys/types.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdio.h>
#include <bstring.h>
#include <string.h>
#include "oid.h"
#include "asn1.h"
#include "snmp.h"
#include "pdu.h"
#include "packet.h"
#include "subagent.h"
#include "sat.h"
#include "agent.h"
#include "table.h"

extern snmpAgent agent;

subAgent::subAgent(void)
{
    mtl = 0;
}

subAgent::~subAgent(void)
{
    mibTableList *m;

    while (mtl != 0) {
	m = mtl->next;
	delete mtl;
	mtl = m;
    }
}

void
subAgent::log(int level, int error, char *format, ...)
{
    extern int exc_level;
    extern void (*exc_vplog)(int, int, char *, va_list);
    va_list ap;

    if (level <= exc_level) {
	va_start(ap, format);
	(*exc_vplog)(level, error, format, ap);
	va_end(ap);
    }
}

int
subAgent::endOfMib(oid *oi)
{
    subID *subid, tmp[OID_MAX_LENGTH];
    unsigned int len;

    oi->getValue(&subid, &len);
    bcopy(subid, tmp, sublen * sizeof(subID));
    tmp[sublen - 1]++;
    oi->setValue(tmp, sublen);
    return SNMP_ERR_noSuchName;
}

int
subAgent::formNextOID(oid *oi, subID last)
{
    unsigned int len;
    subID *subid, tmp[OID_MAX_LENGTH];

    oi->getValue(&subid, &len);

    if (len == sublen) {
	// If the oid is just the subtree, add 1.0
	bcopy(subid, tmp, len * sizeof(subID));
	tmp[len++] = 1;
	tmp[len++] = 0;
	oi->setValue(tmp, len);
	return SNMP_ERR_noError;
    }

    // Check if next is beyond end of MIB
    if (subid[sublen] > last)
	return endOfMib(oi);

    // Check if in a table
    for (mibTableList *m = mtl; m != 0; m = m->next) {
	if (m->subid > subid[sublen])
	    break;
	if (m->subid == subid[sublen]) {
	    // Update the table
	    if (m->update != 0)
		(*m->update)();

	    int rc = m->table->formNextOID(oi);
	    if (rc == SNMP_ERR_noError)
		return rc;
	    break;
	}
    }

    // Do fast case first
    if (len == sublen + 2) {
	// If the oid is the subtree + 2 subids, just fix inline
	subid[sublen]++;
	subid[sublen + 1] = 0;
	return SNMP_ERR_noError;
    }

    if (len == sublen + 1) {
	// If the oid is the subtree + 1 subid, add 0
	bcopy(subid, tmp, len * sizeof(subID));
	tmp[len++] = 0;
	oi->setValue(tmp, len);
	return SNMP_ERR_noError;
    }

    // If the oid is the subtree + (> 2) subids, recreate with next
    len = sublen + 2;
    bcopy(subid, tmp, len * sizeof(subID));
    tmp[sublen] = subid[sublen] + 1;
    tmp[sublen + 1] = 0;
    oi->setValue(tmp, len);
    return SNMP_ERR_noError;
}

int
subAgent::simpleGetNext(asnObjectIdentifier *o, asnObject **a,
			int *t, subID last)
{
    int rc;
    oid *oi = o->getValue();

    for ( ; ; ) {

	// Form the next object identifier
	rc = formNextOID(oi, last);
	if (rc != SNMP_ERR_noError)
	    break;

	// Get the value for the new identifier
	rc = get(o, a, t);
	if (rc != SNMP_ERR_noSuchName)
	    break;
    }
    return rc;
}

mibTable *
subAgent::table(subID subid, unsigned int keylen, void (*func)(void))
{
    // Place this table into the list
    for (mibTableList **m = &mtl; *m != 0; m = &((*m)->next)) {
	if ((*m)->subid > subid)
	    break;
    }
    mibTableList *n = new mibTableList;
    n->next = *m;
    n->subid = subid;
    n->update = func;
    n->table = new mibTable(sublen + 1, keylen + 2);
    *m = n;
    return n->table;
}

int
subAgent::export(char *n, asnObjectIdentifier *o)
{
    // Fill in the sublen
    subID *subid;
    oid *oi = o->getValue();
    oi->getValue(&subid, &sublen);

    // Fill in the name
    name = new char[strlen(n) + 1];
    strcpy(name, n);

    return agent.import(o, this);
}

int
subAgent::unexport(asnObjectIdentifier *o)
{
    return agent.unimport(o);
}

int
subAgent::trap(int, int, varBindList *)
{
    printf("trap not yet implemented\n");
    return SNMP_ERR_genErr;
}

int
subAgent::get(asnObjectIdentifier *o, asnObject **, int *)
{
    char *s = o->getString();
    printf("subagent get: %s\n", s);
    delete s;
    return SNMP_ERR_noSuchName;
}

int
subAgent::getNext(asnObjectIdentifier *o, asnObject **, int *)
{
    char *s = o->getString();
    printf("subagent getNext: %s\n", s);
    delete s;
    return SNMP_ERR_noSuchName;
}

int
subAgent::set(asnObjectIdentifier *o, asnObject **a, int *)
{
    char *s = o->getString();
    char *v = (*a)->getString();
    printf("subagent set: %s = %s\n", s, v);
    delete s;
    delete v;
    return SNMP_ERR_noSuchName;
}
