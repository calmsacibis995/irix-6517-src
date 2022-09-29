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
char *ether_aton(char *);
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

    // printf("usage: msgtest [ hostname ] [ community ]\n");

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
    snmpPacket response;
    snmpPDU *pdu;
    unsigned int requestId = 1;

    // Put in the community name
    if (argc < 3)
	request.setCommunity("public");
    else
	request.setCommunity(argv[2]);

    char cmd = '\0';
    for ( ; ; ) {
	pdu = 0;

	// Find which type of request
	char type[256];
	printf("Enter request  : ");
	if (gets(type) != NULL && strlen(type) != 0)
	    cmd = type[0];
	switch (cmd) {
	    case 'G':
		cmd = 'g';
	    case 'g':
		pdu = new getRequestPDU;
		break;
	    case 'N':
		cmd = 'n';
	    case 'n':
		pdu = new getNextRequestPDU;
		break;
	    case 'S':
		cmd = 's';
	    case 's':
		pdu = new setRequestPDU;
		break;
	    case 'Q':
	    case 'q':
		break;
	    default:
		printf("Valid request are (g)et, (n)ext, (s)et, or (q)uit.\n");
		continue;
	}

	if (pdu == 0)
	    break;

	// Set up a request
	request.setPDU(pdu);

	// Initialize the request
	pdu->setRequestId(requestId++);
	pdu->setErrorStatus(0);
	pdu->setErrorIndex(0);

	// Let the user enter a variable and add it to the list.  An
	// empty string ends the list.
	int varCount = 0;
	for ( ; ; ) {
	    char var[256];
	    asnObject *value = 0;
	    printf("Enter variable : ");
	    if (gets(var) == NULL || strlen(var) == 0)
		break;
	    if (cmd == 's') {
		char val[256];
		printf("Enter value    : ");
		if (gets(val) != NULL && strlen(val) != 0) {
		    for (char* s = val; *s != 0; s++) {
			if (!isdigit(*s)) {
			    if (*s != '.') {
				if (*s != ':')
				    value = new asnOctetString(val);
				else
				    value = new asnOctetString(ether_aton(val),6);
			    } else if (s == val)
				value = new asnObjectIdentifier(val + 1);
			    else
				value = new snmpIPaddress(val);
			    break;
			}
		    }
		    if (value == 0)
			value = new asnInteger(atoi(val));
		}
	    }
	    varBind* v = new varBind(var, value);
	    pdu->appendVar(v);
	    varCount++;
	}

	// If no variables, quit
	if (varCount == 0)
	    break;

	// Print out the request packet
	char* s = request.getString();
	printf("Original Packet = %s\n", s);
	delete s;

	// Encode the request
	int len = request.encode(buf, buflen);
	if (len == 0) {
	    printf("Error in encoding request\n");
	    exit(-1);
	}

	// Print encoded packet
	printf("\nRequest ASN.1 Encoding:\n");
	printbuf(buf, len);
	putchar('\n');

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

	// Print the encoded response
	printf("\nResponse ASN.1 Encoding:\n");
	printbuf(response.getAsn(), rc);
	putchar('\n');

	// Decode the response
	len = response.decode();
	if (len == 0) {
	    printf("Error in decoding response\n");
	    exit(-1);
	}

	// Print out the decoded response
	s = response.getString();
	printf("Decoded Packet = %s\n", s);
	delete s;

	// Clean up
	s = response.getAsn();
	delete s;
	response.setAsn(0, 0);
	delete pdu;
    }

    // Close the message handler
    if (mh.close() < 0) {
	perror("Error closing message handler");
	exit(-1);
    }
}
