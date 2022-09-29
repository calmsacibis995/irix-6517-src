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
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/syslog.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/socket.h>
#include "oid.h"
#include "pdu.h"
#include "packet.h"
#include "message.h"
#include "traphandler.h"
#include "subagent.h"
#include "systemsa.h"
#include "sgisa.h"
#include "sat.h"
#include "agent.h"

extern "C" {
#include "exception.h"
};

snmpTrapHandler *traph;

extern snmpAgent agent;
extern systemSubAgent *syssa;
extern sgiHWSubAgent *sgihwsa;

static char *defaultTrapCommStr = {"traps"};

snmpTrapHandler::snmpTrapHandler(void) {
    traph = this;
    mh = new snmpMessageHandler();
    char hostname[MAXHOSTNAMELEN +1];
    struct hostent *he;
    (void) gethostname(hostname, MAXHOSTNAMELEN);
    if (he = gethostbyname(hostname)) {
	struct in_addr *iaddr;
	iaddr = (struct in_addr *)he->h_addr_list[0];
	myaddress = iaddr->s_addr;
    }
    strcpy (trapcomm, defaultTrapCommStr);
    sock = mh->open();
    if (!sock) {
	exc_errlog (LOG_ERR, errno, "trapHandler: Cannot open message handler");
	return;
    }
    
    readCfg();
}

void
snmpTrapHandler::readCfg(void) {
    FILE *sf;
    char buf[TRAP_FILE_MAX_LINE];
    if ((sf = fopen(SNMP_TRAP_CONF_FILE, "r")) == NULL) {
	exc_errlog(LOG_WARNING, errno,  
	    "trapHandler: Cannot open trap config file %s: ",
	    SNMP_TRAP_CONF_FILE);
	return;
    }
    exc_errlog(LOG_DEBUG, 0, "trapHandler: Opened trap file '%s'",
	    SNMP_TRAP_CONF_FILE);
	    
    int linecount = 0;

    // By default, if no entry exists in the config file, 
    // set agent to generate authenticationFailure traps.
    agent.setSnmpEnableAuthenTraps(ENABLE_AUTHEN_TRAPS);

    while ((fgets(buf, TRAP_FILE_MAX_LINE, sf)) != NULL) {
	// char *addr = NULL;
	int trap = MAX_TRAPS +1;
	char *trapstr = NULL;
	char *line = strdup(buf);
	linecount++;
	char *tok = strtok(line,  " \t\n");
	if(tok != NULL && tok[0] == '#') {
	    delete line;
	    continue;
	}
	
	if (tok) {
	    trapstr = strdup(tok);
	    if (isdigit(*trapstr)) {
		trap = atoi(trapstr);
		if (trap < SNMP_TRAP_COLD_START ||
		    trap >= MAX_TRAPS) {
		    exc_errlog (LOG_WARNING, 0, 
			"trapHandler: invalid trap number %d at line %d", 
			trap, linecount);
		    continue;
		}
	    }
	    else {
		if (strcasecmp(trapstr, "coldStart") == 0)
		    trap = SNMP_TRAP_COLD_START;
		else if (strcasecmp(trapstr, "warmStart") == 0)
		    trap = SNMP_TRAP_WARM_START;
		else if (strcasecmp(trapstr, "linkDown") == 0)
		    trap = SNMP_TRAP_LINK_DOWN;
		else if (strcasecmp(trapstr, "linkUp") == 0)
		    trap = SNMP_TRAP_LINK_UP;
		else if (strcasecmp(trapstr, "authenticationFailure") == 0)
		    trap = SNMP_TRAP_AUTH;
		else if (strcasecmp(trapstr, "egpNeighborLoss") == 0)
		    trap = SNMP_TRAP_EGP_LOSS;
		else if (strcasecmp(trapstr, "enterpriseSpecific") == 0)
		    trap = SNMP_TRAP_ENTERPRISE;
		else if (strcasecmp(trapstr, "community") == 0) {
		    tok = strtok(NULL, " \t\n");
		    if (tok) {
			if (strlen(tok) > MAX_COMM_LEN) {
			    exc_errlog(LOG_WARNING, 0,
				"trapHandler: community name too long: %s", 
				tok);
			}
			else {
			    strcpy (trapcomm, tok);
			    exc_errlog(LOG_DEBUG, 0, 
				"trapHandler: using community string %s", 
				trapcomm);
			}
		    }
		    delete line;
		    continue;
  		}
                else if (strcasecmp(trapstr, "enableAuthenTraps") == 0) {
                    tok = strtok(NULL, " \t\n");
                    if (tok) {
			if ( 0 == strcasecmp(tok, "yes")) {
			    agent.setSnmpEnableAuthenTraps(ENABLE_AUTHEN_TRAPS);
                            exc_errlog(LOG_DEBUG, 0,
                            "trapHandler: using enableAuthenTraps string %s",
                            tok);
			}
			else if ( 0 == strcasecmp(tok, "no")) {
			    agent.setSnmpEnableAuthenTraps(DISABLE_AUTHEN_TRAPS);
                            exc_errlog(LOG_DEBUG, 0,
                            "trapHandler: using enableAuthenTraps string %s",
                            tok);
			}
			else {
			    agent.setSnmpEnableAuthenTraps(ENABLE_AUTHEN_TRAPS);
                            exc_errlog(LOG_WARNING, 0,
                            "trapHandler: invalid enableAuthenTraps entry: %s. Assuming yes.",
                            tok);
                        }
                    }
                    delete line;
                    continue;
                }
  	    }
	    trapcb *tcb = &traps[trap];
	    while (tok = strtok(NULL, " ,\t\n")) {
		struct hostent *he;
		if ((he = gethostbyname(tok)) != NULL) {
		    tcb->appendDest(*(struct in_addr *)he->h_addr_list[0]);
		    // exc_errlog (LOG_DEBUG, 0, 
			// "trapHandler: added dest %s for trap %d", 
			// inet_ntoa(*(struct in_addr *)he->h_addr_list[0]), trap);
		    unique.appendUniqueDest(*(struct in_addr *)he->h_addr_list[0]);
		}
		else {
		    exc_errlog(LOG_WARNING, errno, 
			"trapHandler: cannot get host address for %s at line %d", 
			tok, linecount);
		}
	    }
	}
    }
    fclose(sf);	
}

