/*
 * Copyright 1994 Silicon Graphics, Inc.  All rights reserved.
 *
 *	SNMP Remote Agent
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
#include <sys/syslog.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <bstring.h>
#include <stdio.h>
#include <unistd.h>
#include <syslog.h>
#include <stdlib.h>
#include "oid.h"
#include "asn1.h"
#include "snmp.h"
#include "pdu.h"
#include "packet.h"
#include "message.h"
#include "query.h"
#include "remotesubagent.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

extern "C" {
#include "exception.h"
};

remoteSubAgentHandler::remoteSubAgentHandler(void) {
    count= readCfg(); 
}

remoteSubAgentHandler::~remoteSubAgentHandler(void) {
    
}

int
remoteSubAgentHandler::readCfg(void) {
    FILE *sf;
    char buf[REMOTE_SUB_AGENT_FILE_MAX_LINE];
    if ((sf = fopen(REMOTE_SUB_AGENT_FILE, "r")) == NULL) {
	exc_errlog(LOG_WARNING, errno,  
	    "readCfg: Cannot open remote sub agent config file %s: ",
	    REMOTE_SUB_AGENT_FILE);
	return 0;
    }
    exc_errlog(LOG_DEBUG, 0, 
	"readCfg: Opened remote sub agent file '%s'",
	REMOTE_SUB_AGENT_FILE);
    int linecount = 0;
    int rsacount = 0;

    while ((fgets(buf, REMOTE_SUB_AGENT_FILE_MAX_LINE, sf)) != NULL) {
	char *tok = NULL; char *timeout = NULL;
	char *tree = NULL, *addr = NULL, *port = NULL, *name = NULL;
	linecount++;
	char *line = strdup(buf);
	tok = strtok(line,  " \t\n");
	if((tok != NULL && tok[0] == '#')
		||
	    (tok == NULL)) {
	    delete line;
	    continue;
	}

	if (tok) {
	    tree = strdup(tok);
	    tok = strtok(NULL, " \t\n");
	    if (tok) {
		addr = strdup(tok);
		tok = strtok(NULL, " \t\n");
		if (tok) {
		    port = strdup (tok);
		    tok = strtok(NULL, " \t\n");
		    if (tok) {
			timeout = strdup (tok);
			tok = strtok(NULL, " \t\n");
		    }
		    else {
			exc_errlog(LOG_WARNING, 0, 
			    "readCfg: Bad remote sub agent config file format at %d: %s; no timeout", 
			    linecount, buf);
			delete line;
			delete tree;
			delete addr;
			delete port;
			continue;
		    }
		    if (tok) {
			name = strdup(tok);
		    }
		}
		else {
		    exc_errlog(LOG_WARNING, 0,
			"readCfg: Bad remote sub agent config file format at %d: %s: no port", 
			linecount, buf);
		    delete line;
		    delete tree;
		    delete addr;
		    continue;
		}
	    }
	    else {
		exc_errlog(LOG_WARNING, 0,
		    "readCfg: Bad remote sub agent config file format at %d: %s: no address", 
		    linecount, buf);
		delete line;
		delete tree;
		continue;
	    }
	}
	else {
	    exc_errlog(LOG_WARNING, 0, 
		"readCfg: Bad remote sub agent config file format at %d: %s: No mib tree", 
		linecount, buf);
	    delete line;
	    continue; 
	}
	delete line;
	
	/*
	 * At this point I have a good line from which to create a remote
	 * subagent list element
	 */
	rsacount++;
	exc_errlog(LOG_DEBUG, 0,
	    "readCfg: Found remote sub agent %d: %s	%s  %s	%s  %s", 
	    rsacount, tree, addr, port, timeout,  name);
	rsal.append(new remoteSubAgent(tree, addr, port, timeout, name));
	delete tree;
	delete addr;
	delete port;
	delete timeout;
	if (name)
	    delete name;
    }
   fclose(sf);
   return rsacount; 
}

int
remoteSubAgentHandler::checkVb(varBind *vb) {
    if (vb == NULL) {
	return 0;
    }
    
    // See if this varbind belongs to a remote sub agent.
    remoteSubAgentList *rsl = rsal.find(vb);
    if (rsl == NULL) {
	return 0;
    }
     //Now see if the remote subagent already is already part of the 
    //agent list for this request
    
    remoteSubAgent *reqrsa, *rsa = rsl->getRSA();
    remoteSubAgentList *reql = reqlist.find(rsa);
    if (reql == NULL) {
	reqrsa = new remoteSubAgent(rsa); //XXX make a copy
	reqlist.append(reqrsa); 
    }
    else {
	reqrsa = reql->getRSA();
    }
   // append the varbind to the remote subagent varbind list
    // The rsa is already on the list remote sub agent list
    reqrsa->appendVar(vb);
#ifdef DEBUG
    char *s, *s2;
    exc_errlog(LOG_DEBUG, 0, "checkVb: appended varbind %s to rsa %s", 
	s = vb->getString(),
	s2 = reqrsa->getString());
    delete s; delete s2;
#endif
    return 1;

}

