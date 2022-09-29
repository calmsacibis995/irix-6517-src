/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 *	SNMP Message Handler
 *
 *	$Revision: 1.4 $
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <libc.h>
#include "asn1.h"
#include "snmp.h"
#include "pdu.h"
#include "packet.h"
#include "message.h"
extern "C" {
#include "exception.h"
};


snmpMessageHandler::snmpMessageHandler(void)
{
    struct servent *se;

    se = getservbyname("snmp", "udp");
    if (se != 0)
	snmpPort = se->s_port;
    else
	snmpPort = DEFAULT_SNMP_PORT;

    se = getservbyname("snmp-trap", "udp");
    if (se != 0)
	trapPort = se->s_port;
    else
	trapPort = DEFAULT_TRAP_PORT;
}

void
snmpMessageHandler::setSnmpMessageHandlerSvc(char *s)
{
    struct servent *se;

    se = getservbyname(s, "udp");
    if (se != 0) {
	snmpPort = se->s_port;
        exc_errlog(LOG_INFO, 0,
            "agent: setSnmpMessageHandlerSvc: Using service %s on port %d\n",
            s, snmpPort);
    }
    else {
	snmpPort = DEFAULT_SNMP_PORT;
        exc_errlog(LOG_ERR, 0,
            "agent: setSnmpMessageHandlerSvc: Cannot find service %s. Using port %d instead\n",
            s, snmpPort);
    }
}

void
snmpMessageHandler::setSnmpMessageHandlerPort(int p)
{
    snmpPort = p;
    exc_errlog(LOG_INFO, 0,
        "agent: setSnmpMessageHandlerPort: Using SNMP port %d\n",
        snmpPort);
}

int
snmpMessageHandler::open(void)
{
    struct sockaddr_in sin;
    bzero(&sin, sizeof sin);
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = 0;
    return open(&sin);
}

int
snmpMessageHandler::open(const struct sockaddr_in *a)
{
    /* Open and bind a socket */
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock >= 0) {
	if (bind(sock, (struct sockaddr *) a, sizeof *a) != 0) {
	    ::close(sock);
	    sock = -1;
	}
    }
    return sock;
}

int
snmpMessageHandler::agentOpen(void)
{
    struct sockaddr_in sin;
    bzero(&sin, sizeof sin);
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = snmpPort;
    return open(&sin);
}

int
snmpMessageHandler::close(void)
{
    return ::close(sock);
}

int
snmpMessageHandler::send(const struct sockaddr_in *a, snmpPacket *p,
			 unsigned int l)
{
    return sendto(sock, p->asn, l, 0, a, sizeof *a);
}

int
snmpMessageHandler::sendPacket(const char *d, snmpPacket *p,
				unsigned int l)
{
    struct hostent *he = gethostbyname(d);
    if (he == 0)
	return -1;

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    bcopy(he->h_addr, &sin.sin_addr, sizeof sin.sin_addr);
    sin.sin_port = snmpPort;
    return send(&sin, p, l);
}

int
snmpMessageHandler::sendPacket(const struct in_addr *a, snmpPacket *p,
			       unsigned int l)
{
    struct sockaddr_in sin;

    sin.sin_family = AF_INET;
    sin.sin_addr = *a;
    sin.sin_port = snmpPort;
    return send(&sin, p, l);
}

int
snmpMessageHandler::sendTrap(const char *d, snmpPacket *p, unsigned int l)
{
    struct hostent *he = gethostbyname(d);
    if (he == 0)
	return -1;

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    bcopy(he->h_addr, &sin.sin_addr, sizeof sin.sin_addr);
    sin.sin_port = trapPort;
    return send(&sin, p, l);
}

int
snmpMessageHandler::sendTrap(const struct in_addr *a, snmpPacket *p,
			     unsigned int l)
{
    struct sockaddr_in sin;

    sin.sin_family = AF_INET;
    sin.sin_addr = *a;
    sin.sin_port = trapPort;
    return send(&sin, p, l);
}

int
snmpMessageHandler::recv(struct sockaddr_in *a, snmpPacket *p, unsigned int l)
{
    if (p == 0)
	p = new snmpPacket;

    if (p->getAsn() == 0)
	p->setAsn(new char[l], l);

    int fromlen = sizeof *a;
    return recvfrom(sock, p->getAsn(), l, 0, a, &fromlen);
}
