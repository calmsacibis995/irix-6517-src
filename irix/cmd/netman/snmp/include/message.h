#ifndef __MESSAGE_H__
#define __MESSAGE_H__
/*
 * Copyright 1991, 1994 Silicon Graphics, Inc.  All rights reserved.
 *
 *	SNMP Message Handler
 *
 *	$Revision: 1.5 $
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

#include <packet.h>

#define DEFAULT_SNMP_PORT	161
#define DEFAULT_TRAP_PORT	162


/*
 * A handler that sends and receives SNMP packets
 */
class snmpMessageHandler {
public:
	snmpMessageHandler(void);
	void setSnmpMessageHandlerSvc(char*);
	void setSnmpMessageHandlerPort(int);

	// open() returns a socket that will be used to send and
	// receive on.  It may be used with a select() call.
	// agentOpen() is used by the SNMP agent to open the SNMP port.
	int open(void);
	int open(const struct sockaddr_in *a);
	int agentOpen(void);
	int close(void);

	// send() will send the packet pointed to by p of length l to the
	// destination given by the host name d or the sockaddr_in pointed
	// to by a.  Will return the number of bytes sent.
	int send(const struct sockaddr_in *a, snmpPacket *p,
		 unsigned int l);
	int sendPacket(const char *d, snmpPacket *p, unsigned int l);
	int sendPacket(const struct in_addr *a, snmpPacket *p,
		       unsigned int l);
	int sendTrap(const char *d, snmpPacket *p, unsigned int l);
	int sendTrap(const struct in_addr *a, snmpPacket *p,
		     unsigned int l);

	// recv() will receive a message of length l and place it in the ASN
	// buffer of the packet pointed to by p.  If p is 0, a packet will be
	// created that is not deleted on destruction of the class instance.
	// If a is not 0, *a will be filled with the sending address.
	int recv(struct sockaddr_in *a, snmpPacket *p, unsigned int l);
	
	// set the destination port for the request. Used to by agent to 
	// make requests of remote sub agents.
	
	inline void setDestPort(unsigned int port);
private:
	int sock;
	int snmpPort;
	int trapPort;
};

// inline functions
void
snmpMessageHandler::setDestPort (unsigned int port) {
    snmpPort = port;
}
#endif /* !__MESSAGE_H__ */