int
remoteSubAgentHandler::doRequests(snmpPacket &req, getResponsePDU &respdu,
				  int currerrstat, unsigned int */*varcount*/,
				  int */*ttl*/) {
    
    int errstat = currerrstat;
    int agentErrors = 0;

    snmpPDU *reqpdu = (snmpPDU *) req.getPDU(); 
    qh.setTries(1);	    // let the original requestor do retries
    int commlen = req.getCommunity()->getLength();
    char *newcomm = new char[commlen+1];
    newcomm[commlen] = '\0';
    strncpy(newcomm, req.getCommunity()->getValue(), commlen);
    qh.setCommunity(newcomm);
    delete newcomm;
    unsigned int type =  reqpdu->getType();
    int reqID = reqpdu->getRequestId();
    for (remoteSubAgentList *rl = reqlist.getNext(); rl != NULL; rl = rl->getNext()) {
	varBindList *result = new varBindList;
	remoteSubAgent *rsa = rl->getRSA();
	varBindList *vbl = rsa->getVarBindList();
	char reqtype[16];
	qh.setInterval(rsa->getTimeout());
	qh.setDestPort(rsa->getPort());
	int errIdx = 0;
	char *s;
	switch (type) {
	    case SNMP_GetRequest:
		sprintf(reqtype, "Get");
		exc_errlog(LOG_DEBUG, 0, "doRequests: making Get req with vbl %s to %s", 
			    s = vbl->getString(), inet_ntoa(*(rsa->getAddr())));
		delete s;	    
		errstat = qh.get(rsa->getAddr(), vbl, result,&errIdx,  reqID);
	    break;
	    case SNMP_GetNextRequest:
		sprintf(reqtype, "GetNext");
		errstat = qh.getNext(rsa->getAddr(), vbl, result, &errIdx, reqID);
	    break;
	    case SNMP_SetRequest:
		sprintf(reqtype, "Set");
		errstat = qh.set(rsa->getAddr(), vbl, result, &errIdx, reqID);
	    break;
	    default:
		exc_errlog(LOG_WARNING, 0,
			    "doRequests: remoteSubAgentHandler: unsupported PDU");
		continue;
		//XXX do I need to clean up?
		
	    break;	// Left here for safety
	}
	exc_errlog(LOG_DEBUG, 0, "doRequests: remote sub agent  request(%s) status %d", 
		    reqtype, errstat);
		    
	if (errstat <= SNMP_ERR_genErr) {
	    // the query handler got a response which may or may not 
	    // have an error indication. In all cases move the results
	    // if there are any to the response varBindList (and pdu)
	    for (varBindList *vs = result->getNext(), *vd = vbl->getNext();
		 (vs != NULL) && (vd != NULL);
		  vs = vs->getNext(), vd = vd->getNext()) {
		vd->getVar()->setObjectValue(vs->getVar()->moveObjectValue(), 1);
		if (type == SNMP_GetNextRequest) {
		    vd->getVar()->setObject(vs->getVar()->moveObject(), 1);
		 }   
	    }
	    exc_errlog(LOG_DEBUG, 0, "doRequests: remote sub agent remote response: %s", 
			s = vbl->getString());
	    delete s;
	    // Now if there is an error index indicated find which varBind
	    // Use vbl which is the varbindlist attached to the subagent that
	    // points back into the resp pdu varbindlist. Should only need
	    // to check errIdx because packets with no error should have it
	    // set to 0. But I'm paranoid. Also check errIdx because some errors
	    // will have an error index set and others won't
	    if ((errIdx != 0) && (errstat != SNMP_ERR_noError)) {
		int varpos = 1;
		for (varBindList *vl = vbl->getNext(); vl != 0; vl = vl->getNext()) {
		    if (varpos == errIdx)
			break;
		    varpos++;
		}
		//when the above for loop terminates I should be looking at
		//the actual varBind in error
		//get its address
		varBind *varInError = vl->getVar();
		// now run through the entire varbind list of the pdu looking
		// for this varBind. A match is when the varBinds have the
		// same address. XXX This is ugly! 
		varBindList *respVarList = respdu.getVarList();
		int vcount = 1;
		for (varBindList *vlist = respVarList->getNext(); vlist != 0;
		     vlist = vlist->getNext()) {
		    if (vlist->getVar() == varInError)
			break;
		    vcount++;
		}
		respdu.setErrorIndex(vcount);
	    }
	    
	}
	else {
	    errstat = SNMP_ERR_genErr;
	    respdu.setErrorIndex(0); 
	}
	respdu.setErrorStatus(errstat);
	agentErrors++;
	
	result->clear(1);
	delete result;
	
    }
    return (errstat);			      
}

