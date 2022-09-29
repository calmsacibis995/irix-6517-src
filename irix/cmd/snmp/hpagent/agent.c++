/*
 * Copyright 1991, 1992, 1993, 1994 Silicon Graphics, Inc.  All rights reserved.
 *
 *	SNMP Agent
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
#include <sys/time.h>
#include <sys/times.h>
#include <sys/syslog.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <bstring.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "oid.h"
#include "asn1.h"
#include "snmp.h"
#include "pdu.h"
#include "packet.h"
#include "message.h"
#include "subagent.h"

#include "systemsa.h"
#define MAXCOMMNAMELEN 256

#ifdef MASTER
#include "remotesubagent.h"
#endif

#include "traphandler.h"
extern snmpTrapHandler *traph;
extern systemSubAgent  *syssa; 

typedef struct {
    char hostname[MAXHOSTNAMELEN];
    in_addr addr;
    char community[MAXCOMMNAMELEN];
    int request;
    long timestamp;
} af_entry;

#define AF_MAXENTRIES	10
static af_entry af_table[AF_MAXENTRIES];	// Save up to 10 entries only
static int af_first = 0;	// index for first entry in circular buffer
static int af_next = -1;	// index for next entry in circular buffer
static int af_num = 0;		// Number of valid entries in circular buffer

#include "sat.h"
#include "agent.h"
#include "commauth.h"
extern "C" {
#include "exception.h"
};

int packetCounter = 0;

void
printbuf(char* buf, int end)
{
    printf("%04X(%04d): %02X", 0, 0, buf[0]);
    for (int i = 1; i < end; i++) {
	if (i % 16 == 0)
	    printf("\n%04X(%04d): %02X", i, i, buf[i]);
	else
	    printf(" %02X", buf[i]);
    }
    putchar('\n');
}

snmpAgent::snmpAgent(void)
{
    mh = new snmpMessageHandler;
    bzero(&stats, sizeof stats);
    // snmpEnableAuthenTraps is enabled by default, but overriden if an entry
    // exists in /etc/snmpd.trap.conf
    stats.snmpEnableAuthenTraps = ENABLE_AUTHEN_TRAPS;
    dumpPacket = dumpRequest = dumpResponse = foreground = 0;
}

void                    
snmpAgent::setService(char* s)
{ 
    service = new char[strlen(s) + 1];
    strcpy(service, s);
    mh->setSnmpMessageHandlerSvc(service);
    delete (service);
}

void                    
snmpAgent::setPort(int p)
{ 
    if (p != 0)
    {
    	port = p;
    	mh->setSnmpMessageHandlerPort(port);
    }
}

int
snmpAgent::import(asnObjectIdentifier *o, subAgent *sa)
{
    subAgentNode *san = sat.add(o, sa);
    return (san == 0) ? SNMP_ERR_noError : SNMP_ERR_genErr;
}

int
snmpAgent::unimport(asnObjectIdentifier *o)
{
    subAgentNode *san = sat.remove(o);
    return (san == 0) ? SNMP_ERR_noError : SNMP_ERR_genErr;
}

void
snmpAgent::run(void)
{
    const char *authfile = "/etc/snmpd.auth";
    const int maxPacketSize = 8192;
    char recvbuf[maxPacketSize];
    char sendbuf[maxPacketSize];

    // Open the message handler
    int sock = mh->agentOpen();
    if (sock < 0) {
	exc_errlog(LOG_ERR, errno, "agent: unable to create message handler");
	return;
    }

    struct timeval  boot;

    if (gettimeofday(&boot, 0) != 0) {
        exc_errlog(LOG_ERR, errno, "agent: cannot gettimeofday");
        timerclear(&boot);
    }
#ifdef MASTER
    else {		// Update startfile and send startup trap
        const char      *startfile = "/etc/snmpd.start";
        struct tms 	tm;
        struct stat 	statbuf;
    	int 		saveerrno = 0;

        long tickspersec = sysconf(_SC_CLK_TCK);
        clock_t uptime = times(&tm);
        time_t sysboottime = boot.tv_sec - uptime / tickspersec;
        int status = stat(startfile, &statbuf);
        if (status != 0)
            saveerrno = errno;
        if ( (status == 0 && (sysboottime > statbuf.st_mtime))
                || saveerrno == ENOENT)
                traph->send(SNMP_TRAP_COLD_START);
            else
                traph->send(SNMP_TRAP_WARM_START);
        if (saveerrno == ENOENT) {
            if (creat(startfile, S_IRUSR | S_IWUSR) == -1)
                exc_errlog(LOG_ERR, errno, "agent: cannot create %s",
                            startfile);
        }
        else {
            struct utimbuf tbuf;
            tbuf.actime = tbuf.modtime = boot.tv_sec;
            if (utime(startfile, &tbuf) == -1) {
                exc_errlog(LOG_ERR, errno, "agent: cannot utime %s",
                            startfile);

            }
        }
    }
#endif
    traph->dumpTrapDestLists();

    // Update boot time in the systemsa class to support sysUpTime
    syssa->setBoot(&boot);

    // Create a community-based authorization module
    communityAuth ca(authfile);

    // Set up the response packet
	
    getResponsePDU respdu;
    response.setPDU(&respdu);
    
    // setup to handle remote subagents
#ifdef MASTER
    rsa = new remoteSubAgentHandler();
#endif
    
 
    for ( ; ; ) {

	// Set up the receive buffer
	request.setAsn(recvbuf, maxPacketSize);

	// Receive a request
	int len = mh->recv(&addr, &request, maxPacketSize);
	if (len < 0) {
	    exc_errlog(LOG_WARNING, errno, "agent: unable to receive message");
	    clean();
	    continue;
	}
	stats.snmpInPkts++;
	exc_errlog(LOG_INFO, 0, "agent: received packet from %s\n",
				inet_ntoa(addr.sin_addr));
	if (dumpPacket) {
	    printf("\nReceived packet:\n");
	    printbuf(request.getAsn(), len);
	    putchar('\n');
	}

	if (request.decode(recvbuf, len) == 0) {
	    stats.snmpInASNParseErrs++;
	    exc_errlog(LOG_NOTICE, 0, "agent: unable to decode request");
	    clean();
	    continue;
	}
	if (dumpRequest) {
	    char *s = request.getString();
	    printf("Decoded Request = %s\n", s);
	    delete s;
	}

	// Increment packet counter
	packetCounter++;

	// Get the PDU
	snmpPDU *reqpdu = (snmpPDU *) request.getPDU();	// XXX - dangerous?
	// Ignore any incoming error status and continue to process packet
	int errstat = reqpdu->getErrorStatus();
	switch (errstat) {
	    case SNMP_ERR_tooBig:
		stats.snmpInTooBigs++;
		break;
	    case SNMP_ERR_noSuchName:
		stats.snmpInNoSuchNames++;
		break;
	    case SNMP_ERR_badValue:
		stats.snmpInBadValues++;
		break;
	    case SNMP_ERR_readOnly:
		stats.snmpInReadOnlys++;
		break;
	    case SNMP_ERR_genErr:
		stats.snmpInGenErrs++;
		break;
	}

	// Find the request type
	errstat = SNMP_ERR_noError;
	unsigned int varCount = 0;
	unsigned int type = reqpdu->getType();
	unsigned int *count;
	switch (type) {
	    case SNMP_GetRequest:
		stats.snmpInGetRequests++;
		count = &stats.snmpInTotalReqVars;
		exc_errlog(LOG_DEBUG, 0, "agent: packet is a getRequest");
		break;
	    case SNMP_GetNextRequest:
		stats.snmpInGetNexts++;
		count = &stats.snmpInTotalReqVars;
		exc_errlog(LOG_DEBUG, 0, "agent: packet is a getNextRequest");
		break;
	    case SNMP_SetRequest:
		stats.snmpInSetRequests++;
		count = &stats.snmpInTotalSetVars;
		exc_errlog(LOG_DEBUG, 0, "agent: packet is a setRequest");
		break;
	}

	// Authenticate the request
	int score = ca.authorize(&(addr.sin_addr),
				 request.getCommunity(), type);
	if (score <= 0) {
	    if (score == 0)
		stats.snmpInBadCommunityNames++;
	    else
		stats.snmpInBadCommunityUses++;

	    char comm[MAXCOMMNAMELEN];
	    asnOctetString *o = request.getCommunity();
	    if (o->getLength() > 255)
		comm[0] = '\0';
	    else {
		bcopy(o->getValue(), comm, o->getLength());
		comm[o->getLength()] = '\0';
	    }

	    exc_errlog(LOG_NOTICE, 0, "agent: unauthorized access: (%s, %s, %s)",
				       inet_ntoa(addr.sin_addr), comm,
				       type == SNMP_SetRequest ? "set" : "get");

	    // Save entry in a circular buffer which is then saved to a file,
	    // so that the hp-ux remote subagent can read it ad report as part
	    // of its authfail MIB group. Note that there can be only one 
	    // entry per manager since the MIB table is indexed by IP address.
	    
	    // Verify if there is already one entry in the table with this
	    // IP address.

	    for (int i = 0; i < AF_MAXENTRIES; i++) {
     		if ( 0 == bcmp(&addr.sin_addr, &(af_table[i].addr), 
			sizeof(in_addr)))
		    break;			// Address found in table
	    }
	    if ( i == AF_MAXENTRIES) { 		// Not in table
		// Get position where entry should be added in circular buffer
	        if (af_num < AF_MAXENTRIES) {	// Buffer is not full yet
		    af_num++;	
	        }
	        else {	// Start going in circle
	            af_first = (af_first + 1) % AF_MAXENTRIES;
	        }
	        af_next  = (af_next  + 1) % AF_MAXENTRIES;
	    }
	    else
		af_next = i;
    
	    hostent *hostent_p = gethostbyaddr(&(addr.sin_addr), 
	     sizeof(in_addr), AF_INET);
	    strcpy(af_table[af_next].hostname, hostent_p->h_name);
    
	    bcopy(&(addr.sin_addr), &(af_table[af_next].addr), sizeof(in_addr));
	    strcpy(af_table[af_next].community, comm);
	    af_table[af_next].request = type;
    
	    struct timeval  now;
	    int rc;
	    if (rc = gettimeofday(&now, 0) < 0) {
	        af_table[af_next].timestamp = 0;
	    }
	    else
	        af_table[af_next].timestamp = now.tv_sec;

	    FILE *fp = fopen (AUTH_FAIL_DATAFILE, "w");
    	    if (fp == 0) {
	        exc_errlog(LOG_ERR, errno, "agent: unable to write to file %s.",
		    AUTH_FAIL_DATAFILE);
	    }
	    else {
	        int num = 1;
	        for (int i = af_first; 
		     num++ <= af_num; 
		     i = (i + 1) % AF_MAXENTRIES ) {

		    // Save record as: hostname ip_address community 
		    // SNMP_operation timeticks ASCII_date_time
		    // where SNMP_operation is get or set.
		    // The MIB uses only ip_address community timeticks

    	            fprintf(fp, "%s %s %s %s %d %s", 
		        af_table[i].hostname,
		        inet_ntoa(af_table[i].addr),
		        af_table[i].community,
		        af_table[i].request == SNMP_SetRequest ? "set" : "get",
		        af_table[i].timestamp,
		        ctime ((time_t *)&(af_table[i].timestamp)));
		}
    	        fclose(fp);
	    }
	    
	    // Only the master agent sends authentication trap.
	    // However, if a remote subagent is used as a master,
	    // it should then send authentication failure traps
	    traph->send(SNMP_TRAP_AUTH);
	    
	    clean();
	    continue;
	}	// End of processing invalid community

	// For each variable in the varBindList, ask the sub-agent
	// serving that variable for the value.
	for (varBindList *vbl = reqpdu->getVarList();
				vbl->getNext() != 0; varCount++) {
	    // Move variable from request to response
	    varBind *vb = vbl->getNext()->getVar();
	    if (vb == 0 || vb->getObject() == 0 || vb->getObjectValue() == 0)
		break;
	    vbl->removeVar(vb, 0);
	    respdu.getVarList()->appendVar(vb);

	    // Don't perform request if an error occurred already
	    if (errstat != SNMP_ERR_noError)
		continue;
		
 	    subAgentNode *san;
	    asnObject *value = 0;
  	    int ttl = 0;		// XXX - unused right now

	    // Switch on SNMP PDU and perform request
	    //
	    // see if this varbind belongs to a remote subagent
	    //
#ifdef MASTER
	    if (!rsa->checkVb(vb))
#endif
		{
		switch (type) {
		    case SNMP_GetRequest:
			san = sat.lookup(vb->getObject());
			if (san == 0)
			    errstat = SNMP_ERR_noSuchName;
			else
			    errstat = (san->getSubAgent())->
					get(vb->getObject(), &value, &ttl);
			break;
	
		    case SNMP_GetNextRequest:
			{
			    // Keep a copy of the oid in case the end of the MIB
			    unsigned int len;
			    subID *subid, tmp[OID_MAX_LENGTH];
			    oid *oi = vb->getObject()->getValue();
			    oi->getValue(&subid, &len);
			    bcopy(subid, tmp, len * sizeof(subID));
	
			    for ( ; ; ) {
				san = sat.lookupNext(vb->getObject());
				if (san == 0) {
				    errstat = SNMP_ERR_noSuchName;
				    oi->setValue(tmp, len);
				} else {
				    errstat = (san->getSubAgent())->
						getNext(vb->getObject(), &value, &ttl);
				    if (errstat == SNMP_ERR_noSuchName)
					continue;
				}
				break;
			    }
			}
			break;
	
		    case SNMP_SetRequest:
			san = sat.lookup(vb->getObject());
			if (san == 0)
			    errstat = SNMP_ERR_noSuchName;
			else {
			    asnObject *a = vb->getObjectValue();
			    errstat = (san->getSubAgent())->
					set(vb->getObject(), &a, &ttl);
			}
			break;
	
		    default:
			exc_errlog(LOG_WARNING, 0, 
				"agent: unsupported PDU: %d", type);
			clean();
			continue;
		}
		if (errstat == SNMP_ERR_noError) {
		    if (value != 0)
			vb->setObjectValue(value, 1);
		} else {
		    respdu.setErrorStatus(errstat);
		    respdu.setErrorIndex(varCount + 1);
		}
	    }
	}
#ifdef MASTER
	int rttl = 0;	//XXX unused for now
	errstat = rsa->doRequests(request, respdu, errstat, &varCount,  &rttl);
#endif
	// Finish up response
	*count += varCount;
	response.setCommunity(request.getCommunity());
	respdu.setRequestId(reqpdu->getRequestId());
	switch (errstat) {
	    case SNMP_ERR_noError:
		respdu.setErrorStatus(errstat);
		respdu.setErrorIndex(0);
		break;
	    case SNMP_ERR_tooBig:
		stats.snmpInTooBigs++;
		break;
	    case SNMP_ERR_noSuchName:
		stats.snmpInNoSuchNames++;
		break;
	    case SNMP_ERR_badValue:
		stats.snmpInBadValues++;
		break;
	    case SNMP_ERR_readOnly:
		stats.snmpInReadOnlys++;
		break;
	    case SNMP_ERR_genErr:
		stats.snmpInGenErrs++;
		break;
	}
	if (dumpResponse) {
	    char *s = response.getString();
	    printf("Response = %s\n", s);
	    delete s;
	}

	// Encode response
	len = response.encode(sendbuf, maxPacketSize);
	if (len == 0) {
	    exc_errlog(LOG_WARNING, 0, "agent: unable to encode response");
	    clean();
	    continue;
	}
	if (dumpPacket) {
	    printf("\nEncoded response:\n");
	    printbuf(response.getAsn(), len);
	    putchar('\n');
	}

	int sl = mh->send(&addr, &response, len);
	if (sl != len) {
	    if (sl <= 0)
		exc_errlog(LOG_WARNING, errno, "agent: unable to send");
	    else
		exc_errlog(LOG_WARNING, 0, 
			"agent: bad length (%d vs. %d) returned from send", 
			sl, len);
	    clean();
	    continue;
	}
	stats.snmpOutPkts++;
	stats.snmpOutGetResponses++;
	exc_errlog(LOG_DEBUG, 0, "agent: successful response");

	// Clean up
	clean();
    }
}

void
snmpAgent::clean(void)
{
    request.setPDU(0);
    snmpPDU *p = (snmpPDU *) response.getPDU();
    p->getVarList()->clear();
#ifdef MASTER
    rsa->clean();
#endif
}
