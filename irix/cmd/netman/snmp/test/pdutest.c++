/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 *	PDU Test Program
 *
 *	$Revision: 1.2 $
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
#include "oid.h"
#include "asn1.h"
#include "snmp.h"
#include "pdu.h"
#include "packet.h"
#include "message.h"

const int buflen = 64;
static char buf[buflen];

void
printbuf(int end = buflen)
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

main()
{
    // Set up a GetRequest message
    snmpPacket inpkt;
    inpkt.setCommunity("public");
    getRequestPDU ingr;
    inpkt.setPDU(&ingr);
    ingr.setRequestId(1);
    ingr.setErrorStatus(0);
    ingr.setErrorIndex(0);
    varBind* inv1 = new varBind("1.3.6.1.2.1.1.1.0");
    ingr.appendVar(inv1);
    varBind* inv2 = new varBind("1.3.6.1.2.1.1.3.0");
    ingr.appendVar(inv2);

    // Print it out
    char* s = inpkt.getString();
    printf("Original Packet = %s\n", s);
    delete s;

    // Encode it
    int len = inpkt.encode(buf, buflen);
    if (len == 0) {
	printf("Error in encoding snmpPacket\n");
	exit(-1);
    }

    // Print out buffer
    printf("\nASN.1 Encoding:\n");
    printbuf(len);
    putchar('\n');

    // Open the message handler
    snmpMessageHandler mh;
    int skt = mh.open();
    if (skt < 0) {
	perror("Error opening message handler");
	exit(-1);
    }

    struct sockaddr_in addr;
    int namelen = sizeof addr;
    if (getsockname(skt, &addr, &namelen) < 0) {
	perror("Error getting socket name");
	exit(-1);
    }

    // Send the message to ourself
    int rc = mh.send(&addr, &inpkt, len);
    if (rc < 0) {
	perror("Error sending message");
	exit(-1);
    }
    if (rc != len) {
	printf("Error sending message:  Bad length returned\n");
	exit(-1);
    }

    // Receive the message
    snmpPacket outpkt;
    outpkt.setAsn(buf, buflen);
    rc = mh.recv(0, &outpkt, buflen);
    if (rc < 0) {
	perror("Error receiving message");
	exit(-1);
    }
    if (rc != len) {
	printf("Error receiving message:  Bad length returned\n");
	exit(-1);
    }

    // Close the message handler
    if (mh.close() < 0) {
	perror("Error closing message handler");
	exit(-1);
    }

    // Decode it
    len = outpkt.decode();
    if (len == 0) {
	printf("Error in decoding snmpPacket\n");
	exit(-1);
    }

    // Print it out
    s = outpkt.getString();
    printf("Decoded Packet = %s\n", s);
    delete s;
}
