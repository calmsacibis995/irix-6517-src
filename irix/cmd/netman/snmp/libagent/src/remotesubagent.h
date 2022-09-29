#ifndef _SNMP_REMOTE_SUB_AGENT_H_
#define _SNMP_REMOTE_SUB_AGENT_H_
/*
 * Copyright 1994 Silicon Graphics, Inc.  All rights reserved.
 *
 *	SNMP Remote Sub Agent
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
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "oid.h"
#include "pdu.h"
#include "packet.h"
#include "query.h"

#define REMOTE_SUB_AGENT_FILE	"/etc/snmpd.remote.conf"
#define REMOTE_SUB_AGENT_FILE_MAX_LINE	256
#define REMOTE_VB	1

class remoteSubAgent {
public:
    remoteSubAgent(void);
    remoteSubAgent(char *saroot, char *saaddr, char *saport,
		    char *timeout, char *saname = NULL);
    remoteSubAgent(remoteSubAgent *rsa, int copyVarBindList = 0);
    ~remoteSubAgent(void);
    char *getString(void);
    inline oid *getRoot(void);
    inline void setRoot(oid *asnoid);
    inline struct in_addr *getAddr(void);
    inline void setAddr(char *);
    inline int getPort(void);
    inline void setPort(int p);
    inline char *getName(void);
    inline void setName(char *n);
    inline varBindList *getVarBindList(void);
    inline void appendVarBind(varBind *vb);
    inline int getErrstat(void);
    inline void setErrstat(int e);
    inline unsigned int getTimeout(void);
    inline void setTimeout(unsigned int t);
    inline void appendVar(varBind *vb);
    int operator==(remoteSubAgent &r);
private:
    oid	    root;
    struct in_addr    address;
    int	    port;
    char    *name;
    unsigned int timeout;
    int errstat;
    varBindList vbl;
};

class remoteSubAgentList {
public:
    remoteSubAgentList(void);
    remoteSubAgentList(remoteSubAgent *r);
    ~remoteSubAgentList(void);
    void append(remoteSubAgent *rsa);
    void remove(remoteSubAgent *rsa);
    inline remoteSubAgentList *getNext(void);
    inline remoteSubAgent *getRSA(void);
    remoteSubAgentList *find(varBind *vb);
    remoteSubAgentList *find(remoteSubAgent *rsa);
    void clear(int destroyRSAvbl = 0);
private:
    remoteSubAgent *rsa;
    remoteSubAgentList *next;
    
};
class remoteSubAgentHandler {
public:
    remoteSubAgentHandler(void);
    ~remoteSubAgentHandler(void);
    int checkVb(varBind *vb);
    int doRequests(snmpPacket &req, getResponsePDU &respdu,
		    int currerrstat, unsigned int *varcount, int *ttl);
    void clean(int destroyVarBinds = 0);
private:
    int readCfg(void);
    int count;
    remoteSubAgentList rsal;
    remoteSubAgentList reqlist;
    snmpQueryHandler qh;
};


// inline functions
remoteSubAgentList *
remoteSubAgentList::getNext(void) {
    return next;
}

remoteSubAgent *
remoteSubAgentList::getRSA(void) {
    return rsa;
}
oid *
remoteSubAgent::getRoot(void) {
    return &root;
}
void 
remoteSubAgent::setRoot(oid *asnoid){
    root = *asnoid;
}
struct in_addr *
remoteSubAgent::getAddr(void) {
    return &address;
}
void 
remoteSubAgent::setAddr(char *a) {
    if (!a || !inet_aton(a, &address))
	bzero(&address, sizeof address);
}
int 
remoteSubAgent::getPort(void) {
    return port;
}
void 
remoteSubAgent::setPort(int p) {
    port = p;
}
char *
remoteSubAgent::getName(void) {
    return name;
}
void
remoteSubAgent::setName(char *n) {
    if (name) {
	delete name;
    }
    if (n) {
	name = strdup(n);
    }
    else {
	name = NULL;
    }
}

int
remoteSubAgent::getErrstat(void) {
    return errstat;
}

void
remoteSubAgent::setErrstat(int e) {
    errstat = e;
}

void
remoteSubAgent::setTimeout(unsigned int t) {
    timeout = t;
}

unsigned int
remoteSubAgent::getTimeout(void) {
    return timeout;
}

varBindList *
remoteSubAgent::getVarBindList(void) {
    return &vbl;
}


void
remoteSubAgent::appendVar(varBind *vb) {
    vbl.appendVar(vb);
}
#endif
