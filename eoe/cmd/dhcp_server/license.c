/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	license.c
 *
 *	$Revision: 1.1 $
 *
 *      Programs linked with this module must also be linked
 *      with libnetls.a and libnck.a.
 *
 *      Compiling this file requires the "-dollar" flag and the flag
 *      -I/usr/include/idl/c to the C compiler.
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

#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <lmsgi.h>

#ifdef EDHCP
#define LICENSE_VERSION "1.000"
#endif

/*
 * Error messages from license routines
 */
static  char badInitMessage[] =
	"Error initializing the network license system";
static  char noLicenseMessage[] =
	"Could not find a valid EDHCP license";
static  char licenseWarningMessage[] =
	"The license for EDHCP expires";
static  char licenseLimitMessage[] =
	"EDHCP license limit reached";
static  char logOpenMessage[] =
	"Could not open license log file";
static  char logChmodMessage[] =
	"Could not change permissions of license log file";
static  char logLockMessage[] =
	"Could not lock license log file";
static  char logMagicMessage[] =
	"License log file has bad magic number";
static  char logReadMessage[] =
	"Could not read license log file";
static  char logWriteMessage[] =
	"Could not write license log file";
static  char logTruncMessage[] =
	"Could not truncate license log file";
static  char logUnlockMessage[] =
	"Could not unlock license log file";
static  char logRemoveMessage[] =
	"Could not find entry in license log file";

static char base_feature[] = "edhcp";

LM_CODE(code, ENCRYPTION_CODE_1, ENCRYPTION_CODE_2,
        VENDOR_KEY1, VENDOR_KEY2, VENDOR_KEY3, VENDOR_KEY4,
        VENDOR_KEY5);


/*
 * Release the license back
 */
static void
releaseLicense(void)
{
    int status;

    status = license_chk_in(base_feature, 0);
    return;
}
void
noFeatureCB()
{
  return;
}

/*
 * Get a license from FlexLM
 */
unsigned int
getLicense(char *prog, char **msg)
{
    time_t expDate;
    unsigned int remaining;
    unsigned int i;


    *msg = 0;
    if (license_init(&code, "sgifd", B_TRUE) < 0) {
	*msg = badInitMessage;
	return 0;
    }
    if (!isatty(1)) {
	if (license_set_attr(LMSGI_NO_SUCH_FEATURE, 
			     (LM_A_VAL_TYPE)noFeatureCB)){
	    printf("Set attribute error: %s\n", license_errstr());
	}
    }

    /* Get a license */
    if (license_chk_out(&code, 
                        base_feature,
                        LICENSE_VERSION)) {
        printf("error: %s\n", license_errstr());
	*msg = noLicenseMessage;
	return 0;
    }

    return 1;
}