void
remoteSubAgentHandler::clean(int destroyVarBinds) {
    reqlist.clear(destroyVarBinds);
}
remoteSubAgentList::remoteSubAgentList(void) {
    rsa = 0;
    next = 0;
}

remoteSubAgentList::remoteSubAgentList(remoteSubAgent *r) {
    rsa = r;
    next = 0;
}

remoteSubAgentList::~remoteSubAgentList(void) {
    delete rsa;
}

remoteSubAgentList *
remoteSubAgentList::find(varBind *vb) {
    for (remoteSubAgentList *l = this->next; l != NULL; l = l->next) {
	oid *o = l->rsa->getRoot();
	unsigned int len, len2;
	subID *id,  *id2;
	o->getValue(&id, &len);
	oid *o2 = vb->getObject()->getValue();
	o2->getValue(&id2,  &len2);
	/*
	 * I compare the two subIds just for the len of the root
	 * I have for the subagent. The actual subid from the varbind
	 * will be longer for the actual variable.
	 */
	if (bcmp (id, id2, len *sizeof (subID)) == 0)
	    break;
    }
    return l;	// NULL means not found
}

remoteSubAgentList *
remoteSubAgentList::find(remoteSubAgent *r) {
    for (remoteSubAgentList *l = this->next; l != NULL; l = l->next) {
	if (*r == *(l->getRSA()))
	    break;	
    }
    return l;	
}
void
remoteSubAgentList::append (remoteSubAgent *rsa) {
    
    for ( remoteSubAgentList *l = this; l->next != NULL; l= l->next) {
	;
    }
    l->next = new remoteSubAgentList(rsa);
}

void
remoteSubAgentList::clear(int destroyRSAvbl) {
    while (next != 0) {
	remoteSubAgentList *rl = next;
	next = rl->next;
	rl->getRSA()->getVarBindList()->clear(destroyRSAvbl);
	delete rl;
    }
     
}
remoteSubAgent::remoteSubAgent(void) {
    port = 0;
    name = 0;
    timeout = 0;
    errstat = 0;
    bzero (&address, sizeof address);
}

remoteSubAgent::remoteSubAgent(char *saroot, char *saaddr, char *saport, 
				char * satimeout, char *saname) {
    root.setValue(saroot);
    setAddr(saaddr);
    setPort(atoi(saport));
    setTimeout(atoi(satimeout));
    errstat = 0;
    if (saname)
	name = strdup(saname);	
    else
	name = 0;
}

remoteSubAgent::remoteSubAgent(remoteSubAgent *r, int copyVb) {
    subID *sid;
    unsigned int len;
    r->root.getValue(&sid, &len);
    root.setValue(sid, len);
    address = r->address;
    port = r->port;
    errstat = r->errstat;
    timeout = r->timeout;
    if (r->name) 
	name = strdup(r->name);
    else
	name = 0;
    if (copyVb) {
	for (varBindList *l = r->vbl.getNext(); l != NULL; l = l->getNext()){
	    vbl.appendVar(l->getVar());
	}
    }
}
remoteSubAgent::~remoteSubAgent(void) {
    if (name)
	delete name;
    // my user must clear the varbind list 
    // vbl.clear(0);
}


int
remoteSubAgent::operator==(remoteSubAgent &r) {
    oid *ro = r.getRoot();
    unsigned int rlen, mylen;
    subID *rid,  *myid;
    ro->getValue(&rid, &rlen);
    root.getValue(&myid,  &mylen);
    struct in_addr *raddr = r.getAddr();
    unsigned long raddrl = raddr->s_addr;
    if ((rlen == mylen) && (bcmp (rid, myid, mylen *sizeof (subID)) == 0)
			&&
	((unsigned long)address.s_addr == raddrl)
			&&
	(port == r.getPort()))
	return 1;
    else {
	return 0;
    }
}
char *
remoteSubAgent::getString(void) {
    char buf[8192];
    char ibuf[16];
    char *s;
    strcpy(buf, s = root.getString());
    delete s;
    strcat(buf, ", ");
    strcat (buf, inet_ntoa(address));
    strcat(buf, ", ");
    int len = sprintf(ibuf, "%d", port);
    strcat(buf, ibuf);
    strcat(buf, ", ");
    len = sprintf(ibuf, "%d", timeout);
    strcat (buf, ibuf);
    if (name) {
	strcat(buf, ", ");
	strcat(buf, name);
    }
    strcat(buf, s=vbl.getString());
    delete s;
    return (strdup(buf));
    
}

// setAddr() allows specifying a remote subagent host id as either
// a host name or an IP address.

void
remoteSubAgent::setAddr(char *a) {
    if (atoi(a) >= 1 && atoi(a) <= 9) {
        if (!a || !inet_aton(a, &address))
            bzero(&address, sizeof address);
    }
    else {
	struct hostent *he = gethostbyname(a);
	if (he == 0)
            bzero(&address, sizeof address);
	else
            bcopy(he->h_addr, &address, sizeof address);
    }
}

