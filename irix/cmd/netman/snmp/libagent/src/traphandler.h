#ifndef _SNMP_TRAP_HANDLER_H_
#define _SNMP_TRAP_HANDLER_H_
/*
 * Copyright 1994 Silicon Graphics, Inc.  All rights reserved.
 *
 *	SNMP  Agent Trap Handler
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
#include <sys/param.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "oid.h"
#include "pdu.h"
#include "packet.h"
#include "message.h"

#define SNMP_TRAP_CONF_FILE	"/etc/snmpd.trap.conf"
#define TMP_SNMP_TRAP_CONF_FILE	"/etc/snmpd.trap.conf.tmp"
#define TRAP_FILE_MAX_LINE	256
#define MAX_COMM_LEN		32

#define SNMP_TRAP_COLD_START	0
#define SNMP_TRAP_WARM_START	1
#define SNMP_TRAP_LINK_DOWN	2
#define SNMP_TRAP_LINK_UP	3
#define SNMP_TRAP_AUTH		4
#define SNMP_TRAP_EGP_LOSS	5
#define SNMP_TRAP_ENTERPRISE	6
#define MAX_TRAPS		(SNMP_TRAP_ENTERPRISE + 1)

// Entries to specify whether to send authentication failure traps.
#define ENABLE_AUTHEN_TRAPS	1
#define DISABLE_AUTHEN_TRAPS	2

class trapDestList {
public:
    trapDestList(void);
    trapDestList(struct in_addr);
    inline trapDestList *getNext(void);
    inline struct in_addr *getAddr(void);
    void append(struct in_addr a);
    void appendUnique(struct in_addr a);	// For HP-UX MIB trap table
private:
    struct in_addr dst;
    struct trapDestList *next;
};

class trapcb {
public:
    trapcb(void);
    ~trapcb(void);
    inline void appendDest(struct in_addr dest);
    inline void appendUniqueDest(struct in_addr dest);
    inline void appendVar(varBind *v);
    inline void clear(void);
    inline struct trapDestList *getDestList(void);
private:
    trapDestList dlist;
    varBindList vbl;
};

class snmpTrapHandler {
public:
    snmpTrapHandler(void);
    int send(unsigned int trap,  varBindList *vbl = 0);
    void dumpTrapDestLists();
    struct in_addr *getFirstUniqueAddr();
    struct in_addr *getNextUniqueAddr(struct in_addr *);
    inline unsigned int getTrapCount();
    inline unsigned int getUniqueTrapCount();

private:
    void readCfg(void);
    snmpMessageHandler *mh;
    int sock;
    unsigned long myaddress;
    char trapcomm[MAX_COMM_LEN +1];
    trapcb    traps[MAX_TRAPS +1];
    trapcb    unique;		// To keep lexicographic sort of unique
				// trap destinations for HP-UX MIB
    unsigned int trapCount;
    unsigned int uniqueTrapCount;
};

// inline functions
void
trapcb::appendVar(varBind *v) {
    vbl.appendVar(v);
}
void
trapcb::clear(void) {
    vbl.clear();
}

struct trapDestList *
trapcb::getDestList(void) {
    return &dlist;
}

void
trapcb::appendDest(struct in_addr dst) {
    dlist.append(dst);
}

void
trapcb::appendUniqueDest(struct in_addr dst) {	// For HP-UX MIB trap table
    dlist.appendUnique(dst);
}

trapDestList *
trapDestList::getNext(void) {
    return next;
}

struct in_addr *
trapDestList::getAddr(void) {
    return &dst;
}

unsigned int 
snmpTrapHandler::getTrapCount() {
    return trapCount;
}

unsigned int 
snmpTrapHandler::getUniqueTrapCount() {
    return uniqueTrapCount;
}


#endif
