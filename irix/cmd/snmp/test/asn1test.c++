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
#include <string.h>
#include <ctype.h>
#include "oid.h"
#include "asn1.h"

#define BUFLEN	24

static char buf[BUFLEN];
static char res[BUFLEN];

void
printbuf(void)
{
    for (int i = 0; i < BUFLEN; i++)
	printf(" %02X", buf[i]);
    putchar('\n');
}


main(int argc, char** argv)
{
    int op;

    if (argc < 2)
	op = TAG_NULL;
    else {
	op = TAG_INTEGER;
	for (char* s = argv[1]; *s != 0; s++) {
	    if (!isdigit(*s)) {
		if (*s == '.')
		    op = TAG_OBJECT_IDENTIFIER;
		else
		    op = TAG_OCTET_STRING;
	    }
	}
    }

    switch (op) {
	case TAG_OCTET_STRING:
	    {
		printf("Test Value: string = \"%s\"\n", argv[1]);
		asnOctetString in(argv[1]);
		if (in.encode(buf, BUFLEN) == 0) {
		    printf("Error in encoding string.\n");
		    exit(-1);
		}
		printbuf();
		asnOctetString out;
		out.setValue(res);
		if (out.decode(buf, BUFLEN) == 0) {
		    printf("Error in decoding string.\n");
		    exit(-1);
		}
		char* s = out.getString();
		printf("Result:     string = \"%s\"\n", s);
		delete s;
	    }
	    break;

	case TAG_INTEGER:
	    {
		printf("Test Value: integer = %d\n", atoi(argv[1]));
		asnInteger in(atoi(argv[1]));
		if (in.encode(buf, BUFLEN) == 0) {
		    printf("Error in encoding integer.\n");
		    exit(-1);
		}
		printbuf();
		asnInteger out;
		if (out.decode(buf, BUFLEN) == 0) {
		    printf("Error in decoding integer.\n");
		    exit(-1);
		}
		char* s = out.getString();
		printf("Result:     integer = %s\n", s);
		delete s;
	    }
	    break;

	case TAG_NULL:
	    {
		printf("Test Value: null\n");
		asnNull in;
		if (in.encode(buf, BUFLEN) == 0) {
		    printf("Error in encoding null.\n");
		    exit(-1);
		}
		printbuf();
		asnNull out;
		if (out.decode(buf, BUFLEN) == 0) {
		    printf("Error in decoding null.\n");
		    exit(-1);
		}
		char* s = out.getString();
		printf("Result:     %s\n", s);
		delete s;
	    }
	    break;

	case TAG_OBJECT_IDENTIFIER:
	    {
		printf("Test Value: oid = %s\n", argv[1]);
		oid inobj(argv[1]);
		asnObjectIdentifier in(&inobj);
		if (in.encode(buf, BUFLEN) == 0) {
		    printf("Error in encoding string.\n");
		    exit(-1);
		}
		printbuf();
		oid outobj;
		asnObjectIdentifier out(&outobj);
		if (out.decode(buf, BUFLEN) == 0) {
		    printf("Error in decoding string.\n");
		    exit(-1);
		}
		char* s = out.getString();
		printf("Result:     oid = %s\n", s);
		delete s;
	    }
	    break;

	default:
	    printf("type unknown\n");
	    exit(-1);
    }
}