int
snmpTrapHandler::send (unsigned int trap, varBindList *vbl) {
    trapPDU tpdu;
    tpdu.setTrapType(trap);
    tpdu.setSpecificTrap(0);
    tpdu.setAgentAddress((unsigned int)myaddress);
    short snmpTrapPort = 162;
    
    if (trap == SNMP_TRAP_AUTH &&
	agent.getSnmpEnableAuthenTraps() == DISABLE_AUTHEN_TRAPS)
	return 0;

    asnObjectIdentifier *aosysobjectid = sgihwsa->getSystemType();
    char *systypeoid =  aosysobjectid->getValue()->getString();
    tpdu.setEnterprise(systypeoid);
    if (trap == SNMP_TRAP_WARM_START || trap == SNMP_TRAP_COLD_START)
	tpdu.setTimeStamp(0);
    else {
	asnInteger *aotime;
	asnObjectIdentifier aoisysuptime = "1.3.6.1.2.1.1.3.0";
	int ttl = 1;
	int rc = syssa->get(&aoisysuptime, (asnObject **)&aotime, &ttl);
	if (rc != SNMP_ERR_noError) {
	    exc_errlog(LOG_ERR, 0, "trapHandler: cannot get sysuptime: %d", rc);
	    tpdu.setTimeStamp(0);
	}
	else {
	    int t = aotime->getValue();
	    tpdu.setTimeStamp(aotime->getValue());
	}
    }
    if (vbl) {
	for (varBindList *vl = vbl->getNext(); vl != 0; vl = vl->getNext()) {
	    tpdu.appendVar(vl->getVar());
	}
    }
    char *str;
    exc_errlog(LOG_DEBUG, 0, "trapHandler sending trap:\n%s",
		str= tpdu.getString());
    delete str;
    
    snmpPacket trappacket;
    trappacket.setCommunity(trapcomm);
    trappacket.setPDU(&tpdu);
    
    char *sendbuf = new char[8192];
    int plen = trappacket.encode(sendbuf, 8192);
    if (plen == 0) {
	exc_errlog(LOG_ERR, 0, "trapHandler: trap encode error");
	delete sendbuf;
	return -1;
    }
    else {
	struct sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = snmpTrapPort;
	for (trapDestList *tl = traps[trap].getDestList();
	     tl != 0;  tl = tl->getNext()) {
	    bcopy(tl->getAddr(), &sin.sin_addr, sizeof *tl->getAddr() );
	    if (sin.sin_addr.s_addr != 0)
	    	mh->send(&sin, &trappacket, plen);
	}
	agent.incSnmpOutTraps();
    }
    delete sendbuf;
    return 0;
}

