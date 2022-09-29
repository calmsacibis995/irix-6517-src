/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 *	SNMP Test program
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <ctype.h>
#include "oid.h"
#include "asn1.h"
#include "snmp.h"
#include "pdu.h"
#include "packet.h"
#include "message.h"

extern "C" {
int gethostname(char *, int);
}

const int buflen = 4096;
static char buf[buflen];

void
printbuf(char* buf, int end = buflen)
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

main(int argc, char** argv)
{
    char hostname[64], *dst;
    gethostname(hostname, 63);

    // printf("usage: msgtest [ hostname ] [ community ] [ oid ]\n");

    if (argc < 2)
	dst = hostname;
    else
	dst = argv[1];

    // Open the message handler
    snmpMessageHandler mh;
    int skt = mh.open();
    if (skt < 0) {
	perror("Error opening message handler");
	exit(-1);
    }

    // Set up request and response packets
    snmpPacket request;
    getNextRequestPDU reqpdu;
    request.setPDU(&reqpdu);
    snmpPacket response;
    snmpPDU *respdu;
    unsigned int requestId = 1;
    varBindList *vl;
    varBind *v;
    asnObjectIdentifier o;
    char *s;

    // Put in the community name
    if (argc < 3)
	request.setCommunity("public");
    else
	request.setCommunity(argv[2]);

    if (argc < 4)
	o = "1.3";
    else
	o = argv[3];
    reqpdu.appendVar(new varBind(&o, 0));

    for ( ; ; ) {

	// Initialize the request
	reqpdu.setRequestId(requestId++);
	reqpdu.setErrorStatus(0);
	reqpdu.setErrorIndex(0);

	// Encode the request
	int len = request.encode(buf, buflen);
	if (len == 0) {
	    printf("Error in encoding request\n");
	    exit(-1);
	}

	// Send the message
	int rc = mh.sendPacket(dst, &request, len);
	if (rc < 0) {
	    perror("Error sending message");
	    exit(-1);
	}
	if (rc != len) {
	    printf("Error sending message:  Bad length returned\n");
	    exit(-1);
	}

	// Receive the response
	response.setAsn(0, 0);
	rc = mh.recv(0, &response, buflen);
	if (rc < 0) {
	    perror("Error receiving message");
	    exit(-1);
	}

	// Decode the response
	len = response.decode();
	if (len == 0) {
	    printf("Error in decoding response\n");
	    exit(-1);
	}

	// Check error status
	respdu = (snmpPDU *) response.getPDU();
	if (respdu->getErrorStatus() != SNMP_ERR_noError)
	    break;

	// Print the variable and reuse it in next request
	vl = respdu->getVarList();
	v = vl->getNext()->getVar();
	vl->removeVar(v, 0);
	s = v->getString();
	puts(s);
	delete s;
	vl = reqpdu.getVarList();
	vl->clear();
	vl->appendVar(v);

	// Clean up
	s = response.getAsn();
	delete s;
	response.setAsn(0, 0);
    }

    // Close the message handler
    if (mh.close() < 0) {
	perror("Error closing message handler");
	exit(-1);
    }
}
