/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 *	ASN.1 Test Program
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

#include <stdio.h>
#include <stdlib.h>
#include "asn1.h"

const int buflen = 24;
static char buf[buflen];
static char res[buflen];

void
printbuf(int end = buflen)
{
    for (int i = 0; i < end; i++)
	printf(" %02X", buf[i]);
    putchar('\n');
}


main(int argc, char** argv)
{
    if (argc < 2) {
	printf("usage:  lentest length\n");
	exit(-1);
    }

    int len = atoi(argv[1]);
    asnObject a;
    char* p = buf;
    a.setAsn(buf, buflen);
    int llen = a.encodeLength(&p, len);
    if (llen == 0) {
	printf("Error in encoding length\n");
	exit(-1);
    }
    printbuf(llen);
}
