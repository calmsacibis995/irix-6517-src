/*
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * 
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 * 
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 * 
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 *
 *      Programs linked with this module must also be linked
 *      with liblmsgi ... this is hidden in the linking of libpcp_util.a
 *
 *      Compiling this file requires the "-dollar" flag and the flag
 *      -I/usr/include/idl/c to the C compiler.
 */

#ident "$Id: lmdebug.c,v 1.1 1997/04/18 01:39:13 markgw Exp $"

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <lmsgi.h>

LM_CODE(code, ENCRYPTION_CODE_1, ENCRYPTION_CODE_2,
	VENDOR_KEY1, VENDOR_KEY2, VENDOR_KEY3, VENDOR_KEY4, VENDOR_KEY5);

void
main(int argc, char **argv)
{
    int			sts;
    char		*name;
    char		*version;

    fprintf(stderr, "CODE1=0x%x CODE2=0x%x\n", ENCRYPTION_CODE_1, ENCRYPTION_CODE_2);
    fprintf(stderr, "KEY1=0x%x KEY2=0x%x KEY3=0x%x KEY4=0x%x KEY5=0x%x\n",
	VENDOR_KEY1, VENDOR_KEY2, VENDOR_KEY3, VENDOR_KEY4, VENDOR_KEY5);
    fprintf(stderr, "code=0x%x sizeof(code)=%d\n", code, sizeof(code));

    sts = license_init(&code, "sgifd", B_TRUE);
    fprintf(stderr, "license_init() -> %d\n", sts);

    if (sts >= 0) {
	name = "pcpmon";
	version = "1.0";
	sts = license_chk_out(&code, name, version);
	fprintf(stderr, "license_chk_out(&code, \"%s\", \"%s\") -> %d\n",
	    name, version, sts);
	if (sts == 0) {
	    sts = license_expdate(name);
	    fprintf(stderr, "license_expdate(\"%s\") -> %d\n", name, sts);
	}
    }
}