trapcb::trapcb(void) {

}

trapcb::~trapcb(void) {
    
}



trapDestList::trapDestList(void) {
    dst.s_addr = 0;
    next = NULL;
}

trapDestList::trapDestList (struct in_addr a) {
    dst = a;
    next = NULL;
}

void
trapDestList::append(struct in_addr a) {
    for (trapDestList *tl = this; tl->next != NULL; tl = tl->next) {
	;
    }
    tl->next = new trapDestList (a);
}


// appendUnique() inserts only new unique trap destinations and inserts them
// in lexicographic order.

void
trapDestList::appendUnique(struct in_addr a) {
    static unsigned int uniqueNumTraps = 0;
    int trapDestFound;
    trapDestList *tdlp, *tmp_tdlp;
    unsigned long addr_in_list, new_addr, next_addr_in_list;

    trapDestFound = 0;
    new_addr = inet_addr(inet_ntoa(a));

    for (trapDestList  *tl = this; tl->next != NULL; tl = tl->next) {
	addr_in_list = inet_addr(inet_ntoa(*(tl->getAddr())));
	next_addr_in_list = inet_addr(inet_ntoa(*(tl->next->getAddr())));

        if (new_addr == next_addr_in_list) {
            trapDestFound = 1;
	    break;
        }

        if (new_addr > addr_in_list && new_addr < next_addr_in_list)
	    break;
    }

    if ( trapDestFound == 0 ) {
        // exc_errlog(LOG_DEBUG, 0,
                // "trapHandler: adding new unique trap dest entry: %s\n\n",
                // inet_ntoa(a));
	tdlp = new trapDestList(a);
	tmp_tdlp = tl->next;
	tl->next = tdlp;
	tdlp->next = tmp_tdlp;
        uniqueNumTraps++;
    }
}

void
snmpTrapHandler::dumpTrapDestLists() {
    unsigned int numTraps = 0;
    unsigned int numUniqueTraps = 0;

    for (int t = SNMP_TRAP_COLD_START; t < MAX_TRAPS; t++) {
	exc_errlog(LOG_DEBUG, 0, "trapHandler: trap %d", t);
    	for (trapDestList *tl = traps[t].getDestList();
			tl != NULL; tl = tl->getNext()) {
	    if ((tl->getAddr())->s_addr != 0) {
	        exc_errlog(LOG_DEBUG, 0, 
		    "   %s", inet_ntoa(*(tl->getAddr())));
		numTraps++;
	    }
	}
    }

    for (trapDestList  *tl = unique.getDestList(); tl != NULL; tl = tl->getNext()) {
        if ((tl->getAddr())->s_addr != 0) {
	    numUniqueTraps++;
	    exc_errlog(LOG_DEBUG, 0, 
	        "trapHandler: unique %s", inet_ntoa(*(tl->getAddr())));
	}
    }	
    exc_errlog(LOG_DEBUG, 0, "trapHandler: %d total trap entries found\n", 
	numTraps);

    exc_errlog(LOG_DEBUG, 0, "trapHandler: %d unique trap entries found\n", 
	numUniqueTraps);
    trapCount = numTraps;
    uniqueTrapCount = numUniqueTraps;
}

struct in_addr *
snmpTrapHandler::getFirstUniqueAddr() {
    // Note that the first trapcb struct in the snmpTrapHandler class
    // contains a dummy trapDestList class with an address of zero.
    trapDestList  *tl = (unique.getDestList())->getNext();
    return(tl->getAddr());
}	

struct in_addr *
snmpTrapHandler::getNextUniqueAddr(struct in_addr *dst) {

    for (trapDestList  *tl = (unique.getDestList())->getNext(); 
	 tl != NULL; tl = tl->getNext()) {

	if ((tl->getAddr())->s_addr == dst->s_addr) {
	    if (tl->getNext() != NULL)	// If more entries
	        return (struct in_addr *) ((tl->getNext())->getAddr());
	}
    }
    return NULL;
}

